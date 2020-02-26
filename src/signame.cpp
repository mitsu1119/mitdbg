#include "signame.hpp"

std::vector<_signallist> siglist = {
	#ifdef SIGINT
		 {"SIGINT", SIGINT},
	#endif
	#ifdef SIGILL
		 {"SIGILL", SIGILL},
	#endif
	#ifdef SIGABRT
		 {"SIGABRT", SIGABRT},
	#endif
	#ifdef SIGFPE
		 {"SIGFPE", SIGFPE},
	#endif
	#ifdef SIGSEGV
		 {"SIGSEGV", SIGSEGV},
	#endif
	#ifdef SIGTERM
		 {"SIGTERM", SIGTERM},
	#endif
	#ifdef SIGHUP
		 {"SIGHUP", SIGHUP},
	#endif
	#ifdef SIGQUIT
		 {"SIGQUIT", SIGQUIT},
	#endif
	#ifdef SIGTRAP
		 {"SIGTRAP", SIGTRAP},
	#endif
	#ifdef SIGKILL
		 {"SIGKILL", SIGKILL},
	#endif
	#ifdef SIGBUS
		 {"SIGBUS", SIGBUS},
	#endif
	#ifdef SIGSYS
		 {"SIGSYS", SIGSYS},
	#endif
	#ifdef SIGPIPE
		 {"SIGPIPE", SIGPIPE},
	#endif
	#ifdef SIGALRM
		 {"SIGALRM", SIGALRM},
	#endif
	#ifdef SIGURG
		 {"SIGURG", SIGURG},
	#endif
	#ifdef SIGSTOP
		 {"SIGSTOP", SIGSTOP},
	#endif
	#ifdef SIGTSTP
		 {"SIGTSTP", SIGTSTP},
	#endif
	#ifdef SIGCONT
		 {"SIGCONT", SIGCONT},
	#endif
	#ifdef SIGCHLD
		 {"SIGCHLD", SIGCHLD},
	#endif
	#ifdef SIGTTIN
		 {"SIGTTIN", SIGTTIN},
	#endif
	#ifdef SIGTTOU
		 {"SIGTTOU", SIGTTOU},
	#endif
	#ifdef SIGPOLL
		 {"SIGPOLL", SIGPOLL},
	#endif
	#ifdef SIGXCPU
		 {"SIGXCPU", SIGXCPU},
	#endif
	#ifdef SIGXFSZ
		 {"SIGXFSZ", SIGXFSZ},
	#endif
	#ifdef SIGVTALRM
		 {"SIGVTALRM", SIGVTALRM},
	#endif
	#ifdef SIGPROF
		 {"SIGPROF", SIGPROF},
	#endif
	#ifdef SIGUSR1
		 {"SIGUSR1", SIGUSR1},
	#endif
	#ifdef SIGUSR2
		 {"SIGUSR2", SIGUSR2},
	#endif
	#ifdef SIGWINCH
		 {"SIGWINCH", SIGWINCH},
	#endif
	#ifdef SIGBUS
		 {"SIGBUS", SIGBUS},
	#endif
	#ifdef SIGPOLL
		 {"SIGPOLL", SIGPOLL},
	#endif
	#ifdef SIGSTKSZ
		 {"SIGSTKSZ", SIGSTKSZ},
	#endif
};

std::string signum2name(int signum) {
	for(size_t n = 0; n < siglist.size(); n++) {
		if(siglist[n].signum == signum) return siglist[n].name;
	}
return "";
}
