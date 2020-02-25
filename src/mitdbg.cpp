#include "mitdbg.hpp"

// ------------------------ MitDBG class -----------------------------------------
MitDBG::MitDBG(std::string traced): traced(traced), target(-1), command("") {
}

void MitDBG::input() {
	std::string buf;

	std::cout << "mitdbg$ ";
	std::getline(std::cin, buf);
	if(buf != "") this->command = buf;
}

/*
 * Command launcher processing.
 * If command is "quit", terminate target process
 */
int MitDBG::launch() {
	if(this->command == "run" || this->command == "r") {
		if(this->target != -1) {
			kill(this->target, SIGTERM);
			this->target = -1;
		}

		std::cout << "Starting program: " << this->traced << std::endl;
		this->target = fork();
		if(this->target == -1) {
			err(1, "Could not fork.\n\t");
			return DBG_ERR;
		}

		firstTrap();
		ptrace(PTRACE_CONT, this->target, 0, 0);
	}

	if(this->command == "quit") {
		if(this->target != -1) kill(this->target, SIGTERM);
		return DBG_QUIT;
	}

	return DBG_SUCCESS;
}

/*
 * setting ptrace(PTRACE_SETOPTIONS, ...)
 */
int MitDBG::firstTrap() {
	int status = 0;
	while(waitpid(this->target, &status, WUNTRACED | WCONTINUED) == -1);
	ptrace(PTRACE_SETOPTIONS, this->target, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC);
	return 0;
}

/*
 * managing a child process and responsing the signal
 */
int MitDBG::parentMain() {
	int dbgsignal = DBG_SUCCESS;
	pid_t w;
	int status = 0, signum = 0;
	struct user_regs_struct regs;
	while(1) {
		input();
		dbgsignal = launch();

		// If the command is a debug termination command, break this while loop.
		if(dbgsignal == DBG_QUIT) return DBG_QUIT;

		if(this->target != -1) {
			w = waitpid(this->target, &status, WUNTRACED | WCONTINUED);
			if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

			if(WIFEXITED(status)) break;
			if(WIFSIGNALED(status)) break;
			if(WIFSTOPPED(status)) {
				signum = WSTOPSIG(status);
				if(signum != (SIGTRAP | 0x80)) {
					std::cout << "not SIGTRAP" << std::endl;
					ptrace(PTRACE_SYSCALL, this->target, 0, signum);
				} else {
					// trapped start or end of systemcall (SIGTRAP | 0x80)
					ptrace(PTRACE_GETREGS, this->target, 0, &regs);
					std::cout << "systemcall " << regs.orig_rax << std::endl;
				}
			} else {
				err(1, "error status %d, Wait child process\n\t", status);
				kill(this->target, SIGKILL);
				this->target = -1;
				return DBG_ERR;
			}
		}
	}
	
	return DBG_SUCCESS;
}

int MitDBG::main() {
	int status = 0;
	
	// Call fork() once to determine if it is the parent process.
	pid_t pid = fork();

	if(pid == -1) {
		err(1, "Could not fork.\n\t");
		return DBG_ERR;
	} else if(pid == 0) {
		/* child process */
		// The child process executes and replace myself with a command received in command line.
		if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) != -1) execl(this->traced.c_str(), this->traced.c_str(), NULL);
		err(1, "errno %d, PTRACE_TRACEME\n\t", errno);
		return DBG_ERR;
	} else {
		/* parent process */
		
		// kill child process for determination
		kill(pid, SIGTERM);

		// Parent process collects the child exit information.
		// Without this code, child process that should have killed remains as zonbie.
		waitpid(pid, &status, 0);

		/* parent main loop */
		parentMain();
	}

	return DBG_SUCCESS;
}

