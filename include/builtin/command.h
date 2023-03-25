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
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "builtin/export.h"
#include "builtin/alias.h"
//#include "builtin/builtin.h"
#include "open_files.h"

enum {
	MAX_ARGUMENT_SIZE = 128,
	MAX_ARGUMENTS = 64
};

enum wait{
	WAIT_TO_FINISH,
	DO_NOT_WAIT_TO_FINISH
};

enum status {
	DO_NOT_MATTER_TO_EXEC,
	EXECUTE_IN_SUCCESS,
	EXECUTE_IN_FAILURE
};

enum pipe{
	READ_FROM_SHELL = 0,
	READ_FROM_PIPE = 1,
	WRITE_TO_SHELL = 0,
	WRITE_TO_PIPE = 1
};

struct command {
	char argv[MAX_ARGUMENTS][MAX_ARGUMENT_SIZE];
	int argc;
	char *current_arg;
	pid_t pid;
	// Only used for the first command
	int prev_status_needed_to_exec;
	// Execute in background Only in first command
	int do_wait;
	// Pipes
	int input;
	int output;
	int fd_pipe_input[2];
	int fd_pipe_output[2];
	struct command *pipe_next;
	// Only used when $()
	char * output_buffer;
};

extern struct command *new_command();

extern void free_command(struct command *command);

extern int add_arg(struct command *command);

extern int set_file_cmd(struct command *command,int file_type, char *file);

extern int set_to_background_cmd(struct command *command);

struct command * get_last_command(struct command *command);

extern int pipe_command(struct command *in_command, struct command * out_command);

int set_alias_in_cmd(struct command * command, struct alias **aliases);

extern int find_path(struct command *command);

extern int command_exists(char *path);

extern int exec_command(struct command *command);

int close_fd(int fd);

// BUILTIN
int find_builtin2(struct command *command);

void exec_builtin(struct command *command);