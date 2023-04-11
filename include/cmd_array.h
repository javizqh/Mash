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

#include <string.h>
#include <glob.h>
#include "exec_cmd.h"

enum {
	MAX_COMMANDS_PER_LINE = 32
};

struct cmd_array {
	struct command *commands[MAX_COMMANDS_PER_LINE];
	int n_cmd;
	int status;
};

struct cmd_array *new_commands();

int add_command(struct command *command_to_add, struct cmd_array *commands);

int do_not_wait_commands(struct cmd_array *cmd_array);

void free_cmd_array(struct cmd_array *cmd_array);
