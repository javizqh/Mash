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

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "builtin/cd.h"
#include "builtin/command.h"
#include "builtin/echo.h"
#include "builtin/exit.h"
#include "builtin/export.h"
#include "builtin/ifnot.h"
#include "builtin/ifok.h"
#include "builtin/mash_math.h"
#include "builtin/mash_pwd.h"
#include "builtin/sleep.h"
#include "parse.h"
#include "exec_info.h"
#include "builtin/builtin.h"
#include "builtin/help.h"
#include "mash.h"

char *help_use = "help [-dms] [pattern ...]";
char *help_description = "Display information about builtin commands.";
char *help_help =
    "    Displays brief summaries of builtin commands.  If PATTERN is\n"
    "    specified, gives detailed help on all commands matching PATTERN,\n"
    "    otherwise the list of help topics is printed.\n\n"
    "    Options:\n"
    "      -d       output short description for each topic\n"
    "      -m       display usage in pseudo-manpage format\n"
    "      -s       output only a short usage synopsis for each topic matching PATTERN\n\n"
    "    Arguments:\n"
    "      PATTERN	Pattern specifying a help topic\n\n"
    "    Exit Status:\n"
    "    Returns success unless PATTERN is not found or an invalid option is given.\n";

static int
print_help()
{
	printf("help: %s\n", help_use);
	printf("    %s\n\n%s", help_description, help_help);
	return EXIT_SUCCESS;
}

static int
usage()
{
	fprintf(stderr, "Usage: %s\n", help_use);
	return EXIT_FAILURE;
}

static void
print_help_header()
{
	printf("mash, version %s\n"
	       "These shell commands are defined internally.  Type `help' to see this list.\n"
	       "Type `help name' to find out more about the function `name'.\n"
	       "Use `info mash' to find out more about the shell in general.\n"
	       "Use `man -k' or `info' to find out more about commands not in this list.\n\n",
	       version);
}

static int
print_usage(char *name)
{
	int matched = 0;

	if (name == NULL) {
		printf("$(( expression ))\n");
		matched++;
	}
	if (name == NULL || strncmp("builtin", name, strlen(name)) == 0) {
		printf("builtin: %s\n", builtin_use);
		matched++;
	}
	if (name == NULL || strncmp("cd", name, strlen(name)) == 0) {
		printf("cd: %s\n", cd_use);
		matched++;
	}
	if (name == NULL || strncmp("command", name, strlen(name)) == 0) {
		printf("command: %s\n", command_use);
		matched++;
	}
	if (name == NULL || strncmp("echo", name, strlen(name)) == 0) {
		printf("echo: %s\n", echo_use);
		matched++;
	}
	if (name == NULL || strncmp("exit", name, strlen(name)) == 0) {
		printf("exit: %s\n", exit_use);
		matched++;
	}
	if (name == NULL || strncmp("export", name, strlen(name)) == 0) {
		printf("export: %s\n", export_use);
		matched++;
	}
	if (name == NULL || strncmp("help", name, strlen(name)) == 0) {
		printf("help: %s\n", help_use);
		matched++;
	}
	if (name == NULL || strncmp("ifnot", name, strlen(name)) == 0) {
		printf("ifnot: %s\n", ifnot_use);
		matched++;
	}
	if (name == NULL || strncmp("ifok", name, strlen(name)) == 0) {
		printf("ifok: %s\n", ifok_use);
		matched++;
	}
	if (name == NULL || strncmp("math", name, strlen(name)) == 0) {
		printf("math: %s\n", math_use);
		matched++;
	}
	if (name == NULL || strncmp("pwd", name, strlen(name)) == 0) {
		printf("pwd: %s\n", pwd_use);
		matched++;
	}
	if (name == NULL || strncmp("sleep", name, strlen(name)) == 0) {
		printf("sleep: %s\n", sleep_use);
		matched++;
	}

	if (matched == 0) {
		fprintf(stderr,
			"mash: help: no help topics match `%s'.  Try `help help' or `man -k %s' or `info %s'.\n",
			name, name, name);
	}

	return matched;
}

