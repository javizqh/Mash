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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "builtin/echo.h"
#include "builtin/ifok.h"
#include "builtin/ifnot.h"
#include "builtin/cd.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "mash.h"
#include "exec_cmd.h"
#include "builtin/jobs.h"
#include "builtin/fg.h"
#include "builtin/bg.h"
#include "builtin/builtin.h"

char *builtins_modify_cmd[4] = {"ifnot","ifok","builtin","command"};
char *builtins_in_shell[8] = {"bg","fg","cd","export","alias","exit","source","."};
char *builtins_fork[2] = {"echo","jobs"};

// Builtin command

// DECLARE STATIC FUNCTION
static int usage();

static int usage() {
	fprintf(stderr,"Usage: builtin shell-builtin [arg ..]\n");
	return CMD_EXIT_FAILURE;
}

int builtin(struct command * command) {
  int i;

  if (command->argc < 2) {
    return usage();
  }

  for (i = 1; i < command->argc; i++)
  {
    strcpy(command->argv[i - 1], command->argv[i]);
  }
  strcpy(command->argv[command->argc - 1], command->argv[command->argc]);
  command->argc--;

	search_in_builtin = 1;

	if (!find_builtin(command)) {
		return usage();
	}

  return CMD_EXIT_SUCCESS;
}
// ---------------

int has_builtin_modify_cmd(struct command *command){
  int i;
	for (i = 0; i < 4; i++)
  {
    if (strcmp(command->argv[0], builtins_modify_cmd[i]) == 0) {
      return 1;
    }
  }
	return 0;
}

int modify_cmd_builtin(struct command *modify_command){
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


int
has_builtin_exec_in_shell(struct command *command)
{ 
  int i;

  if (command->pipe_next != NULL) {
    return 0;
  }

	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		return 1;
	}

  for (i = 0; i < 8; i++)
  {
    if (strcmp(command->argv[0], builtins_in_shell[i]) == 0) {
      return 1;
    }
  }

	return 0;
}

int
exec_builtin_in_shell(struct command *command)
{
	int i;
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


	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		return add_env(command->argv[0]);
	}

	if (strcmp(command->argv[0], "alias") == 0) {
		return alias(i, args);
	} else if (strcmp(command->argv[0], "export") == 0) {
		return export(i,args);
	} else if (strcmp(command->argv[0], "exit") == 0) {
		if (are_jobs_stopped()) {
			fprintf(stderr, "Mash: couldn't exit because there are stopped jobs\n");
			return EXIT_FAILURE;
		}
		wait_all_jobs();
		return exit_mash(i,args);
	} else if (strcmp(command->argv[0], "source") == 0 || strcmp(command->argv[0], ".") == 0) {
		return source(i,args);
	} else if (strcmp(command->argv[0], "cd") == 0) {
		return cd(i,args);
	} else if (strcmp(command->argv[0], "fg") == 0) {
		return fg(i,args);
	} else if (strcmp(command->argv[0], "bg") == 0) {
		return bg(i,args);
	}
	return 1;
}

int
find_builtin(struct command *command)
{
  int i;
  for (i = 0; i < 2; i++)
  {
    if (strcmp(command->argv[0], builtins_fork[i]) == 0) {
      return 1;
    }
  }

	return has_builtin_exec_in_shell(command) || has_builtin_modify_cmd(command);
}

void
exec_builtin(struct command *start_scommand, struct command *command)
{
	int i;
	int return_value = EXIT_FAILURE;
	char *args[command->argc + 1];
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
		return_value = echo(i,args);
	} else if (strcmp(args[0], "jobs") == 0) {
		return_value = jobs(i,args);
	} else {
    if (exec_builtin_in_shell(command) != 0) {
			modify_cmd_builtin(command);
		}
  }
	free_command_with_buf(start_scommand);
	exit_mash(0,NULL);
	exit(return_value);
	return;
}
