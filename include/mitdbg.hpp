#pragma once
#include <iostream>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

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

