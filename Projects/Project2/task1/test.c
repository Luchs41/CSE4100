#include <stdio.h>

void main()
{
	char buf[5];
	buf[0] = 'a';
	buf[1] = 'b';
	buf[2] = 0;
	buf[3] = 'c';
	buf[4] = '\0';
	printf("%s\n", buf);
	printf("%d\n", strlen(buf));

}

