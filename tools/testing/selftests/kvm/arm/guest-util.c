#include <stdarg.h>
#include "guest-util.h"

/* Only understands %u, %s */
void printf(const char *fmt, ...)
{
	va_list ap;
	unsigned val;
	unsigned long long llval;
	char intbuf[20], *p;
	unsigned long numlen = 4;
	bool modifier = false;

	va_start(ap, fmt);
	while (*fmt) {
		if (*fmt != '%' && !modifier) {
			putc(*(fmt++));
			continue;
		} else if (*fmt == '%') {
			fmt++;
			numlen = 4;
		}
		modifier = false;

		switch (*fmt) {
		case 'u':
			fmt++;
			if (numlen == 8) {
				llval = va_arg(ap, unsigned long long);
				val = (unsigned long)llval; /* TODO! */
			} else {
				val = va_arg(ap, int);
			}
			if (!val) {
				putc('0');
				continue;
			}
			p = &intbuf[19];
			*(p--) = '\0';
			while (val) {
				*(p--) = (val % 10) + '0';
				val /= 10;
			}
			print(p+1);
			break;
		case 'x':
			fmt++;

			if (numlen == 8) {
				llval = va_arg(ap, unsigned long long);
				p = &intbuf[15];
			} else {
				val = va_arg(ap, int);
				llval = (unsigned long long)val;
				p = &intbuf[7];
			}

			if (!llval) {
				putc('0');
				continue;
			}

			*(p--) = '\0';
			while (llval) {
				unsigned long long rem = llval % 16;
				if (rem > 9)
					*(p--) = (rem - 10ULL) + 'a';
				else
					*(p--) = rem + '0';
				llval /= 16ULL;
			}
			print(p+1);
			break;
		case 'l':
			fmt++;
			modifier = true;
			if (*fmt == 'l') {
				numlen = 8;
				fmt++;
			}

			break;
		case 's':
			fmt++;
			p = va_arg(ap, char *);
			print(p);
			break;
		default:
			putc('%');
			continue;
		}
	}
	va_end(ap);
}

void __guest_div0(void)
{
	printf("division by 0\n");
	fail();
}

void guest_abort_exception(int cpu, unsigned long addr, unsigned long pc)
{
	printf("core[%u]: unexpected abort on: 0x%x (lr: 0x%x)\n",
	       cpu, addr, pc);
}
