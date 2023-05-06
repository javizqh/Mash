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
#include "mash.h"
#include "exec_cmd.h"
#include "exec_pipe.h"

int
launch_pipe(FILE * src_file, ExecInfo * exec_info, char *to_free_excess)
{
	Command *cmd = exec_info->command;

	if (has_builtin_modify_cmd(cmd)) {
		switch (modify_cmd_builtin(cmd)) {
		case CMD_EXIT_FAILURE:
			return CMD_EXIT_FAILURE;
			break;
		case CMD_EXIT_NOT_EXECUTE:
			return EXIT_SUCCESS;	// Not execute more
			break;
		}
	}

	if (search_in_builtin && has_builtin_exec_in_shell(cmd)) {
		if (cmd->do_wait == DO_NOT_WAIT_TO_FINISH) {
			return EXIT_SUCCESS;
		}
		close_all_fd_no_fork(cmd);
		return exec_builtin_in_shell(cmd);
	}

	return exec_pipe(src_file, exec_info, to_free_excess);;
}

int
exec_pipe(FILE * src_file, ExecInfo * exec_info, char *to_free_excess)
{
	Command *current_command;

	if (set_input_shell_pipe(exec_info->command)
	    || set_output_shell_pipe(exec_info->command)
	    || set_err_output_shell_pipe(exec_info->command)) {
		return 1;
	}
	// Make a loop fork each command
	for (current_command = exec_info->command; current_command;
	     current_command = current_command->pipe_next) {
		current_command->pid = fork();
		if (current_command->pid == 0) {
			break;
		}
		if (current_command->pipe_next == NULL) {
			break;
		}
	}

	switch (current_command->pid) {
	case -1:
		close_all_fd(exec_info->command);
		fprintf(stderr, "Mash: Failed to fork");
		return EXIT_FAILURE;
		break;
	case 0:
		Command * start_command = exec_info->command;

		free_exec_info(exec_info);
		free(to_free_excess);
		if (src_file != stdin)
			fclose(src_file);
		if (reading_from_file) {
			fclose(stdin);
		}
		exec_cmd(current_command, start_command,
			 current_command->pipe_next);
		err(EXIT_FAILURE, "Failed to exec");
		break;
	default:
		close_all_fd_io(exec_info->command, current_command);
		if (current_command->do_wait == DO_NOT_WAIT_TO_FINISH) {
			return EXIT_SUCCESS;
		}

		if (exec_info->command->input == HERE_DOC_FILENO) {
			read_from_here_doc(exec_info->command);
		}

		return wait_pipe(current_command->pid);
		break;
	}
	return EXIT_FAILURE;
}

int
wait_pipe(pid_t pipe_pid)
{
	int wstatus;
	pid_t wait_pid;

	while (1) {
		wait_pid = waitpid(-1, &wstatus, WUNTRACED);
		if (WIFEXITED(wstatus)) {
			if (wait_pid == -1) {
				perror("waitpid failed 2");
				return EXIT_FAILURE;
			}
			// If last command then save return value
			if (wait_pid == pipe_pid) {
				return WEXITSTATUS(wstatus);
			}
		} else if (WIFSTOPPED(wstatus)) {
			return EXIT_SUCCESS;
		} else if (WIFSIGNALED(wstatus)) {
			return EXIT_SUCCESS;
		}
	}
	return EXIT_FAILURE;
}
