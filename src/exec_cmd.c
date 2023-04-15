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
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "exec_cmd.h"
#include "builtin/cd.h"

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

int
exec_command(struct command *command, FILE * src_file)
{

	int cmd_to_wait = 0;

	struct command *current_command = command;

	if (set_input_shell_pipe(command) < 0) {
		return -1;
	}

	if (set_output_shell_pipe(command) < 0) {
		return -1;
	}

	if (set_err_output_shell_pipe(command) < 0) {
		return -1;
	}
	// Make a loop fork each command
	do {
		if (cmd_to_wait > 0) {
			current_command = current_command->pipe_next;
		}
		if (has_builtin_exec_in_shell(current_command)) {
			current_command->pid = getpid();
		} else {
			current_command->pid = fork();
		}
		cmd_to_wait++;
	} while (current_command->pid != 0
		 && current_command->pipe_next != NULL);

	switch (current_command->pid) {
	case -1:
		close_fd(current_command->fd_pipe_output[0]);
		close_fd(current_command->fd_pipe_output[1]);
		close_fd(current_command->fd_pipe_input[0]);
		close_fd(current_command->fd_pipe_input[1]);
		err(EXIT_FAILURE, "Failed to fork");
		break;
	case 0:
		if (src_file != stdin)
			fclose(src_file);
		exec_child(current_command, command,
			   current_command->pipe_next);
		err(EXIT_FAILURE, "Failed to exec");
		break;
	default:
		close_all_fd(command, current_command);
		if (command->input == HERE_DOC_FILENO) {
			read_from_here_doc(command);
		} else {
			read_from_file(command);
		}
		write_to_file_or_buffer(current_command);

		return wait_childs(command, current_command, cmd_to_wait);
		break;
	}
	return EXIT_FAILURE;
}

int
exec_in_shell(struct command *command)
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
has_builtin_exec_in_shell(struct command *command)
{
	int found_match = 0;

	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		found_match = 1;
	}

	if (strcmp(command->argv[0], "alias") == 0) {
		found_match = 1;
	} else if (strcmp(command->argv[0], "export") == 0) {
		found_match = 1;
	} else if (strcmp(command->argv[0], "exit") == 0) {
		found_match = 1;
	} else if (strcmp(command->argv[0], "source") == 0) {
		found_match = 1;
	} else if (strcmp(command->argv[0], "cd") == 0) {
		found_match = 1;
	}

	if (found_match) {
		//redirect_stdin(command, start_command);
		//redirect_stdout(command, last_command);
		//redirect_stderr(command, last_command);
	}

	return found_match;
}

void
exec_child(struct command *command, struct command *start_command,
	   struct command *last_command)
{
	int i;
	char *args[command->argc];

	close_all_fd_cmd(command, start_command);
	// Check if builtin
	if (find_builtin(command)) {
		redirect_stdin(command, start_command);
		redirect_stdout(command, last_command);
		redirect_stderr(command, last_command);
		if (last_command != NULL || command->output_buffer != NULL
		    || command->output != STDOUT_FILENO
		    || command->err_output != STDERR_FILENO
		    || command->output != command->err_output) {
			close_fd(command->fd_pipe_output[1]);
		}

		exec_builtin(command);
		exit(EXIT_SUCCESS);
	} else {
		// FIND PATH
		if (!find_path(command)) {
			fprintf(stderr, "%s: command not found\n",
				command->argv[0]);
			close_fd(command->fd_pipe_input[0]);
			close_fd(command->fd_pipe_output[1]);
			exit(EXIT_FAILURE);
		}

		redirect_stdin(command, start_command);
		redirect_stdout(command, last_command);
		redirect_stderr(command, last_command);
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
			}
		}
		args[i] = NULL;

		execv(command->argv[0], args);
	}
}

int
wait_childs(struct command *start_command, struct command *last_command,
	    int n_cmds)
{

	if (start_command->do_wait != WAIT_TO_FINISH) {
		//TODO: have a list to store the surpressed command and later check
		printf("[1] %d\n", start_command->pid);
		return EXIT_SUCCESS;
	}

	struct command *current_command = start_command;
	int wstatus;
	int proc_finished;
	pid_t wait_pid;

	// REVIEW: PROBLEM WITH NOT WAITING FOR COMMANDS AFTER USING BACKGROUND BECAUSE IS WAITING FOR BACKGROUND
	for (proc_finished = 0; proc_finished < n_cmds; proc_finished++) {
		if (current_command->pid == getpid()) {
			// if the number of commands in pipe is greater than 1 DO NOT EXEC
			if (n_cmds == 1) {
				exec_in_shell(current_command);
			}
			current_command = current_command->pipe_next;
			continue;
		}
		wait_pid = waitpid(current_command->pid, &wstatus, WUNTRACED);
		if (wait_pid == -1) {
			perror("waitpid failed");
			return EXIT_FAILURE;
		}
		if (WIFEXITED(wstatus)) {
			if (WEXITSTATUS(wstatus)) {
				// If last command then save return value
				if (wait_pid == last_command->pid) {
					return WEXITSTATUS(wstatus);
				}
			}
		}
		current_command = current_command->pipe_next;
	}
	return EXIT_SUCCESS;
}

