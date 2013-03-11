/*
 * guest-driver - start fake VM and test MMIO operations
 * Copyright (C) 2012 Christoffer Dall <cdall@cs.columbia.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#define _GNU_SOURCE

//#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <err.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/kvm.h>
#include <asm/kvm.h>
#include <elf.h>
#include <err.h>
#include <getopt.h>
#include <stddef.h>
#include <pthread.h>
#include <signal.h>
#include "io_common.h"
#include "guest-driver.h"

#define MAX_VCPUS	2

static int nr_cpus = 1;
static bool use_vgic = false;
static int sys_fd;
static int vm_fd;
static int nr_cpus;
static int vcpu_fd[MAX_VCPUS];
static struct kvm_run *kvm_run[MAX_VCPUS];
static pthread_t vcpu_threads[MAX_VCPUS];
static void *code_base;
static struct kvm_userspace_memory_region code_mem;

static void create_vm(void)
{
	vm_fd = ioctl(sys_fd, KVM_CREATE_VM, 0);
	if (vm_fd < 0)
		err(EXIT_SETUPFAIL, "kvm_create_vm failed");
}

static unsigned long vcpu_get_pc(int vcpu_id)
{
	int ret;
	struct kvm_one_reg r;
	unsigned long pc = 0;

	r.id = KVM_REG_ARM;
	r.id |= KVM_REG_ARM_CORE;
	r.id |= KVM_REG_SIZE_U32;
	r.id |= KVM_REG_ARM_CORE_REG(usr_regs.ARM_pc);
	r.addr = (unsigned long long)( (unsigned long)(&pc) );

	ret = ioctl(vcpu_fd[vcpu_id], KVM_GET_ONE_REG, &r);
	if (ret)
		err(EXIT_FAILURE, "KVM_GET_ONE_REG failed");
	return pc;
}

static void create_vcpu(void)
{
	int mmap_size;
	struct kvm_vcpu_init init = { KVM_ARM_TARGET_CORTEX_A15, { 0 } };
	int i;

	if (nr_cpus > MAX_VCPUS)
		err(EXIT_SETUPFAIL, "Attempt to create too many VCPUs failed");

	mmap_size = ioctl(sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (mmap_size < 0)
		err(EXIT_SETUPFAIL, "KVM_GET_VCPU_MMAP_SIZE failed");


	for (i = 0; i < nr_cpus; i++) {
		vcpu_fd[i] = ioctl(vm_fd, KVM_CREATE_VCPU, i);
		if (vcpu_fd[i] < 0)
			err(EXIT_SETUPFAIL, "kvm_create_vcpu failed");

		if (ioctl(vcpu_fd[i], KVM_ARM_VCPU_INIT, &init) != 0)
			err(EXIT_SETUPFAIL, "KVM_ARM_VCPU_INIT failed");

		kvm_run[i] = mmap(NULL, mmap_size,
				  PROT_READ | PROT_WRITE, MAP_SHARED,
				  vcpu_fd[i], 0);
		if (kvm_run[i] == MAP_FAILED)
			err(EXIT_SETUPFAIL, "mmap VCPU run failed!");
	}
}

static void kvm_register_mem(int id, void *addr, unsigned long base,
			    struct kvm_userspace_memory_region *mem)
{
	int ret;

	mem->slot = id;
	mem->guest_phys_addr = base;
	mem->memory_size = RAM_SIZE;
	mem->userspace_addr = (unsigned long)addr;
	mem->flags = 0;

	ret = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, mem);
	if (ret < 0)
		err(EXIT_SETUPFAIL, "error registering region: %d", id);
}

static void register_memregions(void)
{
	code_base = mmap(NULL, RAM_SIZE, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_ANONYMOUS, 0, CODE_PHYS_BASE);
	if (code_base == MAP_FAILED) {
		err(EXIT_SETUPFAIL, "mmap RAM region failed");
	} else if ((unsigned long)code_base & ~PAGE_MASK) {
		errx(EXIT_SETUPFAIL, "mmap RAM on non-page boundary: %p",
		     code_base);
	}
	kvm_register_mem(CODE_SLOT, code_base, CODE_PHYS_BASE, &code_mem);
}

static void read_elf(int elf_fd, const Elf32_Ehdr *ehdr)
{
	Elf32_Phdr phdr[ehdr->e_phnum];
	unsigned int i;

	/* We read in all the program headers at once: */
	if (pread(elf_fd, phdr, sizeof(phdr), ehdr->e_phoff) != sizeof(phdr))
		err(EXIT_SETUPFAIL, "Reading program headers");

	/*
	 * Try all the headers: there are usually only three.  A read-only one,
	 * a read-write one, and a "note" section which we don't load.
	 */
	for (i = 0; i < ehdr->e_phnum; i++) {
		void *dest;

		/* If this isn't a loadable segment, we ignore it */
		if (phdr[i].p_type != PT_LOAD)
			continue;

		dest = code_base + phdr[i].p_paddr - CODE_PHYS_BASE;
		if (dest < code_base
		    || dest + phdr[i].p_memsz > code_base + RAM_SIZE) {
			errx(EXIT_SETUPFAIL, "Section %u@%p out of bounds",
			     phdr[i].p_memsz, (void *)phdr[i].p_paddr);
		}

		if (pread(elf_fd, dest, phdr[i].p_memsz, phdr[i].p_offset)
		    != phdr[i].p_memsz) {
			err(EXIT_SETUPFAIL, "Reading in elf section");
		}
	}
}

