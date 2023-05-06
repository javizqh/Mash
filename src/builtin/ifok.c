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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "builtin/command.h"
#include "builtin/ifok.h"

// DECLARE STATIC FUNCTIONS
static int usage();

static int usage() {
	fprintf(stderr,"Usage: ifok command [arg ..]\n");
	return CMD_EXIT_FAILURE;
}

int ifok(Command * command) {
  int i;
  char* result = getenv("result");
  if (result == NULL) {
    fprintf(stderr,"error: var result does not exist");
    return CMD_EXIT_FAILURE;
  }

  if (command->argc < 2) {
    return usage();
  }

  if (atoi(result) == 0) {
    for (i = 1; i < command->argc; i++)
    {
      strcpy(command->argv[i - 1], command->argv[i]);
    }
    strcpy(command->argv[command->argc - 1], command->argv[command->argc]);
    command->argc--;
    return CMD_EXIT_SUCCESS;
  }
  return CMD_EXIT_NOT_EXECUTE;
}