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
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/source.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "builtin/exit.h"
#include "mash.h"

int
main(int argc, char **argv)
{
	argc--;
	argv++;
	// TODO: add proper check
	if (argc == 1 && strcmp(argv[0], "-i") == 0) {
		shell_mode = INTERACTIVE_MODE;
	}

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
	char *buf = malloc(1024);

	if (buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(buf, 0, 1024);
	// ------------
	prompt(buf);
	while (fgets(buf, 1024, stdin) != NULL) {	/* break with ^D or ^Z */
		find_command(buf, NULL, stdin, NULL);

		if (has_to_exit)
			break;

		prompt(buf);
	}

	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}

	free(buf);
	return 0;
}
