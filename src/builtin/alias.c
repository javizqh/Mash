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

struct alias *aliases[ALIAS_MAX];

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
		struct alias *alias = new_alias(command, p);

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
