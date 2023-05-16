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
#include <unistd.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "open_files.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "exec_cmd.h"
#include "builtin/jobs.h"
#include "exec_pipe.h"

int
find_command(char *line, char *buffer, FILE * src_file,
	     ExecInfo * prev_exec_info, char *to_free_excess)
{
	int status = 0;
	char *orig_line_ptr = line;
	char cwd[MAX_ENV_SIZE];
	char result[4];
	ExecInfo *exec_info = new_exec_info(orig_line_ptr);

	if (prev_exec_info != NULL) {
		exec_info->prev_exec_info = prev_exec_info;
	}

	if (buffer != NULL) {
		set_buffer_cmd(exec_info->command, buffer);
	}
// ---------------------------------------------------------------

	while ((line = parse(line, exec_info))) {
		status = launch_pipe(src_file, exec_info, to_free_excess);

		sprintf(result, "%d", status);
		add_env_by_name("result", result);
		// Update cwd
		if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
			exit_mash(0, NULL, STDOUT_FILENO, STDERR_FILENO);
			err(EXIT_FAILURE,
			    "error getting current working directory");
		}
		add_env_by_name("PWD", cwd);
		if (has_to_exit) {
			break;
		}
		reset_exec_info(exec_info);
	}

	sprintf(result, "%d", status);
	add_env_by_name("result", result);

	free(exec_info->parse_info);
	free(exec_info->file_info);
	free(exec_info->sub_info);
	free_command(exec_info->command);
	memset(exec_info->line, 0, MAX_ARGUMENT_SIZE);
	free(exec_info);
	return status;
}
