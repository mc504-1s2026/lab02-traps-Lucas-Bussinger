#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>

extern int _hartid[];
void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entrando no modo supervisor\n");
	info("inicializando hart %d\n", _hartid[0]);
	info("configurando memoria virtual...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("habilitando timer...\n");
	timer_irq_enable();
	info("habilitando serial...\n");
	serial_init();
	serial_irq_enable();
	info("habilitando interrupcoes globais...\n");
	hart_irq_enable();

	serial_puts("> ");
	char cmd[256];
	size_t cmd_len = 0;

	while (1) {
		char read_buf[256];
		size_t n = serial_read(read_buf);
		for (size_t i = 0; i < n; i++) {
			char c = read_buf[i];
			if (c == '\r') {
				cmd[cmd_len] = '\0';
				serial_puts("\r\n");
				if (cmd_len > 0) {
					if (strcmp(cmd, "uptime") == 0) {
						u64 secs = timer_read() / TIMER_FREQ;
						char buf[32];
						snprintf(buf, sizeof(buf), "%lus\r\n", secs);
						serial_puts(buf);
					} else if (strncmp(cmd, "echo ", 5) == 0) {
						serial_puts(cmd + 5);
						serial_puts("\r\n");
					} else if (strcmp(cmd, "echo") == 0) {
						serial_puts("\r\n");
					} else if (strncmp(cmd, "alarm ", 6) == 0) {
						u64 secs = strtou64(cmd + 6, 10);
						timer_set_alarm(secs);
					} else {
						serial_puts("comando desconhecido\r\n");
					}
				}
				cmd_len = 0;
				serial_puts("> ");
			} else if (c == '\n') {
				// ignora
			} else if (c == '\x7f' || c == '\b') {
				if (cmd_len > 0) {
					cmd_len--;
					serial_puts("\b \b");
				}
			} else {
				if (cmd_len < sizeof(cmd) - 1) {
					cmd[cmd_len++] = c;
					serial_putc(c);
				}
			}
		}
		if (n == 0) {
			__asm__ __volatile__("wfi");
		}
	}

}

