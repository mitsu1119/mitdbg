#include "mitdbg.hpp"

// ------------------------ MitDBG class -----------------------------------------
MitDBG::MitDBG(std::string traced): traced(traced), target(-1), command("") {
}

void MitDBG::input() {
	std::string buf;

	std::cout << "mitdbg$ ";
	std::getline(std::cin, buf);
	if(buf != "") {
		this->commandArgv = Utils::splitStr(buf, {' '});
		this->command = this->commandArgv[0];
	}
}

/*
 * Command launcher processing.
 * If command is "quit", terminate target process
 */
int MitDBG::launch() {
	if(this->command == "file") {
		if(this->commandArgv.size() <= 1) {
			std::cout << "No executable file now." << std::endl;
		} else {
			this->traced = this->commandArgv[1];
		}
	}

	if(this->command == "run" || this->command == "r") {
		if(this->target != -1) killTarget();

		std::cout << "Starting program: " << this->traced << std::endl;
		this->target = fork();
		if(this->target == -1) {
			err(1, "Could not fork.\n\t");
			return DBG_ERR;
		} else if(this->target == 0) {
			/* child process */
			// The child process executes and replace myself with a command received in command line.
			if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) != -1) execl(this->traced.c_str(), this->traced.c_str(), NULL);
			err(1, "errno %d, PTRACE_TRACEME\n\t", errno);
			return DBG_ERR;
		} else {
			/* main process */
			firstTrap();
			ptrace(PTRACE_CONT, this->target, 0, 0);
		}
	}

	if(this->command == "quit") {
		if(this->target != -1) killTarget();

		return DBG_QUIT;
	}

	return DBG_SUCCESS;
}

int MitDBG::killTarget() {
	if(this->target != -1) {
		kill(this->target, SIGKILL);
		
		// Parent process collects the child exit information.
		// Without this code, child process that should have killed remains as zonbie.
		pid_t w;
		while(1) {
			w = waitpid(this->target, NULL, WNOHANG);
			if(w == this->target) break;
			if(w == -1) {
				err(1, "errno %d, Wait child process.\n\t", errno);
				return DBG_ERR;
			}
		}

		this->target = -1;
	}

	return DBG_SUCCESS;
}


/*
 * setting ptrace(PTRACE_SETOPTIONS, ...)
 */
int MitDBG::firstTrap() {
	int status = 0;
	pid_t w = waitpid(this->target, &status, WUNTRACED | WCONTINUED);
	if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

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

			if(WIFEXITED(status)) {
				std::cout << "[process " << this->target << " exited normaly]" << std::endl;
				this->target = -1;
			} else if(WIFSIGNALED(status)) {
				signum = WSTOPSIG(status);
				std::cout << "[process " << this->target << " exited signal " << signum << "]" << std::endl;
				this->target = -1;
			} else if(WIFSTOPPED(status)) {
				signum = WSTOPSIG(status);
				if(signum != (SIGTRAP | 0x80)) {
					std::string signame = signum2name(signum);
					std::cout << "Program received signal " << signame << std::endl;
					std::cout << "Stopped reason: " << signame << std::endl;
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
	parentMain();

	return DBG_SUCCESS;
}

