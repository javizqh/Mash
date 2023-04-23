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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin/export.h"

// DECLARE STATIC FUNCTION
static int usage();

extern char **environ;

static int usage() {
	fprintf(stderr,"Usage: export [name=value]\n");
	return EXIT_FAILURE;
}

int export(int argc, char* argv[]) {
	argc--;argv++;
	int exit_value = 0;
	if (argc == 0) {
		print_env();
	} else if (argc == 1) {
		exit_value = add_env(argv[0]);
	} else {
		return usage();
	}
	return exit_value;
}

int add_env(const char * line) {
  char *p = strchr(line, '=');
  if (p != NULL) {
    *p = '\0';
    // Remove \n
    char *eol = strchr(++p, '\n');
    if (eol != NULL) {
      *eol = '\0';
    }
    // Set environment variables
    setenv(line, p, 1);
    return 0;
  }
  return 1;
}

int add_env_by_name(const char *key, const char *value) {
  return setenv(key, value, 1);
}

void print_env() {
  char **s = environ;

  for (; *s; s++) {
    printf("%s\n", *s);
  }
}