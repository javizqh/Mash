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
#include "builtin/disown.h"

char * disown_use = "disown [-ar] [jobspec … | pid … ]";
char * disown_description = "Remove jobs from current shell.";
char * disown_help = 
"    Removes each JOBSPEC argument from the table of active jobs. Without\n"
"    any JOBSPECs, the shell uses its notion of the current job.\n\n"
"    Options:\n"
"      -a    remove all jobs if JOBSPEC is not supplied\n"
"      -r    remove only running jobs\n\n"
"    Exit Status:\n"
"    Returns success unless an invalid option or JOBSPEC is given, or job control is not enabled.\n";

static int help() {
	printf("disown: %s\n", disown_use);
	printf("    %s\n\n%s", disown_description, disown_help);
	return EXIT_SUCCESS;
}

static int usage() {
  fprintf(stderr,"Usage: %s\n",disown_use);
	return EXIT_FAILURE;
}

int disown(int argc, char *argv[]) {
  argc--;argv++;
  Job * job;

  if (argc == 0) {
    job = get_job(get_relevance_job_pid(0));
  } else if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
			return help();
		}
    if (strcmp(argv[0], "-a") == 0) {
      // Remove all
      if (remove_all_status_jobs(ALL)) {
        return EXIT_SUCCESS;
      } else {
        return EXIT_FAILURE;
      }
    } else if (strcmp(argv[0],"-r") == 0) {
      // Remove only running
      if (remove_all_status_jobs(RUNNING)) {
        return EXIT_SUCCESS;
      } else {
        return EXIT_FAILURE;
      }
    } else if (*argv[0] == '%') {
      job = get_job(substitute_jobspec(argv[0]));
    } else {
      // Pid
      job = get_job(atoi(argv[0]));
    }
  } else {
    return usage();
  }

  if (job == NULL) {
    return no_job("disown"); 
  }

  remove_job(job);
  
  return EXIT_SUCCESS;
}
