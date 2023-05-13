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

static void
usage()
{
	fprintf(stderr, "Usage: mash [-ibej]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int status;

	argc--;
	argv++;

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
		status = find_command(buf, NULL, stdin, NULL, NULL);

		if (has_to_exit) {
			break;
		}
		update_jobs();
		prompt(buf);
	}

	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}

	if (!has_to_exit) {
		exit_mash(0, NULL);
	}
	free(buf);
	return status;
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
				case 'b':
					syntax_mode = BASIC_SYNTAX;
					use_jobs = 0;
					break;
				case 'e':
					syntax_mode = EXTENDED_SYNTAX;
					use_jobs = 1;
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

	if (syntax_mode == BASIC_SYNTAX) {
		load_basic_lex_tables();
	} else {
		signal(SIGINT, sig_handler);
		signal(SIGTSTP, sig_handler);
		load_lex_tables();
		add_source("env/.mashrc");
		exec_sources();
	}

	if (use_jobs) {
		init_jobs_list();
	}

	add_env_by_name("HOME", getpwuid(getuid())->pw_dir);

	if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
		exit_mash(0, NULL);
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
