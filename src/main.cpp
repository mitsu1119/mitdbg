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

	MitDBG *mitdbg;

	/*
	 * The child process executes and replace myself with a command received in command line.
	 */
	pid_t pid = fork();
	int dbgsignal;
	if(pid == -1) {
		err(1, "Could not fork.\n\t");
		exit(1);
	} else if(pid == 0) {
		/* child process */
		if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) != -1) execvp((argv + 1)[0], argv + 1);
		err(1, "errno %d, PTRACE_TRACEME\n\t", errno);
		exit(1);
	} else {
		/* parent process */
		mitdbg = new MitDBG(pid);
		dbgsignal = mitdbg->main();
	}

	delete mitdbg;

	switch(dbgsignal) {
	case DBG_ERR:
		return -1;
	};
	
	return 0;
}
