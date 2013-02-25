#include "guest.h"
#include "vmexit.h"
#include "guest.h"

bool second_cpu_up;

void smp_init(void)
{
	second_cpu_up = true;

	dsb();
	dmb();
	isb();

	clean_cache(&second_cpu_up);
}

void smp_test(void)
{
	printf(".");
}

void smp_interrupt(void)
{
	printf("core 1 received interrupt\n");
}

void smp_gic_enable(void)
{
	writel(VGIC_CPU_BASE + GICC_CTLR, 0x1); /* enable cpu interface */

	writel(VGIC_CPU_BASE + GICC_PMR, 0xff);		/* unmask irq 0 */
	writel(VGIC_CPU_BASE + GICC_PMR, 0xff << 8);	/* unmask irq 1 */

	dsb();
	dmb();
	isb();

	printf("core 1 gic ready\n");
}
