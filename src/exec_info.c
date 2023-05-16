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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "open_files.h"
#include "parse.h"
#include "exec_info.h"

SubInfo *
new_sub_info()
{
	SubInfo *sub_info = (SubInfo *) malloc(sizeof(SubInfo));

	// Check if malloc failed
	if (sub_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(sub_info, 0, sizeof(SubInfo));
	sub_info->old_ptr = NULL;
	sub_info->old_lexer = NULL;

	return sub_info;
}

void
restore_sub_info(SubInfo * sub_info)
{
	//memset(sub_info->last_alias, 0, MAX_ARGUMENT_SIZE);
	memset(sub_info->buffer, 0, MAX_ENV_SIZE);
	sub_info->old_ptr = NULL;
	sub_info->old_lexer = NULL;
}

FileInfo *
new_file_info()
{
	FileInfo *file_info = (FileInfo *) malloc(sizeof(FileInfo));

	// Check if malloc failed
	if (file_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(file_info, 0, sizeof(FileInfo));
	file_info->mode = NO_FILE_READ;
	file_info->ptr = file_info->buffer;

	return file_info;
}

void
restore_file_info(FileInfo * file_info)
{
	file_info->mode = NO_FILE_READ;
	memset(file_info->buffer, 0, MAX_ARGUMENT_SIZE);
	file_info->ptr = file_info->buffer;
}

ExecInfo *
new_exec_info(char *line)
{
	ExecInfo *exec_info = malloc(sizeof(ExecInfo));

	if (exec_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(exec_info, 0, sizeof(ExecInfo));
	exec_info->command = new_command();
	exec_info->last_command = exec_info->command;
	exec_info->parse_info = new_parse_info();
	exec_info->sub_info = new_sub_info();
	exec_info->file_info = new_file_info();
	exec_info->line = line;
	exec_info->prev_exec_info = NULL;
	return exec_info;
}

void
reset_exec_info(ExecInfo * exec_info)
{
	reset_command(exec_info->command);
	restore_parse_info(exec_info->parse_info);
	restore_file_info(exec_info->file_info);
	restore_sub_info(exec_info->sub_info);
}

void
free_exec_info(ExecInfo * exec_info)
{
	free(exec_info->parse_info);
	free(exec_info->file_info);
	free(exec_info->sub_info);
	free(exec_info->line);

	if (exec_info->prev_exec_info != NULL) {
		free_command_with_buf(exec_info->prev_exec_info->command);
		free_exec_info(exec_info->prev_exec_info);
	}
	free(exec_info);
};