static unsigned long load_code(const char *code_file)
{
	int fd = open(code_file, O_RDONLY);
	Elf32_Ehdr ehdr;

	if (fd < 0)
		err(EXIT_SETUPFAIL, "cannot open code file %s", code_file);

	/* Read in the first few bytes. */
	if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
		err(EXIT_SETUPFAIL, "Reading code file %s", code_file);

	/* If it's an ELF file, it starts with "\177ELF" */
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0
	    || ehdr.e_type != ET_EXEC
	    || ehdr.e_machine != EM_ARM
	    || ehdr.e_phentsize != sizeof(Elf32_Phdr)
	    || ehdr.e_phnum < 1 || ehdr.e_phnum > 65536U/sizeof(Elf32_Phdr))
		errx(EXIT_SETUPFAIL, "Malformed elf file %s", code_file);

	read_elf(fd, &ehdr);
	close(fd);
	return ehdr.e_entry;
}

static void init_vcpu(unsigned long start)
{
	int i;
	struct kvm_one_reg reg;
	__u32 lr = CODE_PHYS_BASE + RAM_SIZE;
	__u64 core_id = KVM_REG_ARM | KVM_REG_SIZE_U32 | KVM_REG_ARM_CORE;
	__u32 smp = nr_cpus;
	__u32 vgic_enabled = use_vgic;

	for (i = 0; i < nr_cpus; i++) {
		int fd = vcpu_fd[i];

		reg.id = core_id | KVM_REG_ARM_CORE_REG(usr_regs.ARM_pc);
		reg.addr = (long)&start;
		if (ioctl(fd, KVM_SET_ONE_REG, &reg) != 0)
			err(EXIT_SETUPFAIL, "error setting PC (%#llx)", reg.id);

		reg.id = core_id | KVM_REG_ARM_CORE_REG(svc_regs[2]);
		reg.addr = (long)&lr;
		if (ioctl(fd, KVM_SET_ONE_REG, &reg) != 0)
			err(EXIT_SETUPFAIL, "error setting LR");

		reg.id = core_id | KVM_REG_ARM_CORE_REG(usr_regs.ARM_r0);
		reg.addr = (long)&smp;
		if (ioctl(fd, KVM_SET_ONE_REG, &reg) != 0)
			err(EXIT_SETUPFAIL, "error setting r0");

		reg.id = core_id | KVM_REG_ARM_CORE_REG(usr_regs.ARM_r1);
		reg.addr = (long)&vgic_enabled;
		if (ioctl(fd, KVM_SET_ONE_REG, &reg) != 0)
			err(EXIT_SETUPFAIL, "error setting r1");
	}
}

static void init_vgic(void)
{
	struct kvm_arm_device_addr kda;
	int ret;

	ret = ioctl(vm_fd, KVM_CREATE_IRQCHIP, 0);
	if (ret) {
		err(EXIT_SETUPFAIL, "error creating irqchip: %d", errno);
	}

	/* Set Vexpress VGIC base addresses */

	kda.id = KVM_ARM_DEVICE_VGIC_V2 << KVM_ARM_DEVICE_ID_SHIFT;
	kda.id |= KVM_VGIC_V2_ADDR_TYPE_DIST;
	kda.addr = 0x2c000000 + 0x1000;

	ret = ioctl(vm_fd, KVM_ARM_SET_DEVICE_ADDR, &kda);
	if (ret)
		err(EXIT_SETUPFAIL, "error setting dist addr: %d", errno);

	kda.id = KVM_ARM_DEVICE_VGIC_V2 << KVM_ARM_DEVICE_ID_SHIFT;
	kda.id |= KVM_VGIC_V2_ADDR_TYPE_CPU;
	kda.addr = 0x2c000000 + 0x2000;

	ret = ioctl(vm_fd, KVM_ARM_SET_DEVICE_ADDR, &kda);
	if (ret)
		err(EXIT_SETUPFAIL, "error setting cpu addr: %d", errno);
}

