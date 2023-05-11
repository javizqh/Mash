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

#include <sys/wait.h>
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
#include "builtin/wait.h"

static int usage() {
  fprintf(stderr,"Usage: wait [jobspec or pid]\n");
	return EXIT_FAILURE;
}

int wait_for_job(int argc, char *argv[]) {
  argc--;argv++;
  Job * job;

  if (argc == 0) {
    job = get_job(get_relevance_job_pid(0));
  } else if (argc == 1) {
    if (*argv[0] == '%') {
      job = get_job(substitute_jobspec(argv[0]));
    } else {
      // Pid
      job = get_job(atoi(argv[0]));
    }
  } else {
    return usage();
  }

  if (job == NULL) {
    return no_job("wait"); 
  }

  if (job->state == STOPPED) {
    fprintf(stderr,"mash: warning: wait_for_job: job is stopped\n");
    return EXIT_FAILURE;
  }

  wait_job(job);
  
  return EXIT_SUCCESS;
}
