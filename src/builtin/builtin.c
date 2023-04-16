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
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "builtin/cd.h"
#include "builtin/builtin.h"

char *builtins_in_shell[5] = {"cd","export","alias","exit","source"};
char *builtins_fork[1] = {"echo"};

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

  for (i = 0; i < 5; i++)
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
	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		return add_env(command->argv[0]);
	}

	if (strcmp(command->argv[0], "alias") == 0) {
		return add_alias(command->argv[1]);
	} else if (strcmp(command->argv[0], "export") == 0) {
		return add_env(command->argv[1]);
	} else if (strcmp(command->argv[0], "exit") == 0) {
		return exit_mash();
	} else if (strcmp(command->argv[0], "source") == 0) {
		return add_source(command->argv[1]);
	} else if (strcmp(command->argv[0], "cd") == 0) {
		return cd(command);
	}
	return 1;
}

int
find_builtin(struct command *command)
{
  int i;
  for (i = 0; i < 1; i++)
  {
    if (strcmp(command->argv[0], builtins_fork[i]) == 0) {
      return 1;
    }
  }

	return has_builtin_exec_in_shell(command);
}

void
exec_builtin(struct command *start_scommand, struct command *command)
{
	// FIX: solve valgrind warnings
	if (strcmp(command->argv[0], "echo") == 0) {
		// If doesn't contain alias
		int i;

		for (i = 1; i < command->argc; i++) {
			printf("%s ",
				command->argv[i]);
		}
		printf("\n");
		free_command_with_buf(start_scommand);
		exit(EXIT_SUCCESS);
	} else {
    exec_builtin_in_shell(command);
    free_command_with_buf(start_scommand);
    exit(EXIT_SUCCESS);
  }
	return;
}
