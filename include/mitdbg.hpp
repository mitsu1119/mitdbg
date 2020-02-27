#pragma once
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include "signame.hpp"

namespace fs = std::filesystem;

using u64 = uint_fast64_t;

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

	u64 baseAddr;

	std::string command;
	std::vector<std::string> commandArgv;

	std::vector<Breakpoint> breaks;

	// get and launch command
	void input();
	int launch();

	// kill traced process
	int killTarget();

	// set breakpoint
	int setBreak(void *addr);

	// process while trapping the first (first trapping is end of first execve(...))
	int firstTrap();

	// parent main loop
	int parentMain();

public:
	MitDBG(std::string traced);

	int main();
};