// Redirect input and output: Parent

void
read_from_here_doc(struct command *start_command)
{
	// Create stdin buffer
	ssize_t count = MAX_BUFFER_IO_SIZE;
	ssize_t bytes_stdin;

	char *buffer_stdin = malloc(sizeof(char[MAX_BUFFER_IO_SIZE]));

	if (buffer_stdin == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buffer_stdin, 0, MAX_BUFFER_IO_SIZE);

	char *here_doc_buffer = new_here_doc_buffer();

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
write_to_file_or_buffer(struct command *last_command)
{
	if ((last_command->output == STDOUT_FILENO
	     && last_command->output_buffer == NULL)
	    && last_command->err_output == STDERR_FILENO) {
		return;
	}

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
			if (last_command->err_output != STDERR_FILENO
			    && last_command->err_output !=
			    last_command->output) {
				write(last_command->err_output, buffer_stdout,
				      bytes_stdout);
			} else {
				if (last_command->output_buffer == NULL) {
					write(last_command->output,
					      buffer_stdout, bytes_stdout);
				} else {
					if (strlen(last_command->output_buffer)
					    > 0) {
						strcat
						    (last_command->output_buffer,
						     buffer_stdout);
					} else {
						strcpy
						    (last_command->output_buffer,
						     buffer_stdout);
					}
				}
			}
		}
	} while (bytes_stdout > 0);
	free(buffer_stdout);
	close_fd(last_command->fd_pipe_output[0]);
	if (last_command->output != STDOUT_FILENO) {
		close_fd(last_command->output);
	}
	if (last_command->err_output != STDERR_FILENO
	    && last_command->err_output != last_command->output) {
		close_fd(last_command->err_output);
	}
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
}

// File descriptor

int
set_input_shell_pipe(struct command *start_command)
{
	// SET INPUT PIPE
	if (start_command->input != STDIN_FILENO) {
		int fd_write_shell[2] = { -1, -1 };
		if (pipe(fd_write_shell) < 0) {
			// TODO: add error message
			err(EXIT_FAILURE, "Failed to pipe");
			return -1;
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
			// TODO: add error message
			err(EXIT_FAILURE, "Failed to pipe");
			return -1;
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
			// TODO: add error message
			err(EXIT_FAILURE, "Failed to pipe");
			return -1;
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
close_all_fd(struct command *start_command, struct command *last_command)
{
	// CLOSE ALL PIPES
	struct command *command = start_command;

	while (command != NULL) {
		close_fd(command->fd_pipe_input[0]);
		if (command->pid != start_command->pid) {
			close_fd(command->fd_pipe_input[1]);
		}
		if (command->pid != last_command->pid) {
			close_fd(command->fd_pipe_output[0]);
		}
		close_fd(command->fd_pipe_output[1]);
		command = command->pipe_next;
	}

	return 1;
}

int
close_all_fd_cmd(struct command *command, struct command *start_command)
{
	// TODO: handle close errors
	// CLOSE ALL PIPES EXCEPT MINE INPUT 0 OUTPUT 1
	// PREVIOUS CMD OUTPUT 0
	// NEXT CMD INPUT 1
	struct command *new_cmd = start_command;

	while (new_cmd != NULL) {
		if (new_cmd->input != STDIN_FILENO) {
			close_fd(new_cmd->input);
		}
		if (new_cmd->output != STDOUT_FILENO) {
			close_fd(new_cmd->output);
		}
		// IF fd is the same don't close it
		if (new_cmd == command) {
			close_fd(new_cmd->fd_pipe_input[1]);
			close_fd(new_cmd->fd_pipe_output[0]);
		} else {
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

// BUILTIN

int
find_builtin(struct command *command)
{
	if (strcmp(command->argv[0], "echo") == 0) {
		return 1;
	}
	return 0;
}

void
exec_builtin(struct command *command)
{
	// FIX: solve valgrind warnings
	if (strcmp(command->argv[0], "echo") == 0) {
		// If doesn't contain alias
		int i;

		for (i = 1; i < command->argc; i++) {
			if (i > 1) {
				dprintf(command->output, " %s",
					command->argv[i]);
			} else {
				dprintf(command->output, "%s",
					command->argv[i]);
			}
		}
		if (command->output == STDOUT_FILENO) {
			printf("\n");
		}
		free_command(command);
		exit(EXIT_SUCCESS);
	}
	return;
}
