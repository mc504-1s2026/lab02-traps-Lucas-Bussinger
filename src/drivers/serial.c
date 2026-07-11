#include <kernel/serial.h>
#include <kernel/panic.h>
#include <arch/io.h>
#include <kernel/mm.h>
#include <arch/spinlock.h>
#include <arch/csr.h>
#include <arch/plic.h>

#define UART_REG(r) ((volatile u8 *)(0x10000000UL + KERNEL_DIRECT_MAP_START + (r)))

#define SERIAL_BUF_SIZE 1024
static struct {
	struct spinlock lock;
	char buf[SERIAL_BUF_SIZE];
	size_t r_idx;
	size_t w_idx;
} serial_dev;

void serial_init()
{
	spin_init(&serial_dev.lock);
	serial_dev.r_idx = 0;
	serial_dev.w_idx = 0;

	// Habilita as FIFOs e as limpa
	iowrite8(SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR, UART_REG(SERIAL_FCR));
}

void serial_irq_enable()
{
	/* Configura o PLIC para interrupções da serial (IRQ 10) */
	plic_irq_set_priority(IRQ_SERIAL, 1);
	plic_hart_enable_irq(0, IRQ_SERIAL);
	plic_hart_set_threshold(0, 0);

	/* Habilita a interrupção de recepção de dados na UART */
	iowrite8(SERIAL_IER_ERBFI, UART_REG(SERIAL_IER));

	/* Habilita interrupções externas no CSR sie */
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	/* Desabilita a interrupção de recepção de dados na UART */
	iowrite8(0, UART_REG(SERIAL_IER));

	/* Desabilita interrupções externas no CSR sie */
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	u64 flags = spin_lock_irqsave(&serial_dev.lock);

	while (ioread8(UART_REG(SERIAL_LSR)) & SERIAL_LSR_DTR) {
		char c = ioread8(UART_REG(SERIAL_RBR));

		size_t next_w = (serial_dev.w_idx + 1) % SERIAL_BUF_SIZE;
		if (next_w != serial_dev.r_idx) {
			serial_dev.buf[serial_dev.w_idx] = c;
			serial_dev.w_idx = next_w;
		}
	}

	spin_unlock_irqrestore(&serial_dev.lock, flags);
}

size_t serial_read(char *buf)
{
	u64 flags = spin_lock_irqsave(&serial_dev.lock);
	size_t count = 0;

	while (serial_dev.r_idx != serial_dev.w_idx) {
		buf[count++] = serial_dev.buf[serial_dev.r_idx];
		serial_dev.r_idx = (serial_dev.r_idx + 1) % SERIAL_BUF_SIZE;
	}

	spin_unlock_irqrestore(&serial_dev.lock, flags);
	return count;
}

void serial_putc(char c)
{
	while (!(ioread8(UART_REG(SERIAL_LSR)) & SERIAL_LSR_THRE)) {
	}
	iowrite8(c, UART_REG(SERIAL_THR));
}

void serial_puts(char *str)
{
	while (*str != '\0') {
		serial_putc(*str++);
	}
}

