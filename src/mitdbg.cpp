#include "mitdbg.hpp"

// ------------------------ MyElf class ------------------------------------------
MyElf::MyElf(std::string fileName) {
	std::ifstream ifs(fileName);
	if(ifs.fail()) {
		err(1, "Faild to open %s.", fileName.c_str());
	} else {
		std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		this->head = new char[s.size()];
		s.copy(this->head, s.size());

		this->ehdr = (Elf64_Ehdr *)this->head;
		this->shdr = (Elf64_Shdr *)(this->head + this->ehdr->e_shoff);
		this->phdr = (Elf64_Phdr *)(this->head + this->ehdr->e_phoff);
		ifs.close();

		std::cout << "Symbols:" << std::endl;

		Elf64_Shdr *strtab = getShdr(".strtab");
		Elf64_Shdr *symtab = getShdr(".symtab");
		Elf64_Sym *symb;
		char *symbname;

		symb = (Elf64_Sym *)(this->head + symtab->sh_offset);
		for(size_t i = 0; i < symtab->sh_size / symtab->sh_entsize; i++) {
			if(symb->st_name) {
				symbname = (char *)(this->head + strtab->sh_offset + symb->st_name);
				std::cout << "\t[" << i << "]\t" << symbname << " " << std::hex << "0x" << symb->st_value << std::endl;
				this->funcSymbols[std::string(symbname)] = symb->st_value;
			}
			symb = (Elf64_Sym *)((char *)symb + symtab->sh_entsize);
		}
	}
}

MyElf::~MyElf() {
	delete[] head;
}

Elf64_Shdr *MyElf::getShstrtab() {
	return (Elf64_Shdr *)((char *)this->shdr + this->ehdr->e_shentsize * this->ehdr->e_shstrndx);
}

Elf64_Shdr *MyElf::getShdr(std::string name) {
	Elf64_Shdr *shdrL = this->shdr;
	Elf64_Shdr *shstr = getShstrtab();
	char *sectionName;

	for(size_t i = 0; i < this->ehdr->e_shnum; i++) {
		sectionName = (char *)(this->head + shstr->sh_offset + shdrL->sh_name);
		if(!strcmp(sectionName, name.c_str())) break;
		shdrL = (Elf64_Shdr *)((char *)shdrL + this->ehdr->e_shentsize);
	}
	return shdrL;
}

bool MyElf::isElf() {
	if(this->ehdr->e_ident[EI_MAG0] == ELFMAG0 && this->ehdr->e_ident[EI_MAG1] == ELFMAG1 && this->ehdr->e_ident[EI_MAG2] == ELFMAG2 && this->ehdr->e_ident[EI_MAG3] == ELFMAG3) return true;
	return false;
}

bool MyElf::isExistFuncSymbol(std::string funcName) {
	if(this->funcSymbols.count(funcName) == 0) return false;
	return true;
}

// ------------------------ MitDBG class -----------------------------------------
MitDBG::MitDBG(std::string traced): traced(Utils::getExecPath(traced)), target(-1), baseAddr(0), command("") {
	this->targetElf = nullptr;

	if(readElf() == 0) {
		if(!this->targetElf->isElf()) {
			this->traced = "";
			delete this->targetElf;
			this->targetElf = nullptr;
		}
	}
}

