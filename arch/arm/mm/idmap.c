#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <asm/cputype.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>

#include <asm/sections.h>
#include <asm/system_info.h>
#include <asm/virt.h>

static void idmap_add_pmd(pud_t *pud, unsigned long addr, unsigned long end,
	unsigned long prot)
{
	pmd_t *pmd = pmd_offset(pud, addr);

	addr = (addr & PMD_MASK) | prot;
	pmd[0] = __pmd(addr);
	addr += SECTION_SIZE;
	pmd[1] = __pmd(addr);
	flush_pmd_entry(pmd);
}

static void idmap_add_pud(pgd_t *pgd, unsigned long addr, unsigned long end,
	unsigned long prot)
{
	pud_t *pud = pud_offset(pgd, addr);
	unsigned long next;

	do {
		next = pud_addr_end(addr, end);
		idmap_add_pmd(pud, addr, next, prot);
	} while (pud++, addr = next, addr != end);
}

static void identity_mapping_add(pgd_t *pgd, const char *text_start,
				 const char *text_end, unsigned long prot)
{
	unsigned long addr, end;
	unsigned long next;

	addr = virt_to_phys(text_start);
	end = virt_to_phys(text_end);

	prot |= PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AF;

	if (cpu_architecture() <= CPU_ARCH_ARMv5TEJ && !cpu_is_xscale())
		prot |= PMD_BIT4;

	pgd += pgd_index(addr);
	do {
		next = pgd_addr_end(addr, end);
		idmap_add_pud(pgd, addr, next, prot);
	} while (pgd++, addr = next, addr != end);
}

#ifdef CONFIG_SMP
static void idmap_del_pmd(pud_t *pud, unsigned long addr, unsigned long end)
{
	pmd_t *pmd = pmd_offset(pud, addr);
	pmd_clear(pmd);
}

#if defined(CONFIG_ARM_VIRT_EXT) && defined(CONFIG_ARM_LPAE)
pgd_t *hyp_pgd;

extern char  __hyp_idmap_text_start[], __hyp_idmap_text_end[];

static int __init init_static_idmap_hyp(void)
{
	hyp_pgd = kzalloc(PTRS_PER_PGD * sizeof(pgd_t), GFP_KERNEL);
	if (!hyp_pgd)
		return -ENOMEM;

	pr_info("Setting up static HYP identity map for 0x%p - 0x%p\n",
		__hyp_idmap_text_start, __hyp_idmap_text_end);
	identity_mapping_add(hyp_pgd, __hyp_idmap_text_start,
			     __hyp_idmap_text_end, PMD_SECT_AP1);

	return 0;
}
#else
static int __init init_static_idmap_hyp(void)
{
	return 0;
}
#endif

extern char  __idmap_text_start[], __idmap_text_end[];

static int __init init_static_idmap(void)
{
	int ret;
	pr_info("Setting up static identity map for 0x%p - 0x%p\n",
		__idmap_text_start, __idmap_text_end);
	identity_mapping_add(idmap_pgd, __idmap_text_start,
			     __idmap_text_end, 0);

	ret = init_static_idmap_hyp();

	return ret;
}

static void idmap_del_pud(pgd_t *pgd, unsigned long addr, unsigned long end)
{
	pud_t *pud = pud_offset(pgd, addr);
	unsigned long next;

	do {
		next = pud_addr_end(addr, end);
		idmap_del_pmd(pud, addr, next);
	} while (pud++, addr = next, addr != end);
}

void identity_mapping_del(pgd_t *pgd, unsigned long addr, unsigned long end)
{
	unsigned long next;

	pgd += pgd_index(addr);
	do {
		next = pgd_addr_end(addr, end);
		idmap_del_pud(pgd, addr, next);
	} while (pgd++, addr = next, addr != end);
}
#endif

/*
 * In order to soft-boot, we need to insert a 1:1 mapping in place of
 * the user-mode pages.  This will then ensure that we have predictable
 * results when turning the mmu off
 */
void setup_mm_for_reboot(char mode)
{
	/*
	 * We need to access to user-mode page tables here. For kernel threads
	 * we don't have any user-mode mappings so we use the context that we
	 * "borrowed".
	 */
	identity_mapping_add(current->active_mm->pgd, 0, TASK_SIZE);
	local_flush_tlb_all();
}
