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

enum {
	MAX_ARGUMENT_SIZE = 1024,
	MAX_ARGUMENTS = 128
};

enum wait {
	WAIT_TO_FINISH,
	DO_NOT_WAIT_TO_FINISH
};

enum exit_code {
	CMD_EXIT_SUCCESS,
	CMD_EXIT_FAILURE,
	CMD_EXIT_NOT_EXECUTE
};

enum search_cmd {
	SEARCH_CMD_EVERYWHERE,
	SEARCH_CMD_ONLY_COMMAND,
	SEARCH_CMD_ONLY_BUILTIN
};

typedef struct Command {
	char argv[MAX_ARGUMENTS][MAX_ARGUMENT_SIZE];
	int argc;
	char *current_arg;
	pid_t pid;
	int search_location;
	// Execute in background Only in first command in pipe
	int do_wait;
	// Pipes
	int input;
	int output;
	int fd_pipe_input[2];
	int fd_pipe_output[2];
	struct Command *pipe_next;
} Command;

// Builtin command
extern char *command_use;
extern char *command_description;
extern char *command_help;

int command(Command *command);

// ---------------

Command *new_command();

void reset_command(Command *command);

void free_command(Command *command);

int add_arg(Command *command);

int reset_last_arg(Command *command);

int set_file_cmd(Command *command, int file_type, char *file);

int set_to_background_cmd(Command *command);

Command *get_last_command(Command *command);

int pipe_command(Command *in_command, Command *out_command);