static int
print_description(char *name)
{
	int matched = 0;

	if (strncmp("builtin", name, strlen(name)) == 0) {
		printf("builtin - %s\n", builtin_description);
		matched++;
	}
	if (strncmp("cd", name, strlen(name)) == 0) {
		printf("cd - %s\n", cd_description);
		matched++;
	}
	if (strncmp("command", name, strlen(name)) == 0) {
		printf("command - %s\n", command_description);
		matched++;
	}
	if (strncmp("echo", name, strlen(name)) == 0) {
		printf("echo - %s\n", echo_description);
		matched++;
	}
	if (strncmp("exit", name, strlen(name)) == 0) {
		printf("exit - %s\n", exit_description);
		matched++;
	}
	if (strncmp("export", name, strlen(name)) == 0) {
		printf("export - %s\n", export_description);
		matched++;
	}
	if (strncmp("help", name, strlen(name)) == 0) {
		printf("help - %s\n", help_description);
		matched++;
	}
	if (strncmp("ifnot", name, strlen(name)) == 0) {
		printf("ifnot - %s\n", ifnot_description);
		matched++;
	}
	if (strncmp("ifok", name, strlen(name)) == 0) {
		printf("ifok - %s\n", ifok_description);
		matched++;
	}
	if (strncmp("math", name, strlen(name)) == 0) {
		printf("math - %s\n", math_description);
		matched++;
	}
	if (strncmp("pwd", name, strlen(name)) == 0) {
		printf("pwd - %s\n", pwd_description);
		matched++;
	}
	if (strncmp("sleep", name, strlen(name)) == 0) {
		printf("sleep - %s\n", sleep_description);
		matched++;
	}

	if (matched == 0) {
		fprintf(stderr,
			"mash: help: no help topics match `%s'.  Try `help help' or `man -k %s' or `info %s'.\n",
			name, name, name);
	}

	return matched;
}

static int
print_default_help(char *name)
{
	int i;
	int n_matches = 0;
	char *builtin[N_BUILTINS];
	char *use[N_BUILTINS];
	char *description[N_BUILTINS];
	char *help_str[N_BUILTINS];

	if (strncmp("builtin", name, strlen(name)) == 0) {
		builtin[n_matches] = "builtin";
		use[n_matches] = builtin_use;
		description[n_matches] = builtin_description;
		help_str[n_matches] = builtin_help;
		n_matches++;
	}
	if (strncmp("cd", name, strlen(name)) == 0) {
		builtin[n_matches] = "cd";
		use[n_matches] = cd_use;
		description[n_matches] = cd_description;
		help_str[n_matches] = cd_help;
		n_matches++;
	}
	if (strncmp("command", name, strlen(name)) == 0) {
		builtin[n_matches] = "command";
		use[n_matches] = command_use;
		description[n_matches] = command_description;
		help_str[n_matches] = command_help;
		n_matches++;
	}
	if (strncmp("echo", name, strlen(name)) == 0) {
		builtin[n_matches] = "echo";
		use[n_matches] = echo_use;
		description[n_matches] = echo_description;
		help_str[n_matches] = echo_help;
		n_matches++;
	}
	if (strncmp("exit", name, strlen(name)) == 0) {
		builtin[n_matches] = "exit";
		use[n_matches] = exit_use;
		description[n_matches] = exit_description;
		help_str[n_matches] = exit_help;
		n_matches++;
	}
	if (strncmp("export", name, strlen(name)) == 0) {
		builtin[n_matches] = "export";
		use[n_matches] = export_use;
		description[n_matches] = export_description;
		help_str[n_matches] = export_help;
		n_matches++;
	}
	if (strncmp("help", name, strlen(name)) == 0) {
		builtin[n_matches] = "help";
		use[n_matches] = help_use;
		description[n_matches] = help_description;
		help_str[n_matches] = help_help;
		n_matches++;
	}
	if (strncmp("ifnot", name, strlen(name)) == 0) {
		builtin[n_matches] = "ifnot";
		use[n_matches] = ifnot_use;
		description[n_matches] = ifnot_description;
		help_str[n_matches] = ifnot_help;
		n_matches++;
	}
	if (strncmp("ifok", name, strlen(name)) == 0) {
		builtin[n_matches] = "ifok";
		use[n_matches] = ifok_use;
		description[n_matches] = ifok_description;
		help_str[n_matches] = ifok_help;
		n_matches++;
	}
	if (strncmp("math", name, strlen(name)) == 0) {
		builtin[n_matches] = "math";
		use[n_matches] = math_use;
		description[n_matches] = math_description;
		help_str[n_matches] = math_help;
		n_matches++;
	}
	if (strncmp("pwd", name, strlen(name)) == 0) {
		builtin[n_matches] = "pwd";
		use[n_matches] = pwd_use;
		description[n_matches] = pwd_description;
		help_str[n_matches] = pwd_help;
		n_matches++;
	}
	if (strncmp("sleep", name, strlen(name)) == 0) {
		builtin[n_matches] = "sleep";
		use[n_matches] = sleep_use;
		description[n_matches] = sleep_description;
		help_str[n_matches] = sleep_help;
		n_matches++;
	}

	if (n_matches == 0) {
		fprintf(stderr,
			"mash: help: no help topics match `%s'.  Try `help help' or `man -k %s' or `info %s'.\n",
			name, name, name);
	}

	for (i = 0; i < n_matches; i++) {
		printf("%s: %s\n", builtin[i], use[i]);
		printf("    %s\n\n%s", description[i], help_str[i]);
	}
	return n_matches;
}

