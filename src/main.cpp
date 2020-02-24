#include <iostream>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
	if(argc < 2) {
		std::cout << "USAGE: ./mitdbg [executable-file]" << std::endl;
		return 0;
	}

	/*
	 * The child process executes and replace myself with a command received in command line.
	 */
	pid_t pid = fork();
	if(pid == -1) {
		err(1, "Error: Could not fork.");
		exit(1);
	} else if(pid == 0) {
		/* child process */
		execvp((argv + 1)[0], argv + 1);
		exit(1);
	} else {
		/* parent process */
		int status = 0;

		// Wait child process end.
		pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
		if(w == -1) {
			err(1, "Error %d: Wait child process.", errno);
		}
		
		std::cout << "child = " << pid << ", status = " << status << std::endl;
	}
	
	return 0;
}
