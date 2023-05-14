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

#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include "builtin/alias.h"

// DECLARE STATIC FUNCTION
static int usage();
static int out_fd;
static int err_fd;

// DECLARE GLOBAL VARIABLE
struct alias *aliases[ALIAS_MAX];
char * alias_use = "alias [name=value]";
char * alias_description = "Define or display aliases.";
char * alias_help = 
"    Without arguments, `alias' prints the list of aliases in the reusable\n"
"    form `alias NAME=VALUE' on standard output.\n\n"
"    Otherwise, an alias is defined for each NAME whose VALUE is given.\n\n"
"    Exit Status:\n"
"    alias returns 0 unless a VALUE is missing or the is out of memory.\n";

static int help() {
	dprintf(out_fd, "alias: %s\n", alias_use);
	dprintf(out_fd, "    %s\n\n%s", alias_description, alias_help);
	return EXIT_SUCCESS;
}

static int usage() {
	dprintf(err_fd,"Usage: %s\n", alias_use);
	return EXIT_FAILURE;
}

int alias(int argc, char *argv[], int stdout_fd, int stderr_fd) {
	argc--;argv++;
	int exit_value = EXIT_SUCCESS;
	out_fd = stdout_fd;
	err_fd = stderr_fd;
	if (argc == 0) {
		print_aliases();
	} else if (argc == 1) {
		if (strcmp(argv[0],"--help") == 0) {
			return help();
		}
		exit_value = add_alias(argv[0]);
		if (exit_value < 0) {
			return usage();
		}
	} else {
		return usage();
	}
	return exit_value;
}
struct alias *
new_alias(const char *command, char *reference)
{
	struct alias *alias = (struct alias *)malloc(sizeof(struct alias));
	// Check if malloc failed
	if (alias == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	strcpy(alias->command, command);
	strcpy(alias->reference, reference);

	return alias;
}

int
add_alias(char *command)
{
	char *p, *eol;
	int index;

	p = strchr(command, '=');
	if (p != NULL) {
		*p = '\0';
		// Remove '\n'
		eol = strchr(++p, '\n');
		if (eol != NULL) {
			*eol = '\0';
		}

    if (strlen(p) <= 0) {
      return -1;
    }

		struct alias *alias = new_alias(command, p);

		for (index = 0; index < ALIAS_MAX; index++) {
			if (aliases[index] == NULL) {
				aliases[index] = alias;
				return 0;
			}
		}
		dprintf(err_fd,"Failed to add new alias. Already at limit.\n");
		return 1;
	}
	return -1;
}

char* get_alias(const char* name) {
	int i;
	for (i = 0; i < ALIAS_MAX; i++) {
		if (aliases[i] == NULL) break;
		if (strcmp(aliases[i]->command,name) == 0) {
			return aliases[i]->reference;
		}
	}
	return NULL;
}

void print_aliases() {
	int i;
	for (i = 0; i < ALIAS_MAX; i++) {
		if (aliases[i] == NULL) break;
		dprintf(out_fd, "alias %s=%s\n",aliases[i]->command,aliases[i]->reference);
	}
}
