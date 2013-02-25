#ifndef _VMEXIT_H_
#define _VMEXIT_H_

#define FAKE_MMIO		0x1c020000

#define VEXPRESS_VGIC_BASE	0x2c000000
#define VGIC_DIST_BASE		(VEXPRESS_VGIC_BASE + 0x1000)
#define VGIC_CPU_BASE		(VEXPRESS_VGIC_BASE + 0x2000)

#define GICC_CTLR		0x00000000
#define GICC_PMR		0x00000004

#define GICD_CTLR		0x00000000
#define GICD_ISENABLE(_n)	(0x00000100 + ((_n / 32) * 4))
#define GICD_SGIR		0x00000f00
#define GICD_SPENDSGI		0x00000f20

#define ISENABLE_IRQ(_irq)	(1UL << (_irq % 32))

#define SGI_SET_PENDING(_target_cpu, _source_cpu) \
	((1UL << _target_cpu) << (8 * _source_cpu))

#define SGIR_IRQ_MASK			((1UL << 4) - 1)
#define SGIR_NSATTR			(1UL << 15)
#define SGIR_CPU_TARGET_LIST_SHIFT	(16)

#define SGIR_FORMAT(_target_cpu, _irq_num) ( \
	((1UL << _target_cpu) << SGIR_CPU_TARGET_LIST_SHIFT) | \
	((_irq_num) & SGIR_IRQ_MASK) | \
	SGIR_NSATTR)
	

#endif /* _VMEXIT_H_ */
