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
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/source.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "mash.h"

int shell_mode = NON_INTERACTIVE;

void
set_prompt_mode(int mode)
{
	shell_mode = mode;
}

int
prompt(char *line)
{
	char *result = getenv("result");
	char *prompt = getenv("PROMPT");

	if (shell_mode == INTERACTIVE_MODE) {
		if (syntax_mode == BASIC_SYNTAX) {
			printf("$ ");
		} else {
			if (prompt) {
				parse_prompt(prompt, line);
			} else {
				printf("$ ");
			}
		}
		fflush(stdout);
	}

	if (writing_to_file) {
		printf("\n");
		fflush(stdout);
	}

	add_env_by_name("result", result);
	return 1;
}

int
prompt_request()
{
	if (shell_mode == INTERACTIVE_MODE) {
		if (ftell(stdout) < 0) {
			printf("> ");
		}
		fflush(stdout);
	}
	return 1;
};

int
parse_prompt(char *prompt, char *line)
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
		if (strstr(token, "ifcustom") == token) {
			token += strlen("ifcustom");
			struct stat buf;

			if (stat(".mash_prompt", &buf) == 0) {
				printf("%s", token);
			} else {
				while ((token = strtok_r(rest, "@", &rest))) {
					if (strstr(token, "else") == token) {
						token += strlen("else");
						printf("%s", token);
						break;
					} else if (strstr(token, "endif") ==
						   token) {
						token += strlen("endif");
						printf("%s", token);
						break;
					}
				}
			}
			match = 1;
		} else if (strstr(token, "ifgit") == token) {
			fflush(stdout);
			char *buffer = malloc(1024);

			if (buffer == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(buffer, 0, 1024);

			strcpy(line, "git branch 2> /dev/null");

			token += strlen("ifgit");

			if (find_command(line, buffer, stdin, NULL, rest_start)
			    == 0) {
				printf("%s", token);
			} else {
				while ((token = strtok_r(rest, "@", &rest))) {
					if (strstr(token, "else") == token) {
						token += strlen("else");
						printf("%s", token);
						break;
					} else if (strstr(token, "endif") ==
						   token) {
						token += strlen("endif");
						printf("%s", token);
						break;
					}
				}
			}
			free(buffer);
			match = 1;
		} else if (strstr(token, "else") == token) {
			while ((token = strtok_r(rest, "@", &rest))) {
				if (strstr(token, "endif") == token) {
					token += strlen("endif");
					printf("%s", token);
					break;
				}
			}
			match = 1;
		} else if (strstr(token, "endif") == token) {
			token += strlen("endif");
			printf("%s", token);
			match = 1;
		} else if (strstr(token, "user") == token) {
			token += strlen("user");
			printf("%s%s", getpwuid(getuid())->pw_name, token);
			match = 1;
		} else if (strstr(token, "-") == token) {
			token += strlen("-");
			printf("%s", token);
			match = 1;
		} else if (strstr(token, "gitstatuscolor") == token) {
			fflush(stdout);
			token += strlen("gitstatuscolor");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				char *buffer = malloc(1024);

				if (buffer == NULL)
					err(EXIT_FAILURE, "malloc failed");
				memset(buffer, 0, 1024);

				strcpy(line,
				       "git status --porcelain 2> /dev/null | wc -l");

				find_command(line, buffer, stdin, NULL,
					     rest_start);

				strtok(buffer, "\n");
				if (atoi(buffer) > 0) {
					printf("\033[01;31m%s", token);
				} else {
					printf("\033[01;32m%s", token);
				}

				free(buffer);
			}
			match = 1;
		} else if (strstr(token, "gitstatus") == token) {
			fflush(stdout);
			char *buffer = malloc(1024);

			if (buffer == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(buffer, 0, 1024);

			strcpy(line,
			       "git status --porcelain 2> /dev/null | wc -l");

			token += strlen("gitstatus");
			find_command(line, buffer, stdin, NULL, rest_start);

			strtok(buffer, "\n");
			if (atoi(buffer) > 0) {
				printf("|%s%s", buffer, token);
			} else {
				printf("%s", token);
			}

			free(buffer);
			match = 1;
		} else if (strstr(token, "black") == token) {
			token += strlen("black");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;30m%s", token);
			}
			match = 1;
		} else if (strstr(token, "red") == token) {
			token += strlen("red");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;31m%s", token);
			}
			match = 1;
		} else if (strstr(token, "green") == token) {
			token += strlen("green");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;32m%s", token);
			}
			match = 1;
		} else if (strstr(token, "yellow") == token) {
			token += strlen("yellow");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;33m%s", token);
			}
			match = 1;
		} else if (strstr(token, "blue") == token) {
			token += strlen("blue");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;34m%s", token);
			}
			match = 1;
		} else if (strstr(token, "pink") == token) {
			token += strlen("pink");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;35m%s", token);
			}
			match = 1;
		} else if (strstr(token, "cyan") == token) {
			token += strlen("cyan");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;36m%s", token);
			}
			match = 1;
		} else if (strstr(token, "white") == token) {
			token += strlen("white");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[01;37m%s", token);
			}
			match = 1;
		} else if (strstr(token, "nocolor") == token) {
			token += strlen("nocolor");
			if (writing_to_file) {
				printf("%s", token);
			} else {
				printf("\033[0m%s", token);
			}
			match = 1;
		} else if (strstr(token, "branch") == token) {
			fflush(stdout);
			char *buffer = malloc(1024);

			if (buffer == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(buffer, 0, 1024);

			strcpy(line,
			       "git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \\(.*\\)/\\1/'");

			token += strlen("branch");
			find_command(line, buffer, stdin, NULL, rest_start);

			strtok(buffer, "\n");
			if (strlen(buffer) > 0) {
				printf("%s%s", buffer, token);
			} else {
				printf("%s", token);
			}

			free(buffer);
			match = 1;
		} else if (strstr(token, "where") == token) {
			char *cwd = getenv("PWD");

			if (cwd == NULL) {
				match = 1;
				break;
			}
			char *tmp = malloc(1024);
			char *tmp_s = tmp;

			if (tmp == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(tmp, 0, 1024);
			strcpy(tmp, cwd);

			char *home = get_env_by_name("HOME");

			if (home == NULL) {
				match = 1;
				free(tmp_s);
				break;
			}
			if (strstr(tmp, home) == tmp) {
				tmp += strlen(home);
				*--tmp = '~';
			}

			token += strlen("where");
			printf("%s%s", tmp, token);
			free(home);
			free(tmp_s);
			match = 1;
		} else if (strstr(token, "host") == token) {
			char hostname[1024];

			hostname[1023] = '\0';
			gethostname(hostname, 1023);

			token += strlen("host");
			printf("%s%s", hostname, token);
			match = 1;
		} else if (strstr(token, "custom") == token) {
			fflush(stdout);
			token += strlen("custom");
			FILE *file_prompt = fopen(".mash_prompt", "r");
			char *buffer = malloc(1024);

			if (buffer == NULL)
				err(EXIT_FAILURE, "malloc failed");
			memset(buffer, 0, 1024);
			char *orig_buffer = buffer;

			fgets(buffer, 1024, file_prompt);
			char *tmp = malloc(1024);

			if (tmp == NULL) {
				err(EXIT_FAILURE, "malloc failed");
			}
			memset(tmp, 0, 1024);
			if (strstr(buffer, "@") == buffer) {
				buffer++;
			}
			strcpy(tmp, buffer);
			strcat(tmp, "@");
			strcat(tmp, rest);
			strcpy(rest, tmp);
			free(tmp);
			if (ferror(stdin)) {
				err(EXIT_FAILURE, "fgets failed");
			}
			fclose(file_prompt);
			free(orig_buffer);
			match = 1;
		}
		if (*rest == '@') {
			printf("@");
		}
		if (match) {
			match = 0;
		} else {
			printf("%s", token);
		}
	}

	free(rest_start);
	return 1;
}
