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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "open_files.h"
#include "builtin/alias.h"
#include "builtin/command.h"

// Builtin command

// DECLARE GLOBAL VARIABLE
char *command_use = "command [-Vv] command [arg ..]";
char *command_description =
    "Execute a simple command or display information about commands.";
char *command_help =
    "    Runs COMMAND with ARGS suppressing  shell function lookup, or display\n"
    "    information about the specified COMMANDs.\n\n" "    Options:\n"
    "      -v    print a description of COMMAND similar to the `type' builtin\n"
    "      -V    print a more verbose description of each COMMAND\n\n"
    "    Exit Status:\n"
    "    Returns exit status of COMMAND, or failure if COMMAND is not found.\n";

static int
help()
{
	printf("command: %s\n", command_use);
	printf("    %s\n\n%s", command_description, command_help);
	return CMD_EXIT_NOT_EXECUTE;
}

static int
usage()
{
	fprintf(stderr, "Usage: %s\n", command_use);
	return CMD_EXIT_FAILURE;
}

int
command(Command * command)
{
	// TODO: add -v / -V
	int i;

	if (command->argc < 2) {
		return usage();
	}

	if (command->argc == 2) {
		if (strcmp(command->argv[1], "--help") == 0) {
			return help();
		}
	}

	for (i = 1; i < command->argc; i++) {
		strcpy(command->argv[i - 1], command->argv[i]);
	}
	strcpy(command->argv[command->argc - 1], command->argv[command->argc]);
	command->argc--;

	command->search_location = SEARCH_CMD_ONLY_COMMAND;

	return CMD_EXIT_SUCCESS;
}

// ---------------
Command *
new_command()
{
	Command *command = (Command *) malloc(sizeof(Command));

	// Check if malloc failed
	if (command == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(command, 0, sizeof(Command));

	command->argc = 0;
	command->current_arg = command->argv[0];
	command->pid = 0;
	command->search_location = SEARCH_CMD_EVERYWHERE;
	command->next_status_needed_to_exec = DO_NOT_MATTER_TO_EXEC;
	command->do_wait = WAIT_TO_FINISH;
	command->input = STDIN_FILENO;
	command->output = STDOUT_FILENO;
	command->err_output = STDERR_FILENO;
	command->fd_pipe_input[0] = -1;
	command->fd_pipe_input[1] = -1;
	command->fd_pipe_output[0] = -1;
	command->fd_pipe_output[1] = -1;
	command->pipe_next = NULL;
	command->output_buffer = NULL;

	return command;
}

void
reset_command(Command * command)
{
	free_command(command->pipe_next);
	memset(command, 0, sizeof(*command));
	command->argc = 0;
	command->current_arg = command->argv[0];
	command->pid = 0;
	command->search_location = SEARCH_CMD_EVERYWHERE;
	command->next_status_needed_to_exec = DO_NOT_MATTER_TO_EXEC;
	command->do_wait = WAIT_TO_FINISH;
	command->input = STDIN_FILENO;
	command->output = STDOUT_FILENO;
	command->err_output = STDERR_FILENO;
	command->fd_pipe_input[0] = -1;
	command->fd_pipe_input[1] = -1;
	command->fd_pipe_output[0] = -1;
	command->fd_pipe_output[1] = -1;
	command->pipe_next = NULL;
	command->output_buffer = NULL;
};

void
free_command(Command * command)
{
	Command *to_free = command;
	Command *next = command;

	while (next != NULL) {
		to_free = next;
		next = to_free->pipe_next;
		free(to_free);
	}
}

void
free_command_with_buf(Command * command)
{
	Command *to_free = command;
	Command *next = command;

	while (next != NULL) {
		to_free = next;
		next = to_free->pipe_next;
		free(to_free->output_buffer);
		free(to_free);
	}
}

extern int
check_alias_cmd(Command * command)
{
	if (command->argc == 0) {
		if (get_alias(command->argv[0]) != NULL) {
			return 1;
		}
	}
	return 0;
};

int
add_arg(Command * command)
{
	if (++command->argc > MAX_ARGUMENTS)
		return 0;
	command->current_arg = command->argv[command->argc];
	return 1;
}

int
reset_last_arg(Command * command)
{
	memset(command->current_arg, 0, MAX_ARGUMENT_SIZE);
	command->current_arg = command->argv[command->argc - 1];
	return 1;
};

int
set_file_cmd(Command * command, int file_type, char *file)
{
	switch (file_type) {
	case HERE_DOC_READ:
		if (command->input != STDIN_FILENO) {
			close(command->input);
		}
		command->input = HERE_DOC_FILENO;
		return 1;
		break;
	case INPUT_READ:
		if (command->input != STDIN_FILENO) {
			close(command->input);
		}
		command->input = open_read_file(file);
		return command->input;
		break;
	case OUTPUT_WRITE:
		// GO TO LAST CMD IN PIPE
		Command * last_cmd = get_last_command(command);
		if (last_cmd->output != STDOUT_FILENO) {
			close(last_cmd->output);
		}
		last_cmd->output = open_write_file(file);
		return last_cmd->output;
		break;
	case ERROR_WRITE:
		// GO TO LAST CMD IN PIPE
		Command * last_err_cmd = get_last_command(command);
		if (last_err_cmd->err_output != STDERR_FILENO) {
			close(last_err_cmd->err_output);
		}
		last_err_cmd->err_output = open_write_file(file);
		return last_err_cmd->err_output;
		break;
	case ERROR_AND_OUTPUT_WRITE:
		// GO TO LAST CMD IN PIPE
		Command * last_err_out_cmd = get_last_command(command);
		if (last_err_out_cmd->output != STDOUT_FILENO) {
			close(last_err_out_cmd->output);
		}
		last_err_out_cmd->output = open_write_file(file);
		last_err_out_cmd->err_output = last_err_out_cmd->output;
		return last_err_out_cmd->err_output;
		break;
	}
	return -1;
}

int
set_buffer_cmd(Command * command, char *buffer)
{
	get_last_command(command)->output_buffer = buffer;
	return 1;
}

int
set_to_background_cmd(Command * command)
{
	command->do_wait = DO_NOT_WAIT_TO_FINISH;
	if (command->input == STDIN_FILENO) {
		set_file_cmd(command, INPUT_READ, "/dev/null");
	}
	return 0;
}

Command *
get_last_command(Command * command)
{
	Command *current_command = command;

	while (current_command->pipe_next != NULL) {
		current_command = current_command->pipe_next;
	}
	return current_command;
}

int
pipe_command(Command * in_command, Command * out_command)
{
	int fd[2];

	if (pipe(fd) < 0) {
		err(EXIT_FAILURE, "Failed to pipe");
	}
	in_command->pipe_next = out_command;
	in_command->fd_pipe_output[0] = fd[0];
	in_command->fd_pipe_output[1] = fd[1];
	out_command->fd_pipe_input[0] = fd[0];
	out_command->fd_pipe_input[1] = fd[1];
	out_command->output_buffer = in_command->output_buffer;
	return 1;
}
