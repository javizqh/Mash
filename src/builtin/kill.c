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

char * kill_use = "kill [-s sigspec] [-n signum] [-sigspec] jobspec or pid";

static int usage() {
  fprintf(stderr,"Usage: %s\n",kill_use);
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

int kill_job(int argc, char *argv[]) {
  argc--;argv++;
  int signal = SIGTERM;  // Default signal
  Job * job;

  if (argc == 1) {
    // Use default signal
    job = get_job(get_relevance_job_pid(0));
  } else if (argc == 2) {
    if (*argv[0] != '-') {
      return usage();
    }
    signal = getSignal(argv[0]);
    job = getJob(argv[1]);
  } else if (argc == 3) {
    if (strcmp(argv[0],"-s") == 0) {
      if (!(signal = getSignal(argv[1]))) {
        return usage();
      }
    } else if (strcmp(argv[0],"-n") == 0) {
      if (!(signal = atoi(argv[1]))) {
        return usage();
      }
    } else {
      return usage();
    }
    job = getJob(argv[2]);
  } else {
    return usage();
  }

  if (job == NULL) {
    return no_job("kill"); 
  }

  kill(job->pid, signal);
  
  if (signal == SIGTSTP && signal != SIGSTOP && signal != SIGTTIN && signal != SIGTTOU) {
    stop_job(job->pid);
  } else if (signal == SIGCONT) {
    job->state = RUNNING;
    printf("%s\n", job->command);
  } else if (signal != SIGCHLD) {
    remove_job(job);
  }
  
  return EXIT_SUCCESS;
}
