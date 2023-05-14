// Copyright 2023 Javier Izquierdo Hernández
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "parse.h"
#include "exec_info.h"
#include "builtin/jobs.h"
#include "builtin/kill.h"

char * kill_use = "kill [-s sigspec] | [-n signum] | [-sigspec] jobspec or pid or kill -l [sigspec]";
char * kill_description = "Send a signal to a job.";
char * kill_help = 
"    Send the processes identified by PID or JOBSPEC the signal named by\n"
"    SIGSPEC or SIGNUM.  If neither SIGSPEC nor SIGNUM is present, then\n"
"    SIGTERM is assumed.\n\n"
"    Options:\n"
"      -s sig    SIG is a signal name\n"
"      -n sig    SIG is a signal number\n"
"      -l        list the signal names; if an argument follows `-l' it is\n"
"                assumed to be a signal name for which number should be listed\n"
"      -L        synonym for -l\n"
"    Exit Status:\n"
"    Returns success unless an invalid option is given or an error occurs or job control is not enabled.\n";

static int out_fd;
static int err_fd;

static int help() {
	dprintf(out_fd, "kill: %s\n", kill_use);
	dprintf(out_fd, "    %s\n\n%s", kill_description, kill_help);
	return EXIT_SUCCESS;
}

static int usage() {
  dprintf(err_fd,"Usage: %s\n",kill_use);
	return EXIT_FAILURE;
}

static int invalid_signal(char *signal) {
  dprintf(err_fd, "mash: kill: %s: invalid signal specification\n", signal);
	return EXIT_FAILURE;
}

static int getSignal(char *line) {
  if (strcasecmp(line, "SIGHUP") == 0) {
    return SIGHUP;
  } else if (strcasecmp(line, "SIGINT") == 0) {
    return SIGINT;
  } else if (strcasecmp(line, "SIGQUIT") == 0) {
    return SIGQUIT;
  } else if (strcasecmp(line, "SIGILL") == 0) {
    return SIGILL;
  } else if (strcasecmp(line, "SIGTRAP") == 0) {
    return SIGTRAP;
  } else if (strcasecmp(line, "SIGABRT") == 0) {
    return SIGABRT;
  } else if (strcasecmp(line, "SIGBUS") == 0) {
    return SIGBUS;
  }  else if (strcasecmp(line, "SIGFPE") == 0) {
    return SIGFPE;
  } else if (strcasecmp(line, "SIGQUIT") == 0) {
    return SIGQUIT;
  } else if (strcasecmp(line, "SIGKILL") == 0) {
    return SIGKILL;
  } else if (strcasecmp(line, "SIGUSR1") == 0) {
    return SIGUSR1;
  } else if (strcasecmp(line, "SIGSEGV") == 0) {
    return SIGSEGV;
  } else if (strcasecmp(line, "SIGUSR2") == 0) {
    return SIGUSR2;
  } else if (strcasecmp(line, "SIGPIPE") == 0) {
    return SIGPIPE;
  } else if (strcasecmp(line, "SIGALRM") == 0) {
    return SIGALRM;
  } else if (strcasecmp(line, "SIGTERM") == 0) {
    return SIGTERM;
  } else if (strcasecmp(line, "SIGSTKFLT") == 0) {
    return SIGSTKFLT;
  } else if (strcasecmp(line, "SIGCHLD") == 0) {
    return SIGCHLD;
  } else if (strcasecmp(line, "SIGCONT") == 0) {
    return SIGCONT;
  }  else if (strcasecmp(line, "SIGSTOP") == 0) {
    return SIGSTOP;
  } else if (strcasecmp(line, "SIGTSTP") == 0) {
    return SIGTSTP;
  } else if (strcasecmp(line, "SIGTTIN") == 0) {
    return SIGTTIN;
  } else if (strcasecmp(line, "SIGTTOU") == 0) {
    return SIGTTOU;
  } else if (strcasecmp(line, "SIGURG") == 0) {
    return SIGURG;
  } else if (strcasecmp(line, "SIGXCPU") == 0) {
    return SIGXCPU;
  } else if (strcasecmp(line, "SIGXFSZ") == 0) {
    return SIGXFSZ;
  } else if (strcasecmp(line, "SIGVTALRM") == 0) {
    return SIGVTALRM;
  } else if (strcasecmp(line, "SIGPROF") == 0) {
    return SIGPROF;
  } else if (strcasecmp(line, "SIGWINCH") == 0) {
    return SIGWINCH;
  } else if (strcasecmp(line, "SIGIO") == 0) {
    return SIGIO;
  } else if (strcasecmp(line, "SIGPWR") == 0) {
    return SIGPWR;
  }  else if (strcasecmp(line, "SIGSYS") == 0) {
    return SIGSYS;
  } else if (strcasecmp(line, "SIGRTMIN") == 0) {
    return SIGRTMIN;
  } else if (strcasecmp(line, "SIGRTMIN+1") == 0) {
    return SIGRTMIN+1;
  } else if (strcasecmp(line, "SIGRTMIN+2") == 0) {
    return SIGRTMIN+2;
  } else if (strcasecmp(line, "SIGRTMIN+3") == 0) {
    return SIGRTMIN+3;
  } else if (strcasecmp(line, "SIGRTMIN+4") == 0) {
    return SIGRTMIN+4;
  } else if (strcasecmp(line, "SIGRTMIN+5") == 0) {
    return SIGRTMIN+5;
  } else if (strcasecmp(line, "SIGRTMIN+6") == 0) {
    return SIGRTMIN+6;
  } else if (strcasecmp(line, "SIGRTMIN+7") == 0) {
    return SIGRTMIN+7;
  } else if (strcasecmp(line, "SIGRTMIN+8") == 0) {
    return SIGRTMIN+8;
  } else if (strcasecmp(line, "SIGRTMIN+9") == 0) {
    return SIGRTMIN+9;
  } else if (strcasecmp(line, "SIGRTMIN+10") == 0) {
    return SIGRTMIN+10;
  } else if (strcasecmp(line, "SIGRTMIN+11") == 0) {
    return SIGRTMIN+11;
  } else if (strcasecmp(line, "SIGRTMIN+12") == 0) {
    return SIGRTMIN+12;
  } else if (strcasecmp(line, "SIGRTMIN+13") == 0) {
    return SIGRTMIN+13;
  } else if (strcasecmp(line, "SIGRTMIN+14") == 0) {
    return SIGRTMIN+14;
  } else if (strcasecmp(line, "SIGRTMIN+15") == 0) {
    return SIGRTMIN+15;
  } else if (strcasecmp(line, "SIGRTMAX-14") == 0) {
    return SIGRTMAX-14;
  } else if (strcasecmp(line, "SIGRTMAX-13") == 0) {
    return SIGRTMAX-13;
  } else if (strcasecmp(line, "SIGRTMAX-12") == 0) {
    return SIGRTMAX-12;
  } else if (strcasecmp(line, "SIGRTMAX-11") == 0) {
    return SIGRTMAX-11;
  } else if (strcasecmp(line, "SIGRTMAX-10") == 0) {
    return SIGRTMAX-10;
  } else if (strcasecmp(line, "SIGRTMAX-9") == 0) {
    return SIGRTMAX-9;
  } else if (strcasecmp(line, "SIGRTMAX-8") == 0) {
    return SIGRTMAX-8;
  } else if (strcasecmp(line, "SIGRTMAX-7") == 0) {
    return SIGRTMAX-7;
  } else if (strcasecmp(line, "SIGRTMAX-6") == 0) {
    return SIGRTMAX-6;
  } else if (strcasecmp(line, "SIGRTMAX-5") == 0) {
    return SIGRTMAX-5;
  } else if (strcasecmp(line, "SIGRTMAX-4") == 0) {
    return SIGRTMAX-4;
  } else if (strcasecmp(line, "SIGRTMAX-3") == 0) {
    return SIGRTMAX-3;
  } else if (strcasecmp(line, "SIGRTMAX-2") == 0) {
    return SIGRTMAX-2;
  } else if (strcasecmp(line, "SIGRTMAX-1") == 0) {
    return SIGRTMAX-1;
  } else if (strcasecmp(line, "SIGRTMAX") == 0) {
    return SIGRTMAX;
  }
  return 0;
}

