#include "mitdbg.hpp"

// -------------------- CommandLine class -----------------------------------------
CommandLine::CommandLine(pid_t target): command(""), target(target) {
}

void CommandLine::input() {
	std::string buf;

	std::cout << "mitdbg$ ";
	std::getline(std::cin, buf);
	if(buf != "") this->command = buf;
}

int CommandLine::launch() {
	/*
	 * Command launcher processing.
	 * If command is "quit", terminate target process
	 */
	if(this->command == "run" || this->command == "r") {
		std::cout << "Starting program: ";

		std::string buf;
		std::ifstream ifs("/proc/" + std::to_string(this->target) + "/cmdline");
		std::getline(ifs, buf);
		std::vector<std::string> names = Utils::splitStr(buf, {'\x00'});
		for(auto &x: names) std::cout << x << " ";
		std::cout << std::endl;

		ptrace(PTRACE_CONT, this->target, 0, 0);
	}

	if(this->command == "quit") {
		kill(this->target, SIGKILL);
		return DBG_QUIT;
	}

	return DBG_SUCCESS;
}

// ------------------------ MitDBG class -----------------------------------------
MitDBG::MitDBG(pid_t target): target(target) {
	commandline = new CommandLine(this->target);
	firstTrap();
}

MitDBG::~MitDBG() {
	delete this->commandline;
}

/*
 * setting ptrace(PTRACE_SETOPTIONS, ...)
 */
int MitDBG::firstTrap() {
	int status = 0;
	waitpid(this->target, &status, WUNTRACED | WCONTINUED);
	ptrace(PTRACE_SETOPTIONS, this->target, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC);
	ptrace(PTRACE_SYSCALL, this->target, 0, 0);
	return 0;
}

/*
 * managing a child process and responsing the signal
 * If get a (SIGTRAP | 0x80) signal, stop child process and manage debugger commands.
 */
int MitDBG::parentMain() {
	pid_t w;
	int signum = 0, status = 0, launchSignal = 0;
	struct user_regs_struct regs;

	while(1) {
		w = waitpid(this->target, &status, WUNTRACED | WCONTINUED);

		if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

		if(WIFEXITED(status)) break;
		if(WIFSIGNALED(status)) break;
		if(WIFSTOPPED(status)) {
			signum = WSTOPSIG(status);
			if(signum != (SIGTRAP | 0x80)) {
				ptrace(PTRACE_SYSCALL, this->target, 0, signum);
			} else {
				// trapped start or end of systemcall (SIGTRAP | 0x80)
				ptrace(PTRACE_GETREGS, this->target, 0, &regs);
				std::cout << "systemcall " << regs.orig_rax << std::endl;
				this->commandline->input();
				launchSignal = this->commandline->launch();

				// If the command is a debug termination command, break this while loop.
				if(launchSignal == DBG_QUIT) {
					return DBG_QUIT;
				}
			}
		} else {
			err(1, "error status %d, Wait child process\n\t", status);
			kill(this->target, SIGKILL);
			return DBG_ERR;
		}
	}
	
	return DBG_SUCCESS;
}

int MitDBG::main() {
	return parentMain();
}

