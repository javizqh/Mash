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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/source.h"
#include "parse_line.h"
#include "show_prompt.h"

int
prompt(int mode)
{
	char *result = getenv("result");

	if (mode == INTERACTIVE_MODE) {
		char *prompt = getenv("PROMPT");

		parse_prompt(prompt);
		fflush(stdout);
	}
	add_env_by_name("result", result);
	return 1;
}

int
parse_prompt(char *prompt)
{
	int match = 0;
	char *token;
	char *rest = malloc(1024);

	if (rest == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(rest, 0, 1024);
	strcpy(rest, prompt);
	char *rest_start = rest;

	while ((token = strtok_r(rest, "@", &rest))) {
		//printf("%s", token);
		if (strstr(token, "user") == token) {
			token += strlen("user");
			printf("%s%s", getpwuid(getuid())->pw_name, token);
			match = 1;
		} else if (strstr(token, "green") == token) {
			token += strlen("green");
			printf("\033[01;32m%s", token);
			match = 1;
		} else if (strstr(token, "pink") == token) {
			token += strlen("pink");
			printf("\033[01;35m%s", token);
			match = 1;
		} else if (strstr(token, "blue") == token) {
			token += strlen("blue");
			printf("\033[01;34m%s", token);
			match = 1;
		} else if (strstr(token, "nocolor") == token) {
			token += strlen("nocolor");
			printf("\033[0m%s", token);
			match = 1;
		} else if (strstr(token, "branch") == token) {
			//FIX: solve error message and space issue
			// Execute this git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/(\1)/'
			char *buffer = malloc(sizeof(char) * 1024);

			if (buffer == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(buffer, 0, sizeof(char) * 1024);

			if (find_command
			    ("git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \\(.*\\)/(\\1)/'",
			     buffer, stdin) == 0) {

				strtok(buffer, "\n");
				token += strlen("branch");
				printf("%s%s", buffer, token);
			} else {
				printf("%s", token);
			}

			free(buffer);
			match = 1;
		} else if (strstr(token, "where") == token) {
			char *cwd = getenv("PWD");
			char *tmp = malloc(1024);
			char *tmp_s = tmp;

			if (tmp == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(tmp, 0, 1024);
			strcpy(tmp, cwd);
			// FIX: read from HOME
			if (strstr(tmp, "/home/javier") == tmp) {
				tmp += strlen("/home/javier");
				*--tmp = '~';
			}

			token += strlen("where");
			printf("%s%s", tmp, token);
			free(tmp_s);
			match = 1;
		} else if (strstr(token, "host") == token) {
			char hostname[1024];

			hostname[1023] = '\0';
			gethostname(hostname, 1023);

			token += strlen("host");
			printf("%s%s", hostname, token);
			match = 1;
		}
		if (*rest == '@') {
			printf("@");
		}
		if (match) {
			match = 0;
		} else {
			printf("@%s", token);
		}
	}

	free(rest_start);
	return 1;
}
