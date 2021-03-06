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

		Elf64_Shdr *strtab = getShdr(".strtab");
		Elf64_Shdr *symtab = getShdr(".symtab");
		Elf64_Sym *symb;
		char *symbname;

		// std::cout << "Symbols:" << std::endl;
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
			// set breakpoints
			for(size_t i = 0; i < this->breaks.size(); i++) setBreak(this->breaks[i].addr);
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
			// set breakpoints
			for(size_t i = 0; i < this->breaks.size(); i++) setBreak(this->breaks[i].addr);
			setBreak("main");
			ptrace(PTRACE_CONT, this->target, 0, 0);
		}

		return DBG_RUN;
	}

	if(this->command == "continue" || this->command == "c") {
		struct user_regs_struct regs;
		
		if(this->target == -1) {
			std::cout << "The program is not being run." << std::endl;
			return DBG_SUCCESS;
		}

		ptrace(PTRACE_GETREGS, this->target, 0, &regs);
		u64 addr = regs.rip;

		std::cout << "Continuing." << std::endl;
		ptrace(PTRACE_SINGLESTEP, this->target, NULL, NULL);
		waitpid(this->target, NULL, 0);
		setBreak((void *)addr);
		ptrace(PTRACE_CONT, this->target, 0, 0);

		return DBG_RUN;
	}

	if(this->command == "step" || this->command == "s") {
		struct user_regs_struct regs;

		if(this->target == -1) {
			std::cout << "The program is not being run." << std::endl;
			return DBG_SUCCESS;
		}

		ptrace(PTRACE_GETREGS, this->target, 0, &regs);
		u64 addr = regs.rip;

		std::cout << "STEP " << std::hex << addr << std::endl;
		ptrace(PTRACE_SINGLESTEP, this->target, NULL, NULL);
		waitpid(this->target, NULL, 0);
		ptrace(PTRACE_GETREGS, this->target, 0, &regs);
		std::cout << std::hex << regs.rip << std::endl;
		setBreak((void *)addr);

		return DBG_SUCCESS;
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

	if(this->command == "break" || this->command == "b") {
		struct user_regs_struct regs;
		ptrace(PTRACE_GETREGS, this->target, 0, &regs);

		if(this->commandArgv.size() <= 1) {
			setBreak((void *)regs.rip, false);
			return DBG_SUCCESS;
		}
		if(this->commandArgv[1][0] == '*') {
			// address
			u64 x = std::stoull(this->commandArgv[1].substr(1, this->commandArgv[1].size() - 1), nullptr, 0);
			if(x == regs.rip) setBreak((void *)x, false);
			else setBreak((void *)x);
			return DBG_SUCCESS;
		}
		// symbol
		void *x = (void *)(this->targetElf->funcSymbols[this->commandArgv[1]] + this->baseAddr);
		if((u64)x == regs.rip) setBreak(this->commandArgv[1], false);
		else setBreak(this->commandArgv[1]);
		return DBG_SUCCESS;
	}

	if(this->command == "delete" || this->command == "d") {
		if(this->commandArgv.size() <= 1) {
			while(this->breaks.size() != 0) removeBreak(this->breaks.back().addr);
		} else {
			if(std::isdigit(this->commandArgv[1][0])) {
				i64 idx = std::stoi(this->commandArgv[1]) - 1;
				if(idx < 0 || this->breaks.size() == 0 || (i64)this->breaks.size() < idx + 1) {
					std::cout << "No breakpoint number " << std::dec << idx + 1 << "." << std::endl;
					return DBG_SUCCESS;
				}
				removeBreak(this->breaks[idx].addr);
			}
		}
		return DBG_SUCCESS;
	}

	if(this->command == "info") {
		if(this->commandArgv.size() <= 1) {
			std::cout << "\"info\" must be followed by the name of an info command." << std::endl;
		} else {
			// breakpoints infomation
			if(this->commandArgv[1] == "b") {
				infoBreaks();
			}
		}

		return DBG_SUCCESS;
	}

	if(this->command == "quit" || this->command == "q") {
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

int MitDBG::setBreak(void *addr, bool writeFlag) {
	long originalCode;

	for(size_t i = 0; i < this->breaks.size(); i++) {
		if(this->breaks[i].addr == addr) {
			if(writeFlag) ptrace(PTRACE_POKETEXT, this->target, addr, (this->breaks[i].originalCode & 0xffffffffffffff00) | 0xcc);
			return DBG_SUCCESS;
		}
	}

	if(this->target != -1) {
		originalCode = ptrace(PTRACE_PEEKTEXT, this->target, addr, NULL);
		if(writeFlag) ptrace(PTRACE_POKETEXT, this->target, addr, ((originalCode & 0xffffffffffffff00) | 0xcc));
		this->breaks.emplace_back(addr, originalCode);
		std::cout << "Breakpoint " << this->breaks.size() << " at " << std::hex << (u64)addr << std::endl;
	}

	return DBG_SUCCESS;
}

int MitDBG::setBreak(std::string funcName, bool writeFlag) {
	void *addr = (void *)(this->targetElf->funcSymbols[funcName] + this->baseAddr);
	if((u64)addr == 0) {
		err(1, "The function %s was not found.", funcName.c_str());
		return DBG_ERR;
	}

	return setBreak(addr, writeFlag);
}

int MitDBG::restoreOriginalCodeForBreaks(size_t idx) {
	ptrace(PTRACE_POKETEXT, this->target, (u64)this->breaks[idx].addr, this->breaks[idx].originalCode);
	return DBG_SUCCESS;
}

int MitDBG::removeBreak(void *addr) {
	i64 idx = searchBreak(addr);
	if(idx == -1) return DBG_ERR;

	// restore addr to original text
	restoreOriginalCodeForBreaks(idx);

	this->breaks[idx] = this->breaks.back();
	this->breaks.pop_back();

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

void MitDBG::printDisasStopped(u64 addr) {
	FILE *fp;
	std::string str;

	std::cout << "[-------------------------------------code-------------------------------------]" << std::endl;

	addr -= this->baseAddr;
	std::string com = "objdump -d -M intel " + this->traced.native() + " | grep $(printf '%x' " + std::to_string(addr) + "): -B4 | sed -e \'$d\'";
	std::cout << "command = " << com << ", " << std::hex << this->baseAddr << std::endl;
	fp = popen(com.c_str(), "r");
	__gnu_cxx::stdio_filebuf<char> *fb = new __gnu_cxx::stdio_filebuf<char>(fp, std::ios_base::in);
	std::istream *in = new std::istream((std::streambuf *)fb);
	while(std::getline(*in, str)) {
		std::cout << "   " << str << std::endl;
	}
	pclose(fp);
	delete fb;
	delete in;

	com = "objdump -d -M intel " + this->traced.native() + " | grep $(printf '%x' " + std::to_string(addr) + "): -A4";
	fp = popen(com.c_str(), "r");
	fb = new __gnu_cxx::stdio_filebuf<char>(fp, std::ios_base::in);
	in = new std::istream((std::streambuf *)fb);
	std::cout << "=> ";
	std::getline(*in, str);
	std::cout << str << std::endl;
	while(std::getline(*in, str)) {
		std::cout << "   " << str << std::endl;
	}
	pclose(fp);
	delete fb;
	delete in;
}
 

void MitDBG::printSLine() {
	std::cout << "[------------------------------------------------------------------------------]" << std::endl;
}

void MitDBG::infoBreaks() {
	std::cout << "Num\tAddress" << std::endl;
	for(size_t i = 0; i < this->breaks.size(); i++) {
		std::cout << i + 1 << "\t" << std::hex << (u64)this->breaks[i].addr << std::endl;
	}
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
				std::cout << "[process " << std::dec << this->target << " exited normaly]" << std::endl << std::endl;
				this->target = -1;
			} else if(WIFSIGNALED(status)) {
				signum = WSTOPSIG(status);
				std::cout << "[process " << std::dec << this->target << " exited signal " << signum << "]" << std::endl;
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
					// Restore rip and int 3 code to original code
					regs.rip = regs.rip - 1;
					ptrace(PTRACE_SETREGS, this->target, 0, &regs);
					restoreOriginalCodeForBreaks(searchBreak((void *)regs.rip));

					printRegisters();
					printDisasStopped(regs.rip);
					printSLine();
					std::cout << "Breakpoint " << searchBreak((void *)regs.rip) + 1 << ", " << std::hex << regs.rip << std::endl;
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
	// disable ASLR
	int old, rc;
	old = personality(0xffffffff);
	rc = personality(old | ADDR_NO_RANDOMIZE);
	if(rc == -1) {
		err(1, "Personality error.\n\t");
		return DBG_ERR;
	}

	parentMain();

	return DBG_SUCCESS;
}

