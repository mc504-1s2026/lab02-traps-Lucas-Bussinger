#include <kernel/trap.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <arch/plic.h>
#include <kernel/serial.h>
#include <kernel/printf.h>

/* definido em src/trap_entry.S */
extern void trap_entry();

void handle_irq()
{
	u64 scause = csr_read(CSR_SCAUSE);
	if (scause == TRAP_TIMER_IRQ) {
		timer_irq();
	} else if (scause == TRAP_EXTERNAL_IRQ) {
		u32 irq = plic_hart_claim_irq(0);
		if (irq == IRQ_SERIAL) {
			serial_irq();
		}
		if (irq > 0) {
			plic_hart_complete_irq(0, irq);
		}
	} else {
		error("interrupcao nao tratada: 0x%x\n", scause);
		BUG();
	}
}

void handle_exception()
{
	u64 scause = csr_read(CSR_SCAUSE);
	u64 stval = csr_read(CSR_STVAL);
	u64 sepc = csr_read(CSR_SEPC);
	error("exceção nao tratada! scause: 0x%x, stval: 0x%x, sepc: 0x%x\n", scause, stval, sepc);
	BUG();
}

void trap_setup()
{
	csr_write(CSR_STVEC, trap_entry);
}

void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);
	if (scause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		handle_exception();
	}
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 flags = csr_read(CSR_SSTATUS);
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	return flags & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	if (flags) {
		csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
	} else {
		csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	}
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

