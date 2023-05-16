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
#include "builtin/exit.h"
#include "builtin/export.h"

// DECLARE GLOBAL VARIABLE
char *exit_use = "exit [n]";
char *exit_description = "Exit the shell.";
char *exit_help =
    "    Exits the shell with a status of N.  If N is omitted, the exit status\n"
    "    is that of the last command executed.\n";
int has_to_exit = 0;

static int out_fd;
static int err_fd;

static int
help()
{
	dprintf(out_fd, "exit: %s\n", exit_use);
	dprintf(out_fd, "    %s\n\n%s", exit_description, exit_help);
	return EXIT_SUCCESS;
}

static int
usage()
{
	dprintf(err_fd, "Usage: %s\n", exit_use);
	return EXIT_FAILURE;
}

int
exit_mash(int argc, char *argv[], int stdout_fd, int stderr_fd)
{
	argc--;
	argv++;

	char *result = get_env_by_name("result");
	int exit_status;

	out_fd = stdout_fd;
	err_fd = stderr_fd;

	if (result == NULL) {
		dprintf(err_fd, "error: var result does not exist\n");
		exit_status = EXIT_FAILURE;
	} else {
		exit_status = atoi(result);
		free(result);
	}

	if (argc == 1) {
		if (strcmp(argv[0], "--help") == 0) {
			return help();
		}
		exit_status = atoi(argv[0]);
		if (exit_status < 0) {
			exit_status = 0;
		}
	} else if (argc > 1) {
		return usage();
	}

	has_to_exit = 1;
	return exit_status;
}
