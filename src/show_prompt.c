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
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/source.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "mash.h"

int shell_mode = NON_INTERACTIVE;

void
set_prompt_mode(int mode)
{
	shell_mode = mode;
}

int
prompt()
{
	if (shell_mode == INTERACTIVE_MODE) {
		printf("$ ");
	}

	if (writing_to_file) {
		printf("\n");
	}

	fflush(stdout);
	return 1;
}

int
prompt_request()
{
	if (shell_mode == INTERACTIVE_MODE) {
		if (reading_from_file) {
			printf("> ");
		}
		fflush(stdout);
	}
	return 1;
};
