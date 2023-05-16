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
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "builtin/mash_pwd.h"
#include "builtin/echo.h"
#include "builtin/mash_math.h"
#include "builtin/sleep.h"
#include "builtin/ifok.h"
#include "builtin/ifnot.h"
#include "builtin/cd.h"
#include "builtin/help.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "mash.h"
#include "exec_cmd.h"
#include "builtin/builtin.h"

char *builtins_modify_cmd[4] = { "ifnot", "ifok", "builtin", "command" };

char *builtins_in_shell[4] = { "cd", "export", "alias", "exit" };
char *builtins_fork[5] = { "math", "help", "sleep", "pwd", "echo" };

int N_BUILTINS = 4 + 4 + 5;

// Builtin command
char *builtin_use = "builtin shell-builtin [arg ..]";
char *builtin_description = "Execute shell builtins.";
char *builtin_help =
    "    Execute SHELL-BUILTIN with arguments ARGs without performing command\n"
    "    lookup.\n\n"
    "    Exit Status:\n"
    "    Returns the exit status of SHELL-BUILTIN, or 1 if SHELL-BUILTIN is\n"
    "    not a shell builtin.\n";
// DECLARE STATIC FUNCTION
static int
help_builtin()
{
	printf("builtin: %s\n", builtin_use);
	printf("    %s\n\n%s", builtin_description, builtin_help);
	return CMD_EXIT_NOT_EXECUTE;
}

static int
usage()
{
	fprintf(stderr, "Usage: %s\n", builtin_use);
	return CMD_EXIT_FAILURE;
}

int
builtin(Command * command)
{
	int i;

	if (command->argc < 2) {
		return usage();
	}

	if (command->argc == 2) {
		if (strcmp(command->argv[1], "--help") == 0) {
			return help_builtin();
		}
	}

	for (i = 1; i < command->argc; i++) {
		strcpy(command->argv[i - 1], command->argv[i]);
	}
	strcpy(command->argv[command->argc - 1], command->argv[command->argc]);
	command->argc--;

	command->search_location = SEARCH_CMD_ONLY_BUILTIN;

	if (!find_builtin(command)) {
		return usage();
	}

	return CMD_EXIT_SUCCESS;
}

// ---------------

int
has_builtin_modify_cmd(Command * command)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (strcmp(command->argv[0], builtins_modify_cmd[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

int
modify_cmd_builtin(Command * modify_command)
{
	if (strcmp(modify_command->argv[0], "command") == 0) {
		return command(modify_command);
	} else if (strcmp(modify_command->argv[0], "builtin") == 0) {
		return builtin(modify_command);
	} else if (strcmp(modify_command->argv[0], "ifnot") == 0) {
		return ifnot(modify_command);
	} else if (strcmp(modify_command->argv[0], "ifok") == 0) {
		return ifok(modify_command);
	}
	return 1;
}

static int
found_builtin_exec_in_shell(Command * command)
{
	int i;

	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		return 1;
	}

	for (i = 0; i < 4; i++) {
		if (strcmp(command->argv[0], builtins_in_shell[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

int
has_builtin_exec_in_shell(Command * command)
{
	if (command->pipe_next != NULL) {
		return 0;
	}

	return found_builtin_exec_in_shell(command);
}

static void
wait_for_heredoc()
{
	int has_max_length = 0;

	// MINIMUM size of buffer = 4
	char *buf = malloc(4);

	if (buf == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buf, 0, 4);

	while (fgets(buf, 4, stdin) != NULL) {
		if (strlen(buf) >= 4 - 1) {
			has_max_length = 1;
		} else {
			if (!has_max_length && (strcmp(buf, "}\n") == 0 ||
						(strlen(buf) == 1
						 && *buf == '}'))) {
				break;
			}
			has_max_length = 0;
		}
	}

	free(buf);
	return;
}

int
exec_builtin_in_shell(Command * command, int is_pipe)
{
	int i;
	int exit_code = EXIT_FAILURE;
	int cmd_out = command->output;
	int cmd_err = STDERR_FILENO;

	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		strcpy(command->argv[1], command->argv[0]);
		strcpy(command->argv[0], "export");
		command->argc = 2;
	}

	char *args[command->argc];

	for (i = 0; i < command->argc; i++) {
		if (strlen(command->argv[i]) > 0) {
			args[i] = command->argv[i];
		} else {
			args[i] = NULL;
			break;
		}
	}
	args[i] = NULL;

	if (!is_pipe && command->input == HERE_DOC_FILENO) {
		wait_for_heredoc();
	}

	if (strcmp(command->argv[0], "alias") == 0) {
		exit_code = alias(i, args, cmd_out, cmd_err);
	} else if (strcmp(command->argv[0], "export") == 0) {
		exit_code = export(i, args, cmd_out, cmd_err);
	} else if (strcmp(command->argv[0], "exit") == 0) {
		exit_code = exit_mash(i, args, cmd_out, cmd_err);
	} else if (strcmp(command->argv[0], "cd") == 0) {
		exit_code = cd(i, args, cmd_out, cmd_err);
	}

	if (!is_pipe) {
		if (cmd_out != STDOUT_FILENO) {
			close_fd(cmd_out);
		}
		if (cmd_err != STDERR_FILENO) {
			close_fd(cmd_err);
		}
	}

	return exit_code;
}

int
find_builtin(Command * command)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (strcmp(command->argv[0], builtins_fork[i]) == 0) {
			return 1;
		}
	}

	return found_builtin_exec_in_shell(command)
	    || has_builtin_modify_cmd(command);
}

void
exec_builtin(Command * start_scommand, Command * command)
{
	int i;
	int return_value = EXIT_FAILURE;
	char *args[command->argc + 1];

	// FiX: treat properly sigpipe
	signal(SIGPIPE, SIG_IGN);

	for (i = 0; i < command->argc; i++) {
		if (strlen(command->argv[i]) > 0) {
			args[i] = command->argv[i];
		} else {
			args[i] = NULL;
			break;
		}
	}
	args[i] = NULL;
	if (strcmp(args[0], "echo") == 0) {
		return_value = echo(i, args);
	} else if (strcmp(args[0], "pwd") == 0) {
		return_value = pwd(i, args);
	} else if (strcmp(args[0], "sleep") == 0) {
		return_value = mash_sleep(i, args);
	} else if (strcmp(args[0], "help") == 0) {
		return_value = help(i, args);
	} else if (strcmp(args[0], "math") == 0) {
		return_value = math(i, args);
	} else if (strcmp(args[0], "exit") != 0) {
		if (found_builtin_exec_in_shell(command)) {
			exec_builtin_in_shell(command, 1);
		}
		modify_cmd_builtin(command);
	}
	free_command(start_scommand);
	exit_mash(0, NULL, STDOUT_FILENO, STDERR_FILENO);
	exit(return_value);
	signal(SIGPIPE, SIG_DFL);
	return;
}
