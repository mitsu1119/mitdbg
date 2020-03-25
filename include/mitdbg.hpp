#pragma once
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cxxabi.h>
#include <elf.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <ext/stdio_filebuf.h>
#include "signame.hpp"

namespace fs = std::filesystem;

using u64 = uint_fast64_t;
using i64 = int_fast64_t;

class Utils {
private:
	static std::string getPath(std::string &name, std::string path) {
		if(path == "") return "";
		std::vector<std::string> paths = Utils::splitStr(path, {':'});

		for(size_t i = 0; i < paths.size(); i++) {
			std::string file = paths[i] + "/" + name;
			if(access(file.c_str(), X_OK) == 0) return file;
		}

		return "";
	}

public:
	// string splitter
	static std::vector<std::string> splitStr(const std::string &str, std::vector<char> &&delim) {
		std::vector<std::string> elems;
		std::string x;
		for(auto ch: str) {
			if(std::find(delim.begin(), delim.end(), ch) != delim.end()) {
				if(!x.empty()) elems.emplace_back(x);
				x.clear();
			} else {
				x += ch;
			}
		}

		if(!x.empty()) elems.emplace_back(x);
		return elems;
	}

	// get executable path
	static std::string getExecPath(std::string &path) {
		if(path.size() == 0) return "";
		if(path[0] == '.') {
			std::string file = fs::absolute(path);
			file = fs::canonical(file);
			return file;
		}

		return Utils::getPath(path, std::getenv("PATH"));
	}	
};

class MyElf {
private:
	char *head;
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdr;
	Elf64_Phdr *phdr;

	// get .shstrtab section
	Elf64_Shdr *getShstrtab();

	// get section header named @name
	Elf64_Shdr *getShdr(std::string name);

public:
	MyElf(std::string fileName);
	~MyElf();

	std::unordered_map<std::string, u64> funcSymbols;

	bool isElf();
	bool isExistFuncSymbol(std::string funcName);
};

enum {
	DBG_ERR = -1, DBG_SUCCESS, DBG_QUIT, DBG_RUN
};

class Breakpoint {
public:
	inline Breakpoint(void *addr, long originalCode): addr(addr), originalCode(originalCode) {
	}

	void *addr;
	long originalCode;
};

// main class
class MitDBG {
private:
	fs::path traced;
	pid_t target;

	MyElf *targetElf;
	u64 baseAddr;

	std::string command;
	std::vector<std::string> commandArgv;

	std::vector<Breakpoint> breaks;

	// get and launch command
	void input();
	int launch();

	// leading this->traced symbol
	int readElf();

	// kill traced process
	int killTarget();

	// set breakpoint
	int setBreak(void *addr);
	// set breakpoint at start address of the function named @funcName
	int setBreak(std::string funcName);	

	// restore the original code of a breakpoint indexd @idx
	int restoreOriginalCodeForBreaks(size_t idx);

	// remove breakpoint from this->breaks
	int removeBreak(void *addr);

	// get "breaks" index from break point address
	i64 searchBreak(void *addr);

	// process while trapping the first (first trapping is end of first execve(...))
	int firstTrap();

	// output registers
	int printRegisters();

	// print disassembled code to use when stopped by breakpoint and signal and so on
	void printDisasStopped(u64 addr);

	// output stlip line
	void printSLine();

	// breakpoint list
	void infoBreaks();

	// parent main loop
	int parentMain();

public:
	MitDBG(std::string traced);
	~MitDBG();

	int main();
};

