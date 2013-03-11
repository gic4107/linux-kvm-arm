#include "guest.h"
#include "vmexit.h"
#include "guest.h"

__asm__(".arch_extension	virt");

volatile bool second_cpu_up;
volatile bool first_cpu_ack;

void smp_init(void)
{
	second_cpu_up = true;
}

void smp_test(void)
{
	printf(".");
}

void smp_interrupt(void)
{
	printf("core[1]: received interrupt\n");
}

void smp_gic_enable(int smp_cpus, int vgic_enabled)
{
	assert(smp_cpus >= 2);

	if (!vgic_enabled)
		return;

	writel(VGIC_CPU_BASE + GICC_CTLR, 0x1); /* enable cpu interface */

	writel(VGIC_CPU_BASE + GICC_PMR, 0xff);		/* unmask irq 0 */
	writel(VGIC_CPU_BASE + GICC_PMR, 0xff << 8);	/* unmask irq 1 */

	dsb();
	dmb();
	isb();

	debug("core[1]: gic ready\n");

	while (!first_cpu_ack);

	debug("core[1]: first cpu acked\n");
}
