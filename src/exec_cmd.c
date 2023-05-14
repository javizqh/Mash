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
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "builtin/jobs.h"
#include "mash.h"
#include "exec_cmd.h"

pid_t active_command = 0;

int
find_path(Command * command)
{
	// Check if the first character is /
	if (*command->argv[0] == '/') {
		return command_exists(command->argv[0]);
	}
	// CHECK IN PWD
	char *cwd = malloc(MAX_ENV_SIZE);

	if (cwd == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(cwd, 0, MAX_ENV_SIZE);
	// Copy the path
	if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
		free(cwd);
		return -1;
	}
	char *cwd_ptr = cwd;

	strcat(cwd_ptr, "/");
	strcat(cwd_ptr, command->argv[0]);

	if (command_exists(cwd_ptr)) {
		strcpy(command->argv[0], cwd_ptr);
		free(cwd);
		return 1;
	}
	free(cwd);

	// SEARCH IN PATH
	// First get the path from env PATH
	char *path = malloc(MAX_ENV_SIZE * MAX_PATH_SIZE);

	if (path == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(path, 0, MAX_ENV_SIZE * MAX_PATH_SIZE);
	// Copy the path
	strcpy(path, getenv("PATH"));
	if (path == NULL) {
		free(path);
		return -1;
	}
	char *path_ptr = path;

	// Then separate the path by : using strtok
	char path_tok[MAX_ENV_SIZE][MAX_PATH_SIZE];

	char *token;
	int path_len = 0;

	while ((token = strtok_r(path_ptr, ":", &path_ptr))) {
		strcpy(path_tok[path_len], token);
		path_len++;
	}
	// Loop through the path until we find an exec
	int i;

	for (i = 0; i < path_len; i++) {
		strcat(path_tok[i], "/");
		strcat(path_tok[i], command->argv[0]);
		if (command_exists(path_tok[i])) {
			strcpy(command->argv[0], path_tok[i]);
			free(path);
			return 1;
		}
	}
	free(path);
	return 0;
}

int
command_exists(char *path)
{
	struct stat buf;

	if (stat(path, &buf) != 0) {
		return 0;
	}
	// Check the `st_mode` field to see if the `S_IXUSR` bit is set
	if (buf.st_mode & S_IXUSR) {
		return 1;
	}
	return 0;
}

void
exec_cmd(Command * cmd, Command * start_cmd, Command * last_cmd)
{
	int i;
	char *args[cmd->argc + 1];

	close_all_fd_cmd(cmd, start_cmd);
	redirect_stdin(cmd, start_cmd);
	redirect_stdout(cmd);
	redirect_stderr(cmd);
	// Check if builtin
	if (cmd->search_location != SEARCH_CMD_ONLY_COMMAND
	    && find_builtin(cmd)) {
		if (last_cmd != NULL || cmd->output_buffer != NULL
		    || cmd->output != STDOUT_FILENO
		    || cmd->err_output != STDERR_FILENO
		    || cmd->output != cmd->err_output) {
			close_fd(cmd->fd_pipe_output[1]);
		}
		exec_builtin(start_cmd, cmd);
	} else {
		exit_mash(0, NULL, STDOUT_FILENO, STDERR_FILENO);
		if (!find_path(cmd)) {
			fprintf(stderr, "%s: cmd not found\n", cmd->argv[0]);
			close_fd(cmd->fd_pipe_input[0]);
			close_fd(cmd->fd_pipe_output[1]);
			free_command_with_buf(start_cmd);
			exit(EXIT_FAILURE);
		}

		if (last_cmd != NULL || cmd->output_buffer != NULL
		    || cmd->output != STDOUT_FILENO
		    || cmd->err_output != STDERR_FILENO
		    || cmd->output != cmd->err_output) {
			close_fd(cmd->fd_pipe_output[1]);
		}

		for (i = 0; i < cmd->argc; i++) {
			if (strlen(cmd->argv[i]) > 0) {
				args[i] = cmd->argv[i];
			} else {
				args[i] = NULL;
				break;
			}
		}

		args[i] = NULL;
		execv(args[0], args);
	}
}

// Redirect input and output: Parent

void
read_from_here_doc(Command * start_command)
{
	// Create stdin buffer
	ssize_t count = MAX_BUFFER_IO_SIZE;
	ssize_t bytes_stdin;
	int has_max_length = 0;

	char *buffer_stdin = malloc(MAX_BUFFER_IO_SIZE);

	if (buffer_stdin == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buffer_stdin, 0, MAX_BUFFER_IO_SIZE);

	char *here_doc_buffer = new_here_doc_buffer();

	do {
		bytes_stdin = read(STDIN_FILENO, buffer_stdin, count);
		if (strlen(buffer_stdin) >= MAX_BUFFER_IO_SIZE - 1) {
			has_max_length = 1;
		} else {
			if (!has_max_length && strcmp(buffer_stdin,"}\n") == 0) {
				break;
			}
			has_max_length = 0;
			strcat(here_doc_buffer, buffer_stdin);
			memset(buffer_stdin, 0, MAX_BUFFER_IO_SIZE);
		}
	} while (bytes_stdin > 0
		 && strlen(here_doc_buffer) < MAX_HERE_DOC_BUFFER);

	if (strlen(here_doc_buffer) >= MAX_HERE_DOC_BUFFER) {
		fprintf(stderr,
			"Mash: error: exceeded max size of %d of here document\n",
			MAX_HERE_DOC_BUFFER);
	} else {
		write(start_command->fd_pipe_input[1], here_doc_buffer,
		      strlen(here_doc_buffer));
	}

	close_fd(start_command->fd_pipe_input[1]);
	free(buffer_stdin);
	free(here_doc_buffer);
}

void
write_to_buffer(Command * last_command)
{
	ssize_t count = MAX_BUFFER_IO_SIZE;
	ssize_t bytes_stdout;

	// Create stdout buffer
	char *buffer_stdout = malloc(sizeof(char[MAX_BUFFER_IO_SIZE]));

	if (buffer_stdout == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(buffer_stdout, 0, MAX_BUFFER_IO_SIZE);

	do {
		bytes_stdout =
		    read(last_command->fd_pipe_output[0], buffer_stdout, count);
		if (bytes_stdout > 0) {
			strcat(last_command->output_buffer, buffer_stdout);
		}
	} while (bytes_stdout > 0);
	free(buffer_stdout);
	close_fd(last_command->fd_pipe_output[0]);
}

// Redirect input and output: Child
void
redirect_stdin(Command * command, Command * start_command)
{
// NOT INPUT COMMAND OR INPUT COMMAND WITH FILE
	if (command->pid != start_command->pid
	    || start_command->input == HERE_DOC_FILENO) {
		if (dup2(command->fd_pipe_input[0], STDIN_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdin %i",
			    command->fd_pipe_input[0]);
		}
		close_fd(command->fd_pipe_input[0]);
	}
	if (command->input != STDIN_FILENO && command->input != HERE_DOC_FILENO) {
		if (dup2(command->input, STDIN_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdin %i",
			    command->input);
		}
		close_fd(command->input);
	}
}

void
redirect_stdout(Command * command)
{
	// NOT LAST COMMAND OR LAST COMMAND WITH FILE OR BUFFER
	if (command->pipe_next || command->output_buffer) {
		// redirect stdout
		if (dup2(command->fd_pipe_output[1], STDOUT_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdout");
		}
	}

	if (command->output != STDOUT_FILENO) {
		if (dup2(command->output, STDOUT_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdout");
		}
		if (command->output != command->err_output) {
			close_fd(command->output);
		}
	}
}

void
redirect_stderr(Command * command)
{
	if (command->err_output != STDERR_FILENO) {
		if (dup2(command->err_output, STDERR_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdout");
		}
		close_fd(command->err_output);
	}
}

// File descriptor

int
set_input_shell_pipe(Command * start_command)
{
	// SET INPUT PIPE
	if (start_command->input == HERE_DOC_FILENO) {
		int fd_write_shell[2] = { -1, -1 };
		if (pipe(fd_write_shell) < 0) {
			fprintf(stderr, "Failed to pipe to stdin");
			free_command_with_buf(start_command);
			return 1;
		}
		start_command->fd_pipe_input[0] = fd_write_shell[0];
		start_command->fd_pipe_input[1] = fd_write_shell[1];
	}
	return 0;
}

int
set_output_shell_pipe(Command * start_command)
{
	// SET OUTOUT PIPE
	// Find last one and add it
	Command *last_command = get_last_command(start_command);

	if (last_command->output_buffer) {
		int fd_read_shell[2] = { -1, -1 };
		if (pipe(fd_read_shell) < 0) {
			fprintf(stderr, "Failed to pipe to stdout");
			free_command_with_buf(start_command);
			return 1;
		}
		last_command->fd_pipe_output[0] = fd_read_shell[0];
		last_command->fd_pipe_output[1] = fd_read_shell[1];
	}
	return 0;
}

int
close_fd(int fd)
{
	if (fd >= 0) {
		close(fd);
		return 0;
	}
	return 1;
}

int
close_all_fd(Command * start_command)
{
	Command *command = start_command;

	while (command != NULL) {
		if (command->input != STDIN_FILENO) {
			close_fd(command->input);
		}
		if (command->output != STDOUT_FILENO) {
			close_fd(command->output);
		}
		if (command->err_output != STDERR_FILENO) {
			close_fd(command->err_output);
		}
		close_fd(command->fd_pipe_input[0]);
		close_fd(command->fd_pipe_input[1]);
		close_fd(command->fd_pipe_output[0]);
		close_fd(command->fd_pipe_output[1]);
		command = command->pipe_next;
	}

	return 1;
}

int
close_all_fd_no_fork(Command * start_command)
{
	Command *command = start_command;

	if (command->input != STDIN_FILENO) {
		close_fd(command->input);
	}
	close_fd(command->fd_pipe_input[0]);
	close_fd(command->fd_pipe_input[1]);
	close_fd(command->fd_pipe_output[0]);
	close_fd(command->fd_pipe_output[1]);

	return 1;
}

int
close_all_fd_io(Command * start_command, Command * last_command)
{
	Command *command = start_command;

	while (command != NULL) {
		close_fd(command->fd_pipe_input[0]);
		if (command->pid != start_command->pid
		    || command->input != HERE_DOC_FILENO) {
			close_fd(command->fd_pipe_input[1]);
		}
		if (command->pid != last_command->pid
		    || command->output_buffer == NULL) {
			close_fd(command->fd_pipe_output[0]);
		}
		close_fd(command->fd_pipe_output[1]);
		if (command->input > 0) {
			close_fd(command->input);
		}
		if (command->output != STDOUT_FILENO) {
			close_fd(command->output);
		}
		if (command->err_output != STDERR_FILENO) {
			close_fd(command->err_output);
		}
		command = command->pipe_next;
	}

	return 1;
}

int
close_all_fd_cmd(Command * command, Command * start_command)
{
	// CLOSE ALL PIPES EXCEPT MINE INPUT 0 OUTPUT 1
	// PREVIOUS CMD OUTPUT 0
	// NEXT CMD INPUT 1
	Command *new_cmd = start_command;

	while (new_cmd != NULL) {
		if (new_cmd->input != STDIN_FILENO
		    && start_command->input != new_cmd->input) {
			close_fd(new_cmd->input);
		}
		// IF fd is the same don't close it
		if (new_cmd == command) {
			close_fd(new_cmd->fd_pipe_input[1]);
			close_fd(new_cmd->fd_pipe_output[0]);
		} else {
			if (new_cmd->output != STDOUT_FILENO) {
				close_fd(new_cmd->output);
			}
			// Close if the cmd output is not the next input
			if (new_cmd->fd_pipe_input[1] !=
			    command->fd_pipe_output[1]) {
				close_fd(new_cmd->fd_pipe_input[1]);
			}
			// Close if the cmd input is not the prev output
			if (new_cmd->fd_pipe_output[0] !=
			    command->fd_pipe_input[0]) {
				close_fd(new_cmd->fd_pipe_output[0]);
			}
			close_fd(new_cmd->fd_pipe_input[0]);
			close_fd(new_cmd->fd_pipe_output[1]);
		}
		new_cmd = new_cmd->pipe_next;
	}
	return 1;
}
