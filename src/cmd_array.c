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

#include "cmd_array.h"

// -------- Command Array -----------

struct cmd_array *
new_commands()
{
	struct cmd_array *commands =
	    (struct cmd_array *)malloc(sizeof(struct cmd_array));
	// Check if malloc failed
	if (commands == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(commands, 0, sizeof(struct cmd_array));

	commands->n_cmd = 0;
	commands->status = 0;

	return commands;
}

void
free_cmd_array(struct cmd_array *cmd_array)
{
	int i;

	for (i = 0; i < cmd_array->n_cmd; i++) {
		free_command(cmd_array->commands[i]);
		cmd_array->commands[i] = NULL;
	}
	free(cmd_array);
	return;
}

int
add_command(struct command *command_to_add, struct cmd_array *commands)
{
	if (commands->commands[commands->n_cmd] == NULL) {
		commands->commands[commands->n_cmd] = command_to_add;
		return ++commands->n_cmd;
	}
	return -1;
}

int
do_not_wait_commands(struct cmd_array *cmd_array)
{
	int i;

	for (i = 0; i < cmd_array->n_cmd; i++) {
		set_to_background_cmd(cmd_array->commands[i]);
	}
	return 0;
}
