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

char * bg_use = "bg [jobspec]";
char * bg_description = "Move jobs to the background.";
char * bg_help = 
"    Place the jobs identified by the JOB_SPEC in the background, as if they\n"
"    had been started with `&'. If JOB_SPEC is not present, the shell's notion\n"
"    of the current job is used.\n\n"
"    Exit Status:\n"
"    Returns success unless job control is not enabled or an error occurs.\n";

static int out_fd;
static int err_fd;

static int help() {
	dprintf(out_fd, "bg: %s\n", bg_use);
	dprintf(out_fd, "    %s\n\n%s", bg_description, bg_help);
	return EXIT_SUCCESS;
}

static int usage() {
  dprintf(err_fd,"Usage: %s\n",bg_use);
	return EXIT_FAILURE;
}

int bg(int argc, char *argv[], int stdout_fd, int stderr_fd) {
  argc--;argv++;
  char relevance;
  Job * job;

	out_fd = stdout_fd;
	err_fd = stderr_fd;

  if (!use_job_control) {
		return no_job_control(err_fd);
	}

  if (argc == 0) {
    job = get_job(get_relevance_job_pid(0));
  } else if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
			return help();
		}
    job = get_job(substitute_jobspec(argv[0]));
  } else {
    return usage();
  }

  if (job == NULL) {
    return no_job("bg", err_fd); 
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
  dprintf(out_fd, "[%d]%c\t%s\n",job->pos,relevance,job->command);

  job->state = RUNNING;
  kill(job->pid, SIGTTOU);
  kill(job->pid, SIGCONT);

  int wstatus;
	pid_t wait_pid;

  wait_pid = waitpid(job->pid,&wstatus,WNOHANG|WUNTRACED);
  if (wait_pid == -1) {
    perror("waitpid failed 2");
  }
  
  if (WIFSTOPPED(wstatus) || WIFSIGNALED(wstatus)) {
    stop_job(job->pid);
    waitpid(job->pid,&wstatus,WUNTRACED);
  }
  return EXIT_SUCCESS;
}