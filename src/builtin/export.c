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

#include <err.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin/export.h"

extern char **environ;
char *export_use = "export [name=value]";
char *export_description = "Set export attribute for shell variables.";
char *export_help =
    "    Marks each NAME for automatic export to the environment of subsequently\n"
    "    executed commands.  If VALUE is supplied, assign VALUE before exporting.\n\n"
    "    Exit Status:\n"
    "    Returns success unless an invalid option is given or NAME is invalid.\n";

static int out_fd;
static int err_fd;

static int
help()
{
	dprintf(out_fd, "export: %s\n", export_use);
	dprintf(out_fd, "    %s\n\n%s", export_description, export_help);
	return EXIT_SUCCESS;
}

static int
usage()
{
	dprintf(err_fd, "Usage: %s\n", export_use);
	return EXIT_FAILURE;
}

int
export(int argc, char *argv[], int stdout_fd, int stderr_fd)
{
	argc--;
	argv++;
	int exit_value = 0;

	out_fd = stdout_fd;
	err_fd = stderr_fd;

	if (argc == 0) {
		print_env();
	} else if (argc == 1) {
		if (strcmp(argv[0], "--help") == 0) {
			return help();
		}
		exit_value = add_env(argv[0]);
	} else {
		return usage();
	}
	return exit_value;
}

int
add_env(const char *line)
{
	char *p = strchr(line, '=');

	if (p != NULL) {
		*p = '\0';
		// Remove \n
		char *eol = strchr(++p, '\n');

		if (eol != NULL) {
			*eol = '\0';
		}

		if (strlen(p) <= 0) {
			return usage();
		}
		// Set environment variables
		setenv(line, p, 1);
		return 0;
	}
	return 1;
}

int
add_env_by_name(const char *key, const char *value)
{
	return setenv(key, value, 1);
}

char *
get_env_by_name(const char *key)
{
	char *env = malloc(MAX_ENV_SIZE);

	if (env == NULL) {
		err(EXIT_FAILURE, "error maloc failed");
	}
	memset(env, 0, MAX_ENV_SIZE);
	char *ret = getenv(key);

	if (ret == NULL) {
		free(env);
		return NULL;
	}
	strcpy(env, ret);
	return env;
}

void
print_env()
{
	char **s = environ;

	for (; *s; s++) {
		dprintf(out_fd, "%s\n", *s);
	}
}
