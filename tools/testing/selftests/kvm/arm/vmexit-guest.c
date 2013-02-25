#include "guest.h"
#include "guest-util.h"
#include "mmio_test.h"
#include "vmexit.h"

__asm__(".arch_extension	virt");

#define DEBUG 1

#define GOAL (1ULL << 28)

#define ARR_SIZE(_x) ((sizeof(_x) / sizeof(_x[0])))

static unsigned long vgic_base;
static const int sgi_irq = 1; /* just use IRQ number 0 */

#if 0
typedef unsigned long long u64;

static u64 pgd[4] __attribute__ ((aligned (32)));

#define PGD_SHIFT 30
#define PGD_SIZE (1 << PGD_SHIFT)
#define PGD_AF   (1 << 10) /* Don't raise access flag exceptions */
#define PGD_SH	 (3 << 8) /* All memory inner+outer shareable */

static void enable_mmu(void)
{
	unsigned long long i;

	/* Set up an identitify mapping */
	for (i = 0; i < 4; i++) {
		pgd[i] = (i * PGD_SIZE);
		pgd[i] |= PGD_AF | PGD_SH

	}
}
#endif

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
	vgic_init();
	return 0;
}

static void mmio_vgic_test(void)
{
	(void)readl(vgic_base);
}

static int ipi_init(void)
{
	vgic_init();
	//unsigned counter = 1U << 28;

	/* Give it a chance... */
	while (!second_cpu_up) {
		dsb();
		dmb();
		isb();
		clean_cache(&second_cpu_up);
	}

	if (!second_cpu_up) {
		printf("ipi_init: no secondary CPU\n");
		return -1;
	} else {
		printf("ipi_init: secondary CPU is up!\n");
	}

	return 0;
}

static void ipi_test(void)
{
	unsigned long val;

	/* Signal IRQ 0 to CPU 1 */
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

#if DEBUG
	printf("%s exit %u cycles over %u iterations = %u\n",
	       test->name, cycles, iterations, cycles / iterations);
#else
	printf("%s\t%u\n",
	       test->name, cycles / iterations);
#endif
}

static struct exit_test available_tests[] = {
	{ "hvc",		hvc_test,		NULL		},
	{ "vgic_mmio",		mmio_vgic_test,		mmio_vgic_init	},
	{ "fake_mmio",		mmio_fake_test,		NULL		},
	{ "ipi",		ipi_test,		ipi_init	},
	{ "noop_guest",		noop_guest,		NULL		},
};

int test(void)
{
	unsigned int i;
	struct exit_test *test;
	int ret;

	ipi_init();
	ipi_test();

	while (1);


	return 0;

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
