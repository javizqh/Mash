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
find_path(struct command *command)
{
	// Check if the first character is /
	if (*command->argv[0] == '/') {
		return command_exists(command->argv[0]);
	}
	// THEN CHECK IN PWD
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
	// First get the path from env PATH
	char *path = malloc(sizeof(char) * MAX_ENV_SIZE);

	if (path == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(path, 0, sizeof(char) * MAX_ENV_SIZE);
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
exec_cmd(struct command *command, struct command *start_command,
	 struct command *last_command)
{
	int i;
	char *args[command->argc + 1];

	close_all_fd_cmd(command, start_command);
	redirect_stdin(command, start_command);
	redirect_stdout(command, last_command);
	redirect_stderr(command, last_command);
	// Check if builtin
	if (search_in_builtin && find_builtin(command)) {
		if (last_command != NULL || command->output_buffer != NULL
		    || command->output != STDOUT_FILENO
		    || command->err_output != STDERR_FILENO
		    || command->output != command->err_output) {
			close_fd(command->fd_pipe_output[1]);
		}
		exec_builtin(start_command, command);
	} else {
		exit_mash(0, NULL);
		if (!find_path(command)) {
			fprintf(stderr, "%s: command not found\n",
				command->argv[0]);
			close_fd(command->fd_pipe_input[0]);
			close_fd(command->fd_pipe_output[1]);
			free_command_with_buf(start_command);
			exit(EXIT_FAILURE);
		}

		if (last_command != NULL || command->output_buffer != NULL
		    || command->output != STDOUT_FILENO
		    || command->err_output != STDERR_FILENO
		    || command->output != command->err_output) {
			close_fd(command->fd_pipe_output[1]);
		}
		for (i = 0; i < command->argc; i++) {
			if (strlen(command->argv[i]) > 0) {
				args[i] = command->argv[i];
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
read_from_here_doc(struct command *start_command)
{
	// Create stdin buffer
	ssize_t count = MAX_BUFFER_IO_SIZE;
	ssize_t bytes_stdin;

	char *buffer_stdin = malloc(MAX_BUFFER_IO_SIZE);

	if (buffer_stdin == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buffer_stdin, 0, MAX_BUFFER_IO_SIZE);

	char *here_doc_buffer = new_here_doc_buffer();

	//REVIEW: read line print line
	do {
		bytes_stdin = read(STDIN_FILENO, buffer_stdin, count);
		if (*buffer_stdin == '}') {
			if (*++buffer_stdin == '\n') {
				--buffer_stdin;
				break;
			}
			--buffer_stdin;
		}
		strcat(here_doc_buffer, buffer_stdin);
	} while (bytes_stdin > 0);

	write(start_command->fd_pipe_input[1], here_doc_buffer,
	      strlen(here_doc_buffer));

	close_fd(start_command->fd_pipe_input[1]);
	free(buffer_stdin);
	free(here_doc_buffer);
}

void
read_from_file(struct command *start_command)
{
	if (start_command->input == STDIN_FILENO)
		return;

	// Create stdin buffer
	ssize_t count = MAX_BUFFER_IO_SIZE;
	ssize_t bytes_stdin;
	char *buffer_stdin = malloc(sizeof(char[MAX_BUFFER_IO_SIZE]));

	if (buffer_stdin == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	// Initialize buffer to 0
	memset(buffer_stdin, 0, MAX_BUFFER_IO_SIZE);
	do {
		bytes_stdin = read(start_command->input, buffer_stdin, count);
		if (write(start_command->fd_pipe_input[1], buffer_stdin,
			  bytes_stdin) < 0) {
			break;
		}
	} while (bytes_stdin > 0);

	close_fd(start_command->fd_pipe_input[1]);
	close_fd(start_command->input);
	free(buffer_stdin);
}

void
write_to_buffer(struct command *last_command)
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
redirect_stdin(struct command *command, struct command *start_command)
{
// NOT INPUT COMMAND OR INPUT COMMAND WITH FILE
	if (command->pid != start_command->pid
	    || command->input != STDIN_FILENO) {
		if (dup2(command->fd_pipe_input[0], STDIN_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdin");
		}
		close_fd(command->fd_pipe_input[0]);
	}
	if (command->input != STDIN_FILENO && command->input != HERE_DOC_FILENO) {
		if (dup2(command->input, STDIN_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdin");
		}
		close_fd(command->input);
	}
}

void
redirect_stdout(struct command *command, struct command *last_command)
{
	// NOT LAST COMMAND OR LAST COMMAND WITH FILE OR BUFFER
	if (last_command != NULL || command->output_buffer != NULL
	    || command->output != STDOUT_FILENO) {
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
redirect_stderr(struct command *command, struct command *last_command)
{
	// NOT LAST COMMAND OR LAST COMMAND WITH FILE OR BUFFER
	if (last_command != NULL || command->err_output != STDERR_FILENO) {
		// redirect stderr
		if (dup2(command->fd_pipe_output[1], STDERR_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stderr %i",
			    command->fd_pipe_input[1]);
		}
	}
	if (command->err_output != STDERR_FILENO) {
		if (dup2(command->err_output, STDERR_FILENO) == -1) {
			err(EXIT_FAILURE, "Failed to dup stdout");
		}
		close_fd(command->err_output);
	}
}

// File descriptor

int
set_input_shell_pipe(struct command *start_command)
{
	// SET INPUT PIPE
	if (start_command->input != STDIN_FILENO) {
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
set_output_shell_pipe(struct command *start_command)
{
	// SET OUTOUT PIPE
	// Find last one and add it
	struct command *last_command = get_last_command(start_command);

	if (last_command->output != STDOUT_FILENO
	    || last_command->output_buffer != NULL) {
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
set_err_output_shell_pipe(struct command *start_command)
{
	// SET OUTOUT PIPE
	// Find last one and add it
	struct command *last_command = get_last_command(start_command);

	if (last_command->err_output != STDERR_FILENO
	    && last_command->err_output != last_command->output) {
		int fd_read_shell[2] = { -1, -1 };
		if (pipe(fd_read_shell) < 0) {
			fprintf(stderr, "Failed to pipe to stderr");
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
close_all_fd(struct command *start_command)
{
	struct command *command = start_command;

	while (command != NULL) {
		if (command->input != STDIN_FILENO) {
			close_fd(command->input);
		}
		if (command->output != STDOUT_FILENO) {
			close_fd(command->output);
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
close_all_fd_io(struct command *start_command, struct command *last_command)
{
	struct command *command = start_command;

	while (command != NULL) {
		close_fd(command->fd_pipe_input[0]);
		if (command->pid != start_command->pid
		    || start_command->input != HERE_DOC_FILENO) {
			close_fd(command->fd_pipe_input[1]);
		}
		if (command->pid != last_command->pid
		    || last_command->output_buffer == NULL) {
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
close_all_fd_cmd(struct command *command, struct command *start_command)
{
	// CLOSE ALL PIPES EXCEPT MINE INPUT 0 OUTPUT 1
	// PREVIOUS CMD OUTPUT 0
	// NEXT CMD INPUT 1
	struct command *new_cmd = start_command;

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