static Job * getJob(char * jobspec) {
  Job * job;
  if (*jobspec == '%') {
    job = get_job(substitute_jobspec(jobspec));
  } else {
    // Pid
    job = get_job(atoi(jobspec));
  }
  return job;
}

static void print_all_signals(void) {
  dprintf(out_fd, "%2d) %-12s", SIGHUP, "SIGHUP");
  dprintf(out_fd, "%2d) %-12s", SIGINT, "SIGINT");
  dprintf(out_fd, "%2d) %-12s", SIGQUIT, "SIGQUIT");
  dprintf(out_fd, "%2d) %-12s", SIGILL, "SIGILL");
  dprintf(out_fd, "%2d) %-12s\n", SIGTRAP, "SIGTRAP");

  dprintf(out_fd, "%2d) %-12s", SIGABRT, "SIGABRT");
  dprintf(out_fd, "%2d) %-12s", SIGBUS, "SIGBUS");
  dprintf(out_fd, "%2d) %-12s", SIGFPE, "SIGFPE");
  dprintf(out_fd, "%2d) %-12s", SIGKILL, "SIGKILL");
  dprintf(out_fd, "%2d) %-12s\n", SIGUSR1, "SIGUSR1");

  dprintf(out_fd, "%2d) %-12s", SIGSEGV, "SIGSEGV");
  dprintf(out_fd, "%2d) %-12s", SIGUSR2, "SIGUSR2");
  dprintf(out_fd, "%2d) %-12s", SIGPIPE, "SIGPIPE");
  dprintf(out_fd, "%2d) %-12s", SIGALRM, "SIGALRM");
  dprintf(out_fd, "%2d) %-12s\n", SIGTERM, "SIGTERM");

  dprintf(out_fd, "%2d) %-12s", SIGSTKFLT, "SIGSTKFLT");
  dprintf(out_fd, "%2d) %-12s", SIGCHLD, "SIGCHLD");
  dprintf(out_fd, "%2d) %-12s", SIGCONT, "SIGCONT");
  dprintf(out_fd, "%2d) %-12s", SIGSTOP, "SIGSTOP");
  dprintf(out_fd, "%2d) %-12s\n", SIGTSTP, "SIGTSTP");

  dprintf(out_fd, "%2d) %-12s", SIGTTIN, "SIGTTIN");
  dprintf(out_fd, "%2d) %-12s", SIGTTOU, "SIGTTOU");
  dprintf(out_fd, "%2d) %-12s", SIGURG, "SIGURG");
  dprintf(out_fd, "%2d) %-12s", SIGXCPU, "SIGXCPU");
  dprintf(out_fd, "%2d) %-12s\n", SIGXFSZ, "SIGXFSZ");

  dprintf(out_fd, "%2d) %-12s", SIGVTALRM, "SIGVTALRM");
  dprintf(out_fd, "%2d) %-12s", SIGPROF, "SIGPROF");
  dprintf(out_fd, "%2d) %-12s", SIGWINCH, "SIGWINCH");
  dprintf(out_fd, "%2d) %-12s", SIGIO, "SIGIO");
  dprintf(out_fd, "%2d) %-12s\n", SIGPWR, "SIGPWR");

  dprintf(out_fd, "%2d) %-12s", SIGSYS, "SIGSYS");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN, "SIGRTMIN");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+1, "SIGRTMIN+1");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+2, "SIGRTMIN+2");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMIN+3, "SIGRTMIN+3");

  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+4, "SIGRTMIN+4");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+5, "SIGRTMIN+5");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+6, "SIGRTMIN+6");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+7, "SIGRTMIN+7");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMIN+8, "SIGRTMIN+8");

  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+9, "SIGRTMIN+9");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+10, "SIGRTMIN+10");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+11, "SIGRTMIN+11");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+12, "SIGRTMIN+12");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMIN+13, "SIGRTMIN+13");

  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+14, "SIGRTMIN+14");
  dprintf(out_fd, "%2d) %-12s", SIGRTMIN+15, "SIGRTMIN+15");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-14, "SIGRTMAX-14");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-13, "SIGRTMAX-13");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMAX-12, "SIGRTMAX-12");

  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-11, "SIGRTMAX-11");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-10, "SIGRTMAX-10");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-9, "SIGRTMAX-9");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-8, "SIGRTMAX-8");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMAX-7, "SIGRTMAX-7");

  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-6, "SIGRTMAX-6");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-5, "SIGRTMAX-5");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-4, "SIGRTMAX-4");
  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-3, "SIGRTMAX-3");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMAX-2, "SIGRTMAX-2");

  dprintf(out_fd, "%2d) %-12s", SIGRTMAX-1, "SIGRTMAX-1");
  dprintf(out_fd, "%2d) %-12s\n", SIGRTMAX, "SIGRTMAX");
}

