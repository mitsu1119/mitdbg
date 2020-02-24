#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

class Utils {
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
};

// command line process class
class CommandLine {
private:
	std::string command;
	pid_t target;

public:
	CommandLine(pid_t target);

	void input();
	int launch();
};

enum {
	DBG_ERR = -1, DBG_SUCCESS, DBG_QUIT
};

// main class
class MitDBG {
private:
	pid_t target;
	CommandLine *commandline;

	// process while trapping the first (first trapping is end of first execve(...))
	int firstTrap();

	// parent main loop
	int parentMain();

public:
	MitDBG(pid_t target);
	~MitDBG();

	int main();
};

