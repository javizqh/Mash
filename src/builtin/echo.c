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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "builtin/echo.h"

// DECLARE STATIC FUNCTIONS
static int usage();

static int usage() {
	fprintf(stderr,"Usage: echo [-n] [arg ...]\n");
	return EXIT_FAILURE;
}

int echo(int argc, char* argv[]) {
  int i;
  int print_newline = 1;
  argc--; argv++;

  if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
      usage();
      return EXIT_SUCCESS;
    }
  }

  if (argc > 0) {
    if (strcmp(argv[0],"-n") == 0) {
      print_newline = 0;
      argc--; argv++;
    }
  }

  for (i = 0; i < argc - 1; i++) {
    printf("%s ",argv[i]);
  }
  printf("%s",argv[i]);

  if (print_newline) {
    printf("\n");
  }
  return EXIT_SUCCESS;
}