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
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "builtin/jobs.h"
#include "builtin/bg.h"

static int usage() {
  fprintf(stderr,"Usage: bg [jobspec]\n");
	return EXIT_FAILURE;
}

int bg(int argc, char *argv[]) {
  argc--;argv++;
  char relevance;
  Job * job;

  if (argc == 0) {
    job = get_job(get_relevance_job_pid(0));
  } else if (argc == 1) {
    job = get_job(substitute_jobspec(argv[0]));
    if (job == NULL) {
      return usage(); 
    }
  } else {
    return usage();
  }

	switch (job->relevance) {
	case 0:
		relevance = '+';
		break;
	case 1:
		relevance = '-';
		break;
	default:
		relevance = ' ';
		break;
	}
  printf("[%d]%c\t%s\n",job->pos,relevance,job->command);

  if (job->execution == FOREGROUND) {
    // BUG: only stop if reading from stdin
    stop_job(job->pid);
    return EXIT_SUCCESS;
  }
  job->state = RUNNING;
  kill(job->pid, SIGCONT);
  return EXIT_SUCCESS;
}