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
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/source.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "builtin/exit.h"
#include "exec_cmd.h"
#include "mash.h"
#include "builtin/jobs.h"

int reading_from_file = 0;
int writing_to_file = 0;
char flags[2];
char version[32] = "1.1.0";

static void
usage()
{
	fprintf(stderr, "Usage: mash [-i]\n");
	exit(EXIT_FAILURE);
}

static void
help()
{
	printf("Mash, version %s\n", version);
	printf("Usage: mash [-i]\n\n");
	printf("Options:\n\t-i\tInteractive mode\n\n");
	printf
	    ("Enter mash and type `help' for more information about shell builtin commands.\n\n");
	printf("Mash source code: <https://github.com/javizqh/Mash>\n");
	exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
	int status;

	argc--;
	argv++;

	if (argc == 1 && strcmp(argv[0], "--help") == 0) {
		help();
	}

	set_arguments(argv);
	init_mash();

	// ---------- Read command line
	// ------ Buffer
	char *buf = malloc(MAX_ARGUMENT_SIZE);

	if (buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(buf, 0, MAX_ARGUMENT_SIZE);
	// ------------
	prompt(buf);
	while (fgets(buf, MAX_ARGUMENT_SIZE, stdin) != NULL) {	/* break with ^D or ^Z */
		status = find_command(buf);

		if (has_to_exit) {
			break;
		}
		prompt();
	}

	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}

	if (!has_to_exit) {
		exit_mash(0, NULL, STDOUT_FILENO, STDERR_FILENO);
	}
	free(buf);
	return status;
}

static void
set_flag_string()
{
	if (shell_mode == INTERACTIVE_MODE) {
		flags[0] = 'i';
	} else {
		flags[0] = '\0';
	}

	flags[1] = '\0';
	return;
}

int
set_arguments(char *argv[])
{
	char *arg_ptr;

	for (; *argv != NULL; argv++) {
		if (*argv[0] == '-') {
			arg_ptr = argv[0];
			arg_ptr++;
			for (; *arg_ptr != '\0'; arg_ptr++) {
				switch (*arg_ptr) {
				case 'i':
					shell_mode = INTERACTIVE_MODE;
					break;
				default:
					usage();
					break;
				}
			}
		} else {
			usage();
		}
	}
	set_flag_string();
	return 1;
}

int
init_mash()
{
	char cwd[MAX_ENV_SIZE];

	if (!isatty(0)) {
		reading_from_file = 1;
	}

	if (!isatty(1)) {
		writing_to_file = 1;
	}

	load_basic_lex_tables();

	add_env_by_name("HOME", getpwuid(getuid())->pw_dir);

	if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
		exit_mash(0, NULL, STDOUT_FILENO, STDERR_FILENO);
		err(EXIT_FAILURE, "error getting current working directory");
	}
	add_env_by_name("PWD", cwd);
	add_env_by_name("result", "0");

	return 1;
}

void
sig_handler(int sig)
{
	// Do nothing
	switch (sig) {
	default:
		break;
	}
}
