#include <stdio.h>
#include <c64/vic.h>

int main(void)
{
	vic.color_border = VCOL_BLACK;
	printf("hello, wasi!\n");
	return 0;
}
