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
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "builtin/mash_pwd.h"

char * pwd_use = "pwd";

static int usage() {
	fprintf(stderr,"Usage: %s\n",pwd_use);
	return EXIT_FAILURE;
}

int pwd(int argc, char* argv[]) {
  argc--; argv++;
  char *pwd;

  if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
      usage();
      return EXIT_SUCCESS;
    }
  }

  if (argc > 0) {
    return usage();
  }

  if ((pwd = getenv("PWD")) == NULL) {
    fprintf(stderr,"mash: pwd: failed to get current working directory\n");
    return EXIT_FAILURE;
  }
  
  printf("%s\n",pwd);

  return EXIT_SUCCESS;
}