int kill_job(int argc, char *argv[], int stdout_fd, int stderr_fd) {
  argc--;argv++;
  int signal = SIGTERM;  // Default signal
  Job * job;

  out_fd = stdout_fd;
	err_fd = stderr_fd;

  if (!use_job_control) {
		return no_job_control(err_fd);
	}

  if (argc == 1) {
    // Use default signal
    if (strcmp(argv[0],"--help") == 0) {
			return help();
		}
    if (strcmp(argv[0],"-l") == 0 || strcmp(argv[0],"-L") == 0) {
      print_all_signals();
			return EXIT_SUCCESS;
		}
    job = getJob(argv[0]);
  } else if (argc == 2) {
    if (*argv[0] != '-') {
      return usage();
    }
    if (strcmp(argv[0],"-l") == 0 || strcmp(argv[0],"-L") == 0) {
      if (!(signal = getSignal(argv[1]))) {
        return invalid_signal(argv[1]);
      }
      dprintf(out_fd, "%d\n",signal);
			return EXIT_SUCCESS;
		}
    if (!(signal = getSignal(argv[0]))) {
      return invalid_signal(argv[0]);
    }
    job = getJob(argv[1]);
  } else if (argc == 3) {
    if (strcmp(argv[0],"-s") == 0) {
      if (!(signal = getSignal(argv[1]))) {
        return invalid_signal(argv[1]);
      }
    } else if (strcmp(argv[0],"-n") == 0) {
      if ((signal = atoi(argv[1])) <= 0) {
        return invalid_signal(argv[1]);
      }
    } else {
      return usage();
    }
    job = getJob(argv[2]);
  } else {
    return usage();
  }

  if (job == NULL) {
    return no_job("kill", err_fd); 
  }

  kill(job->pid, signal);
  
  if (signal == SIGTSTP && signal != SIGSTOP && signal != SIGTTIN && signal != SIGTTOU) {
    stop_job(job->pid);
  } else if (signal == SIGCONT) {
    job->state = RUNNING;
    dprintf(out_fd, "%s\n", job->command);
  } else if (signal != SIGCHLD) {
    remove_job(job);
  }
  
  return EXIT_SUCCESS;
}