static int
print_help_man(char *name)
{
	int i;
	int n_matches = 0;
	char *builtin[N_BUILTINS];
	char *use[N_BUILTINS];
	char *description[N_BUILTINS];
	char *help_str[N_BUILTINS];

	if (strncmp("builtin", name, strlen(name)) == 0) {
		builtin[n_matches] = "builtin";
		use[n_matches] = builtin_use;
		description[n_matches] = builtin_description;
		help_str[n_matches] = builtin_help;
		n_matches++;
	}
	if (strncmp("cd", name, strlen(name)) == 0) {
		builtin[n_matches] = "cd";
		use[n_matches] = cd_use;
		description[n_matches] = cd_description;
		help_str[n_matches] = cd_help;
		n_matches++;
	}
	if (strncmp("command", name, strlen(name)) == 0) {
		builtin[n_matches] = "command";
		use[n_matches] = command_use;
		description[n_matches] = command_description;
		help_str[n_matches] = command_help;
		n_matches++;
	}
	if (strncmp("echo", name, strlen(name)) == 0) {
		builtin[n_matches] = "echo";
		use[n_matches] = echo_use;
		description[n_matches] = echo_description;
		help_str[n_matches] = echo_help;
		n_matches++;
	}
	if (strncmp("exit", name, strlen(name)) == 0) {
		builtin[n_matches] = "exit";
		use[n_matches] = exit_use;
		description[n_matches] = exit_description;
		help_str[n_matches] = exit_help;
		n_matches++;
	}
	if (strncmp("export", name, strlen(name)) == 0) {
		builtin[n_matches] = "export";
		use[n_matches] = export_use;
		description[n_matches] = export_description;
		help_str[n_matches] = export_help;
		n_matches++;
	}
	if (strncmp("help", name, strlen(name)) == 0) {
		builtin[n_matches] = "help";
		use[n_matches] = help_use;
		description[n_matches] = help_description;
		help_str[n_matches] = help_help;
		n_matches++;
	}
	if (strncmp("ifnot", name, strlen(name)) == 0) {
		builtin[n_matches] = "ifnot";
		use[n_matches] = ifnot_use;
		description[n_matches] = ifnot_description;
		help_str[n_matches] = ifnot_help;
		n_matches++;
	}
	if (strncmp("ifok", name, strlen(name)) == 0) {
		builtin[n_matches] = "ifok";
		use[n_matches] = ifok_use;
		description[n_matches] = ifok_description;
		help_str[n_matches] = ifok_help;
		n_matches++;
	}
	if (strncmp("math", name, strlen(name)) == 0) {
		builtin[n_matches] = "math";
		use[n_matches] = math_use;
		description[n_matches] = math_description;
		help_str[n_matches] = math_help;
		n_matches++;
	}
	if (strncmp("pwd", name, strlen(name)) == 0) {
		builtin[n_matches] = "pwd";
		use[n_matches] = pwd_use;
		description[n_matches] = pwd_description;
		help_str[n_matches] = pwd_help;
		n_matches++;
	}
	if (strncmp("sleep", name, strlen(name)) == 0) {
		builtin[n_matches] = "sleep";
		use[n_matches] = sleep_use;
		description[n_matches] = sleep_description;
		help_str[n_matches] = sleep_help;
		n_matches++;
	}

	if (n_matches == 0) {
		fprintf(stderr,
			"mash: help: no help topics match `%s'.  Try `help help' or `man -k %s' or `info %s'.\n",
			name, name, name);
	}

	for (i = 0; i < n_matches; i++) {
		printf("NAME\n"
		       "    %s - %s\n\n"
		       "SYNOPSIS\n"
		       "    %s\n\n"
		       "DESCRIPTION\n"
		       "    %s\n\n%s\n\n"
		       "SEE ALSO\n"
		       "    mash(1)\n\n"
		       "IMPLEMENTATION\n"
		       "    mash, version %s\n"
		       "    Copyright 2023 Javier Izquierdo Hernández.\n"
		       "    License Apache: Apache License version 2.0 or later <http://www.apache.org/licenses/LICENSE-2.0>\n\n",
		       builtin[i],
		       description[i],
		       use[i], description[i], help_str[i], version);
	}
	return n_matches;
}

