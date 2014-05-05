#include <stdio.h>
#include <fcntl.h>
#include "../../include/uapi/linux/interrupt_distributor.h"
//#include "interrupt_distributor.h"

int main()
{
	int fd = open("/dev/isr_dst", O_RDWR);
	if(fd<0)
		printf("fd<0\n");
	ioctl(fd, SEND_IRQ_TO_GUEST, 0);
}
