#pragma once
#include <string>
#include <vector>
#include <signal.h>

struct _signallist {
	std::string name;
	int signum;
};

std::string signum2name(int signum);