/* Returns true to shut down. */
static bool handle_mmio(struct kvm_run *kvm_run, int vcpu_id,
			bool (*test)(struct kvm_run *kvm_run, int vcpu_fd))
{
	unsigned long phys_addr;
	unsigned char *data;
	bool is_write;

	if (kvm_run->exit_reason != KVM_EXIT_MMIO)
		return false;

	phys_addr = (unsigned long)kvm_run->mmio.phys_addr;
	data = kvm_run->mmio.data;
	is_write = kvm_run->mmio.is_write;

	/* Test if it's a control operation */
	switch (phys_addr) {
	case IO_CTL_STATUS:
		if (!is_write)
			errx(EXIT_SETUPFAIL, "Guest read from IO_CTL_STATUS");
		if (data[0] == 0) {
			debug(".");
			return false;
		} else {
			errx(EXIT_FAILURE, "TEST FAIL");
		}

	case IO_CTL_PRINT:
		if (!is_write)
			errx(EXIT_SETUPFAIL, "Guest read from IO_CTL_PRINT");
		printf("%c", data[0]);
		return false;

	case IO_CTL_EXIT: {
		/* Kill other CPUs */
		printf("VM shutting down status %i\n", data[0]);
		if (data[0] != 0)
			exit(data[0]);
		exit(1);
	}
	default:
		/* Let this test handle it. */
		if (test && test(kvm_run, vcpu_fd[vcpu_id]))
			return false;
		errx(EXIT_FAILURE,
		     "Guest accessed unexisting mem area: %#08lx + %#08x",
		     phys_addr, kvm_run->mmio.len);
	}
}

struct cpu_exec_args {
	bool (*test_fn)(struct kvm_run *kvm_run, int vcpu_fd);
	int vcpu_id;
};

void set_cpu_affinity(int cpuid)
{
	int s, i;
	cpu_set_t cpuset;
	pthread_t thread;

	thread = pthread_self();

	CPU_ZERO(&cpuset);
	CPU_SET(cpuid, &cpuset);
	s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0)
		err(EXIT_SETUPFAIL, "Error setting affinity");

	s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0)
		err(EXIT_SETUPFAIL, "Can't get affinity?");

	debug("cpu[%d]: affinity: ");
	for (i = 0; i < 4; i++) {
		if (CPU_ISSET(i, &cpuset))
			debug("cpu%d, ", i);
	}
	debug("\n");
}

static void *kvm_cpu_exec(void *opaque)
{
	struct cpu_exec_args *args = (struct cpu_exec_args *)opaque;
	int id = args->vcpu_id;

	set_cpu_affinity(id);

	do {
		int ret = ioctl(vcpu_fd[id], KVM_RUN, 0);

		if (ret != -EINTR && ret != -EAGAIN && ret < 0) {
			fprintf(stderr, "err at 0x%08lx\n", vcpu_get_pc(id));
			err(EXIT_SETUPFAIL, "Error running vcpu");
		}
	} while (!handle_mmio(kvm_run[id], id, args->test_fn));

	return NULL;
}

/* Linker-generated symbols for GUEST_TEST() macros */
extern struct test __start_tests[], __stop_tests[];


static void usage(int argc, char * const *argv)
{
	struct test *i;
	fprintf(stderr, "Usage: %s <testname>\n\n", argv[0]);
	fprintf(stderr, "Available test:\n");

	for (i = __start_tests; i < __stop_tests; i++)
		fprintf(stderr, " - %s:\n", i->name);

	errx(EXIT_SETUPFAIL, "failed");
}


int main(int argc, char * const *argv)
{
	struct test *tptr;
	const char *file = NULL;
	bool (*test)(struct kvm_run *kvm_run, int vcpu_fd);
	unsigned long start;
	int opt;
	char *test_name;
	int ret, i;

	nr_cpus = 1;

	while ((opt = getopt(argc, argv, "vm")) != -1) {
		switch (opt) {
		case 'v':
			use_vgic = true;
			break;
		case 'm':
			nr_cpus = 2;
			break;
		default:
			usage(argc, argv);
		}
	}

	if (optind >= argc)
		usage(argc, argv);

	test_name = argv[optind];
	for (tptr = __start_tests; tptr < __stop_tests; tptr++) {
		if (strcmp(tptr->name, test_name) == 0) {
			test = tptr->mmiofn;
			file = tptr->binname;
			break;
		}
	}
	if (!file)
		errx(EXIT_SETUPFAIL, "Unknown test '%s'", argv[1]);

	debug("Starting VM with code from: %s\n", file);

	sys_fd = open("/dev/kvm", O_RDWR);
	if (sys_fd < 0)
		err(EXIT_SETUPFAIL, "cannot open /dev/kvm - module loaded?");

	create_vm();
	register_memregions();
	if (use_vgic)
		init_vgic();
	start = load_code(file);
	create_vcpu();
	init_vcpu(start);

	for (i = 0; i < nr_cpus; i++) {
		struct cpu_exec_args *a;

		a = malloc(sizeof(struct cpu_exec_args));
		if (!a)
			errx(EXIT_SETUPFAIL, "Out of address space?!?");
		a->test_fn = test;
		a->vcpu_id = i;

		ret = pthread_create(&vcpu_threads[i], NULL,
				     kvm_cpu_exec, a);
		if (ret)
			errx(EXIT_SETUPFAIL, "Pthread create failed");
	}

	for (i = 0; i < nr_cpus; i++) {
		pthread_join(vcpu_threads[i], NULL);
	}

	return EXIT_SUCCESS;
}
