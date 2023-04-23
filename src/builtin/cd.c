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
#include "builtin/cd.h"

// DECLARE STATIC FUNCTION
static int usage();

static int usage() {
  fprintf(stderr, "Usage: cd [directory]\n");
  return EXIT_FAILURE;
}

int cd(int argc, char* argv[]) {
  char *home;

  argc--; argv++;
  if (argc > 1) {
    return usage();
  }
  if (argc == 0) {
    home = getenv("HOME");
    if (home == NULL) {
      home = getpwuid(getuid())->pw_dir;
    }
    if (chdir(home) < 0) {
      fprintf(stderr,"mash: cd: %s: No such directory\n", home);
      return EXIT_FAILURE;
    }
  } else {
    if (strcmp(argv[0],"--help") == 0 || strcmp(argv[0],"-h") == 0) {
      return usage();
    }

    if (chdir(argv[0]) < 0) {
      fprintf(stderr,"mash: cd: %s: No such directory\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}