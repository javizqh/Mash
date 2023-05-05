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

int use_jobs = 1;

int
find_command(char *line, char *buffer, FILE * src_file,
	     struct exec_info *prev_exec_info, char *to_free_excess)
{
	int status = 0;
	int status_for_next_cmd = DO_NOT_MATTER_TO_EXEC;
	char *orig_line_ptr = line;
	char cwd[MAX_ENV_SIZE];
	char result[4];
	struct exec_info *exec_info = new_exec_info(orig_line_ptr);

	if (prev_exec_info != NULL) {
		exec_info->prev_exec_info = prev_exec_info;
	}

	if (buffer != NULL) {
		set_buffer_cmd(exec_info->command, buffer);
	}
// ---------------------------------------------------------------

	while ((line = parse(line, exec_info))) {
		switch (status_for_next_cmd) {
		case DO_NOT_MATTER_TO_EXEC:
			if (use_jobs) {
				status =
				    launch_job(src_file, exec_info,
					       to_free_excess);
			} else {
				status = launch_pipe(src_file, exec_info,
						     to_free_excess);
			}
			break;
		case EXECUTE_IN_SUCCESS:
			if (status == 0) {
				if (use_jobs) {
					status =
					    launch_job(src_file, exec_info,
						       to_free_excess);
				} else {
					status =
					    launch_pipe(src_file, exec_info,
							to_free_excess);
				}
			}
			break;
		case EXECUTE_IN_FAILURE:
			if (status != 0) {
				if (use_jobs) {
					status =
					    launch_job(src_file, exec_info,
						       to_free_excess);
				} else {
					status =
					    launch_pipe(src_file, exec_info,
							to_free_excess);
				}
			} else {
				status = 0;
			}
			break;
		}
		status_for_next_cmd =
		    exec_info->command->next_status_needed_to_exec;
		sprintf(result, "%d", status);
		add_env_by_name("result", result);
		// Update cwd
		if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
			// TODO: error, load from home
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
