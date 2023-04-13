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
#include "show_prompt.h"

int
prompt()
{
	long size = ftell(stdin);
	char *prompt = getenv("PROMPT");
	char *cwd = getenv("PWD");

	if (size < 0) {
		printf("\033[01;35m%s:~%s $ \033[0m", prompt, cwd);
	}
	return 1;
}