MitDBG::~MitDBG() {
	if(this->targetElf != nullptr) delete this->targetElf;
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

int MitDBG::readElf() {
	std::cout << "Reading symbols from " << this->traced << std::endl;

	std::ifstream ifs(this->traced);
	if(ifs.fail()) {
		err(1, "Failed to open %s", this->traced.c_str());
		return 1;
	}

	if(this->targetElf != nullptr) {
		delete this->targetElf;
		this->targetElf = nullptr;
	}

	this->targetElf = new MyElf(this->traced);

	return 0;
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
			this->traced = Utils::getExecPath(this->commandArgv[1]);
		}

		return DBG_SUCCESS;
	}

	if(this->command == "run" || this->command == "r") {
		if(this->target != -1) killTarget();

		std::cout << "Starting program: " << this->traced.native() << std::endl;
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

		return DBG_RUN;
	}

	if(this->command == "start") {
		if(this->target != -1) killTarget();
	
		std::cout << "Starting program: " << this->traced.native() << std::endl;
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
			setBreak("main");
			ptrace(PTRACE_CONT, this->target, 0, 0);
		}

		return DBG_RUN;
	}

	if(this->command == "disas") {
		if(this->commandArgv.size() <= 1) {
			std::cout << "No frame selected." << std::endl;
		} else {
			if(!this->targetElf->isExistFuncSymbol(this->commandArgv[1])) {
				std::cout << "Function " << this->commandArgv[1] << "is not found. Try the \"file\" command." << std::endl;
			} else {
				FILE *fp;
				std::string str;
				std::cout << "Dump of assembler code for function " << this->commandArgv[1] << ":";
				std::cout << std::endl;
				std::string com = "objdump -d -M intel " + this->traced.native() + " --disassemble=" + this->commandArgv[1];
				std::cout << "From \"" << com << "\"" << std::endl;
				fp = popen(com.c_str(), "r");
				__gnu_cxx::stdio_filebuf<char> *fb = new __gnu_cxx::stdio_filebuf<char>(fp, std::ios_base::in);
				std::istream *in = new std::istream((std::streambuf *)fb);
				while(std::getline(*in, str)) {
					if(str.find(this->commandArgv[1]) == std::string::npos) continue;
					break;
				}
				while(std::getline(*in, str)) {
					if(str.empty()) break;
					std::cout << str << std::endl;
				}
				std::cout << "End of assembler dump." << std::endl;
				pclose(fp);
				delete in;
			}

		}

		return DBG_SUCCESS;
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

int MitDBG::setBreak(void *addr) {
	long originalCode;

	if(this->target != -1) {
		originalCode = ptrace(PTRACE_PEEKTEXT, this->target, addr, NULL);
		ptrace(PTRACE_POKETEXT, this->target, addr, ((originalCode & 0xffffffffffffff00) | 0xcc));
		this->breaks.emplace_back(addr, originalCode);
		std::cout << "Breakpoint " << this->breaks.size() << " at " << std::hex << addr << std::endl;
	}

	return DBG_SUCCESS;
}

int MitDBG::setBreak(std::string funcName) {
	long originalCode;
	void *addr = (void *)(this->targetElf->funcSymbols[funcName] + this->baseAddr);
	if((u64)addr == 0) {
		err(1, "The function %s was not found.", funcName.c_str());
		return DBG_ERR;
	}

	if(this->target != -1) {
		originalCode = ptrace(PTRACE_PEEKTEXT, this->target, addr, NULL);
		ptrace(PTRACE_POKETEXT, this->target, addr, ((originalCode & 0xffffffffffffff00) | 0xcc));
		this->breaks.emplace_back(addr, originalCode);
		std::cout << "Breakpoint " << this->breaks.size() << " at " << std::hex << addr << std::endl;
	}

	return DBG_SUCCESS;
}

i64 MitDBG::searchBreak(void *addr) {
	if(this->breaks.size() == 0) return -1;
	for(size_t i = 0; i < this->breaks.size(); i++) {
		if(this->breaks[i].addr == addr) return i;
	}
	return -1;
}

/*
 * setting ptrace(PTRACE_SETOPTIONS, ...)
 */
int MitDBG::firstTrap() {
	pid_t w = waitpid(this->target, NULL, WUNTRACED | WCONTINUED);
	if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

	ptrace(PTRACE_SETOPTIONS, this->target, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC);

	// get process base address
	std::ifstream ifs("/proc/" + std::to_string(this->target) + "/maps");
	std::string buf;
	std::getline(ifs, buf);
	buf = Utils::splitStr(buf, {' ','-'})[0];
	this->baseAddr= std::stoull(buf, nullptr, 16);

	return 0;
}

int MitDBG::printRegisters() {
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, this->target, 0, &regs);
	std::cout << "[----------------------------------registers-----------------------------------]" << std::endl;
	std::cout << "RAX: 0x" << std::hex << regs.rax << std::endl;
	std::cout << "RBX: 0x" << std::hex << regs.rbx << std::endl;
	std::cout << "RCX: 0x" << std::hex << regs.rcx << std::endl;
	std::cout << "RDX: 0x" << std::hex << regs.rdx << std::endl;
	std::cout << "RSI: 0x" << std::hex << regs.rsi << std::endl;
	std::cout << "RDI: 0x" << std::hex << regs.rdi << std::endl;
	std::cout << "RBP: 0x" << std::hex << regs.rbp << std::endl;
	std::cout << "RSP: 0x" << std::hex << regs.rsp << std::endl;
	std::cout << "RIP: 0x" << std::hex << regs.rip << std::endl;
	std::cout << "R8 : 0x" << std::hex << regs.r8 << std::endl;
	std::cout << "R9 : 0x" << std::hex << regs.r9 << std::endl;
	std::cout << "R10: 0x" << std::hex << regs.r10 << std::endl;
	std::cout << "R11: 0x" << std::hex << regs.r11 << std::endl;
	std::cout << "R12: 0x" << std::hex << regs.r12 << std::endl;
	std::cout << "R13: 0x" << std::hex << regs.r13 << std::endl;
	std::cout << "R14: 0x" << std::hex << regs.r14 << std::endl;
	std::cout << "R15: 0x" << std::hex << regs.r15 << std::endl;
	std::cout << "EFLAGS: 0x" << std::hex << regs.eflags << std::endl;

	return DBG_SUCCESS;
}

void MitDBG::printSLine() {
	std::cout << "[------------------------------------------------------------------------------]" << std::endl;
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

		if(this->target != -1 && dbgsignal == DBG_RUN) {
			w = waitpid(this->target, &status, WUNTRACED | WCONTINUED);
			if(w == -1) err(1, "errno %d, Wait child process.\n\t", errno);

			if(WIFEXITED(status)) {
				std::cout << "[process " << this->target << " exited normaly]" << std::endl << std::endl;
				this->target = -1;
			} else if(WIFSIGNALED(status)) {
				signum = WSTOPSIG(status);
				std::cout << "[process " << this->target << " exited signal " << signum << "]" << std::endl;
				this->target = -1;
			} else if(WIFSTOPPED(status)) {
				signum = WSTOPSIG(status);
				if(signum == (SIGTRAP | 0x80)) {
					// trapped start or end of systemcall (SIGTRAP | 0x80)
					ptrace(PTRACE_GETREGS, this->target, 0, &regs);
					std::cout << "systemcall " << regs.orig_rax << std::endl;
				} else if(signum == SIGTRAP) {
					// get to break point
					ptrace(PTRACE_GETREGS, this->target, 0, &regs);
					u64 originalAddr = regs.rip - 1;
					printRegisters();
					printSLine();
					std::cout << "Breakpoint " << searchBreak((void *)originalAddr) + 1 << ", " << std::hex << originalAddr << std::endl;
				} else if(signum != (SIGTRAP | 0x80)) {
					std::string signame = signum2name(signum);
					std::cout << "Program received signal " << signame << std::endl;
					std::cout << "Stopped reason: " << signame << std::endl << std::endl;
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

