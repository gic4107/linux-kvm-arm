#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define DEVICE "/dev/mydev"

int main()
{
	FILE *fp = fopen(DEVICE, "w+");
	char buf[100] = "u0256070 hello lab4";
	fwrite(buf, sizeof(char), strlen(buf), fp);
	char tmp[100];
	fread(tmp, sizeof(char), strlen(buf), fp);
	printf("%s\n", tmp);
	fclose(fp);

	return 0;
} 