int
help(int argc, char *argv[])
{
	int mode = 0;
	int has_pattern = 0;
	char *arg_ptr;

	argc--;
	argv++;

	if (argc == 0) {
		// Print short info about all builtins
		print_help_header();
		print_usage(NULL);
		return EXIT_SUCCESS;
	} else if (argc == 1) {
		if (strcmp(argv[0], "--help") == 0) {
			return print_help();
		}
	}
	// Check for option or pattern

	for (; *argv != NULL; argv++) {
		if (*argv[0] == '-') {
			arg_ptr = argv[0];
			arg_ptr++;
			for (; *arg_ptr != '\0'; arg_ptr++) {
				switch (*arg_ptr) {
				case 'd':
					if (!mode)
						mode = HELP_DESCRIPTION;
					break;
				case 'm':
					if (!mode)
						mode = HELP_MANPAGE;
					break;
				case 's':
					if (!mode)
						mode = HELP_USE;
					break;
				default:
					usage();
					break;
				}
			}
		} else {
			// Check pattern
			has_pattern = 1;
			switch (mode) {
			case HELP_DESCRIPTION:
				if (!print_description(argv[0])) {
					return EXIT_FAILURE;
				}
				break;
			case HELP_MANPAGE:
				if (!print_help_man(argv[0])) {
					return EXIT_FAILURE;
				}
				break;
			case HELP_USE:
				if (!print_usage(argv[0])) {
					return EXIT_FAILURE;
				}
				break;
			default:
				if (!print_default_help(argv[0])) {
					return EXIT_FAILURE;
				}
				break;
			}
		}
	}
	if (!has_pattern) {
		print_help_header();
		print_usage(NULL);
	}
	return EXIT_SUCCESS;
}
