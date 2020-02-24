#include <iostream>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

/* 
 * process while trapping the first (first trapping is end of first execve(...))
 * setting ptrace(PTRACE_SETOPTIONS, ...)
 */
int firstTrap(pid_t pid) {
	int status = 0;
	waitpid(pid, &status, WUNTRACED | WCONTINUED);
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC);
	ptrace(PTRACE_SYSCALL, pid, 0, 0);

	std::cout << "First... signal " << WSTOPSIG(status) << std::endl;
	return 0;
}

/* parent main loop */
int parentMain(pid_t pid) {
	pid_t w;
	int signum = 0, status = 0;
	struct user_regs_struct regs;

	while(1) {
		w = waitpid(pid, &status, WUNTRACED | WCONTINUED);

		if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

		if(WIFEXITED(status)) break;
		if(WIFSIGNALED(status)) break;
		if(WIFSTOPPED(status)) {
			signum = WSTOPSIG(status);
			if(signum != (SIGTRAP | 0x80)) {
				std::cout << "Signal " << signum << std::endl;
				ptrace(PTRACE_SYSCALL, pid, 0, signum);
			} else {
				// trapped start or end of systemcall (SIGTRAP | 0x80)
				ptrace(PTRACE_GETREGS, pid, 0, &regs);
				std::cout << "Systemcall " << regs.orig_rax << std::endl;
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
			}
		} else {
			err(1, "error status %d, Wait child process\n\t", status);
			kill(pid, SIGKILL);
			return -1;
		}
	}
	
	return 0;
}

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
		err(1, "Could not fork.\n\t");
		exit(1);
	} else if(pid == 0) {
		/* child process */
		if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) != -1) execvp((argv + 1)[0], argv + 1);
		err(1, "errno %d, PTRACE_TRACEME\n\t", errno);
		exit(1);
	} else {
		/* parent process */

		firstTrap(pid);

		/* parent main loop */
		if(parentMain(pid) == -1) exit(1);
	}
	
	return 0;
}
