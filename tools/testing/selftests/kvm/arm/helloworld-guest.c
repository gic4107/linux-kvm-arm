#include "guest.h"
#include "mmio_test.h"

int test(int smp_cpus, int vgic_enabled)
{
	printf("Hello World (smp: %u, vgic: %u)\n", smp_cpus, vgic_enabled);
	return 0;
}
