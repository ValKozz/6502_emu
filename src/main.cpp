#include <iostream>

#include "cpu.h"

int main(int argc, char **argv) {
	printf("Hello World!\n");
	CPU _6502(1);
	_6502.run();
	return 0;
}
