#include "guest.h"
#include "guest-util.h"
#include "mmio_test.h"
#include "vmexit.h"

typedef unsigned long long u64;

static u64 pgd_mem[8] __attribute__ ((aligned (32)));

#define PGD_SHIFT	 30
#define PGD_SIZE 	(1 << PGD_SHIFT)
#define PGD_AF   	(1 << 10) /* Don't raise access flag exceptions */
#define PGD_SH	 	(3 << 8) /* All memory inner+outer shareable */
#define PGD_TYPE_BLOCK	(1 << 0) /* All memory inner+outer shareable */

#define TTBCR_EAE	(0x1UL << 31)

#define SCTLR_M		(0x1 << 0)
#define SCTLR_C		(0x1 << 2)


static inline void set_ttbr1(unsigned long long value)
{
	unsigned long low = (value << 32) >> 32;
	unsigned long high = (value >> 32);

	asm volatile("mcrr p15, 1, %[val_l], %[val_h], c2": :
		     [val_l] "r" (low), [val_h] "r" (high));
	isb();
}

static inline void set_ttbr0(unsigned long long value)
{
	unsigned long low = (value << 32) >> 32;
	unsigned long high = (value >> 32);

	asm volatile("mcrr p15, 0, %[val_l], %[val_h], c2": :
		     [val_l] "r" (low), [val_h] "r" (high));
	isb();
}

static inline unsigned long long get_ttbr0()
{
	unsigned long low;
	unsigned long high;

	asm volatile("mrrc p15, 0, %[val_l], %[val_h], c2":
		     [val_l] "=r" (low), [val_h] "=r" (high));
	return ((unsigned long long)high << 32ULL) | (unsigned long long)low;
}

static inline void set_ttbcr(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c2, c0, 2": :
		     [val] "r" (value));
	isb();
}

static inline unsigned long get_sctlr(void)
{
	unsigned long value;

	asm volatile("mrc p15, 0, %[val], c1, c0, 0":
		     [val] "=r" (value));
	return value;
}

static inline void set_sctlr(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c1, c0, 0": :
		     [val] "r" (value));
	isb();
}

static inline unsigned long get_mair0(void)
{
	unsigned long value;

	asm volatile("mrc p15, 0, %[val], c10, c2, 0":
		     [val] "=r" (value));
	return value;
}

static inline void set_mair0(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c10, c2, 0": :
		     [val] "r" (value));
	isb();
}

void enable_mmu(int cpu)
{
	unsigned long long i;
	unsigned long ttbcr;
	unsigned long long ttbr0;
	unsigned long sctlr;
	unsigned long pgd_ptr;
	unsigned long mair0;
	u64 *pgd;

	/* Set up an identitify mapping */
	pgd = pgd_mem + (4 * cpu);
	for (i = 0; i < 4; i++) {
		pgd[i] = (i * PGD_SIZE);
		pgd[i] |= PGD_AF | PGD_SH | PGD_TYPE_BLOCK;
		if (i < 3) {
			/* HACK !!! */
			pgd[i] |= (1 << 2); /* MAIR0 attr 0 */
		}
	}
	dsb();
	dmb();

	ttbcr = TTBCR_EAE;
	set_ttbcr(ttbcr);

	/*
	 * MAIR0:
	 *  attr0, normal memory
	 *  attr1, device memory
	 */
	mair0 = 0x44;
	mair0 |= (0x04) << 8;
	set_mair0(mair0);

	pgd_ptr = (unsigned long)pgd;
	ttbr0 = (unsigned long long)pgd_ptr & (~(0x1fULL));
	set_ttbr0(ttbr0);

	sctlr = get_sctlr();
	//sctlr |= SCTLR_M | SCTLR_C;
	sctlr |= SCTLR_M;
	set_sctlr(sctlr);

	printf("core[%u]: mmu enabled!\n", cpu);
}
