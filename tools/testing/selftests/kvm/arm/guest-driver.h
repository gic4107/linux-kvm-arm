#ifndef __GUEST_DRIVER_H_
#define __GUEST_DRIVER_H_
#include <stdint.h>

#define PAGE_SIZE (4096)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define EXIT_SETUPFAIL 2

#define CODE_SLOT 0
#define CODE_PHYS_BASE (0xFFE00000)
#define RAM_SIZE 0x200000 /* 2M of physical RAM , yeah! */
#define PHYS_OFFSET(_addr)	(_addr & ~(CODE_PHYS_BASE))

struct kvm_run;
struct test {
	const char *name;
	const char *binname;
	bool (*mmiofn)(struct kvm_run *kvm_run, int vcpu_fd);
};

#define stringify(expr)		stringify_1(expr)
/* Double-indirection required to stringify expansions */
#define stringify_1(expr)	#expr

#define GUEST_TEST(name, testfn)					\
	struct test test_##name __attribute__((section("tests"))) = {	\
		stringify(name), stringify(name) "-guest", testfn }

typedef uint32_t u32;

#ifdef DEBUG
#define debug(args...) printf(args)
#else
#define debug(args...)
#endif

#endif /* __GUEST_DRIVER_H_ */
