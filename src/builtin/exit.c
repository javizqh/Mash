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
#include <stdio.h>
#include <stdlib.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "builtin/jobs.h"
#include "builtin/exit.h"

// DECLARE GLOBAL VARIABLE
char * exit_use = "exit [n]";
char * exit_description = "Exit the shell.";
char * exit_help = 
"    Exits the shell with a status of N.  If N is omitted, the exit status\n"
"    is that of the last command executed.\n";
int has_to_exit = 0;

static int help() {
	printf("exit: %s\n", exit_use);
	printf("    %s\n\n%s", exit_description, exit_help);
	return EXIT_SUCCESS;
}

static int usage() {
	fprintf(stderr,"Usage: %s\n",exit_use);
	return EXIT_FAILURE;
}

int
exit_mash(int argc, char* argv[])
{
	int i;
	int exit_status = atoi(getenv("result"));

	argc--;argv++;
	
	if (argc == 1) {
		if (strcmp(argv[0],"--help") == 0) {
			return help();
		}
		exit_status = atoi(argv[0]);
		if ( exit_status < 0) {
			exit_status = 0;
		}	
	} else if (argc > 1) {
		return usage();
	}

  for (i = 0; i < ALIAS_MAX; i++) {
		if (aliases[i] == NULL) break;
		free(aliases[i]);
	}

  free_source_file();

	free_jobs_list();

	has_to_exit = 1;
  return exit_status;
}