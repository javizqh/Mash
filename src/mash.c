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

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <pwd.h>
#include <stdlib.h>
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

int
main(int argc, char **argv)
{
	int status;

	argc--;
	argv++;
	// TODO: add proper check
	if (argc == 1 && strcmp(argv[0], "-i") == 0) {
		shell_mode = INTERACTIVE_MODE;
	}
	if (ftell(stdin) >= 0) {
		reading_from_file = 1;
	}

	signal(SIGINT, sig_handler);
	signal(SIGTSTP, sig_handler);

	load_lex_tables();
	init_jobs_list();
	add_source("env/.mashrc");
	exec_sources();

	// TODO: check for errors
	add_env_by_name("HOME", getpwuid(getuid())->pw_dir);
	char cwd[MAX_ENV_SIZE];

	if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
		// TODO: error, load from home
	}
	add_env_by_name("PWD", cwd);

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

void
sig_handler(int sig)
{
	switch (sig) {
	case SIGINT:
		end_current_job();
		break;
	case SIGTSTP:
		stop_current_job();
		break;
	default:
		break;
	}
}
