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

struct alias *
new_alias(const char *command, const char *reference)
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
add_alias(const char *line, struct alias **aliases)
{
	char *p, *eol;
	int index;

	p = strchr(line, '=');
	if (p != NULL) {
		*p = '\0';
		// Remove '\n'
		eol = strchr(++p, '\n');
		if (eol != NULL) {
			*eol = '\0';
		}
		struct alias *alias = new_alias(line, p);

		for (index = 0; index < ALIAS_MAX; index++) {
			if (aliases[index] == NULL) {
				aliases[index] = alias;
				return 0;
			}
		}
		perror("Failed to add new alias. Already at limit.\n");
	} else {
		perror("Failed to add new alias. Failed to found =.\n");
	}
	return 1;
}

char* get_alias(const char* name, struct alias **aliases) {
	int i;
	// TODO:
	for (i = 0; i < 2; i++) {
		if (strcmp(aliases[i]->command,name) == 0) {
			return aliases[i]->reference;
		}
	}
	return NULL;
}

struct alias **
init_aliases(const char *alias_file)
{
	// Open and read env file
	FILE *fd_env = fopen(alias_file, "r");

	// Create Aliases array
	struct alias **aliases =
	    (struct alias **)malloc(ALIAS_MAX * sizeof(struct alias *));

	// Check if malloc failed
	if (aliases == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	// Initialize buffer to 0
	memset(aliases, 0, ALIAS_MAX);

	// Create Buffer
	char line[LINE_SIZE];

	// ----------- Read file and write
	char *p;

	while (fgets(line, sizeof(line), fd_env)) {
		/* note that fgets don't strip the terminating \n, checking its
		   presence would allow to handle lines longer that sizeof(line) */
		// Check if contains alias
		p = strstr(line, "alias");
		if (p != NULL) {
			// If doesn't contain alias
			add_alias(line + 6, aliases);
		}
	}
	// -----------------------------------------

	if (fclose(fd_env)) {
		err(EXIT_FAILURE, "close failed");
	}
	return aliases;
}

int set_alias_in_cmd(struct command *command, struct alias **aliases);
