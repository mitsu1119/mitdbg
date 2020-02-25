#include <iostream>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/ptrace.h>
#include "mitdbg.hpp"

int main(int argc, char *argv[]) {
	if(argc < 2) {
		std::cout << "USAGE: ./mitdbg [executable-file]" << std::endl;
		return 0;
	}

	MitDBG mitdbg(argv[1]);
	mitdbg.main();

	return 0;
}
