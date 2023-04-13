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
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "builtin/command.h"
#include "builtin/cd.h"

int usage() {
  fprintf(stderr, "Usage: cd [directory]\n");
  return EXIT_FAILURE;
}

int cd(struct command * command) {
  if (command->argc > 2) {
    return usage();
  }
  if (strcmp(command->argv[1],"--help") == 0 || strcmp(command->argv[1],"-h") == 0) {
    return usage();
  }
  if (command->argc == 1) {
    // FIX: get it from home
    strcpy(command->argv[1], getpwuid(getuid())->pw_dir);
  }
  if (chdir(command->argv[1]) < 0) {
    fprintf(stderr,"mash: cd: %s: No such directory\n", command->argv[1]);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}