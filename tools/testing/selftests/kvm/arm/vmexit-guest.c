#define DEBUG 1

#include "guest.h"
#include "guest-util.h"
#include "mmio_test.h"
#include "vmexit.h"

__asm__(".arch_extension	virt");

#define GOAL (1ULL << 26)

#define ARR_SIZE(_x) ((sizeof(_x) / sizeof(_x[0])))

static int nr_cpus;
static bool use_vgic;

static unsigned long vgic_base;
const int sgi_irq = 1; /* just use IRQ number 1 */


static unsigned long read_cc(void)
{
	unsigned long cc;
	asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
	return cc;
}

static void hvc_test(void)
{
	asm volatile("hvc #0");
}

static void vgic_init(void)
{
	if (vgic_base)
		return; /* already init'ed */
	vgic_base = VGIC_DIST_BASE;

	writel(vgic_base + GICD_CTLR, 0x1); /* enable distributor */
	writel(vgic_base + GICD_ISENABLE(sgi_irq), ISENABLE_IRQ(sgi_irq));

	dsb();
	dmb();
	isb();
}

static int mmio_vgic_init(void)
{
	if (!use_vgic) {
		/* Fake measure */
		vgic_base = VGIC_DIST_BASE;
		return 0;
	}

	vgic_init();
	return 0;
}

static void mmio_vgic_test(void)
{
	(void)readl(vgic_base);
}

static int ipi_init(void)
{
	unsigned counter = 1U << 28;

	if (!use_vgic || nr_cpus == 1)
		return -1;

	vgic_init();

	debug("core[0]: first cpu up\n");

	while (!second_cpu_up && counter--);

	if (!second_cpu_up)
		return -1;

	debug("core[0]: second cpu up\n");

	first_cpu_ack = true;

	return 0;
}

static void ipi_test(void)
{
	unsigned long val;

	/* Signal IPI/SGI IRQ to CPU 1 */
	val = SGIR_FORMAT(1, sgi_irq);

	writel(vgic_base + GICD_SGIR, val);
	dsb();
	dmb();
	isb();
}

static void mmio_fake_test(void)
{
	(void)readl(FAKE_MMIO);
}

static void noop_guest(void)
{
}

struct exit_test {
	char *name;
	void (*test_fn)(void);
	int (*init_fn)(void);
};

static void loop_test(struct exit_test *test)
{
	unsigned long i, iterations = 32;
	unsigned long c2, c1, cycles = 0;

	do {
		iterations *= 2;

		c1 = read_cc();
		for (i = 0; i < iterations; i++)
			test->test_fn();
		c2 = read_cc();

		if (c1 >= c2)
			continue;
		cycles = c2 - c1;
	} while (cycles < GOAL);

	debug("%s exit %u cycles over %u iterations = %u\n",
	       test->name, cycles, iterations, cycles / iterations);
	printf("%s\t%u\n",
	       test->name, cycles / iterations);
}

static struct exit_test available_tests[] = {
	{ "noop_guest",		noop_guest,		NULL		},
	{ "hvc",		hvc_test,		NULL		},
	{ "vgic_mmio",		mmio_vgic_test,		mmio_vgic_init	},
	{ "fake_mmio",		mmio_fake_test,		NULL		},
	{ "ipi",		ipi_test,		ipi_init	},
};

int test(int smp_cpus, int vgic_enabled)
{
	unsigned int i;
	struct exit_test *test;
	int ret;

	nr_cpus = smp_cpus;
	use_vgic = vgic_enabled;

	for (i = 0; i < ARR_SIZE(available_tests); i++) {
		test = &available_tests[i];
		ret = 0;
		if (test->init_fn) {
			ret = test->init_fn();
			if (ret)
				printf("skipping test: %s\n", test->name);
		}
		if (!ret)
			loop_test(test);
	}

	return 0;
}
