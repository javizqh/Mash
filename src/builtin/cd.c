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

char * cd_use = "cd [directory]";
char * cd_description = "Change the shell working directory.";
char * cd_help = 
"    Change the current directory to DIR.  The default DIR is the value of the\n"
"    HOME shell variable.\n"
"    of the current job is used.\n\n"
"    Exit Status:\n"
"    Returns 0 if the directory is changed, and non-zero otherwise.\n";

static int out_fd;
static int err_fd;

static int help() {
	dprintf(out_fd, "cd: %s\n", cd_use);
	dprintf(out_fd, "    %s\n\n%s", cd_description, cd_help);
	return EXIT_SUCCESS;
}

static int usage() {
  dprintf(err_fd, "Usage: %s\n",cd_use);
  return EXIT_FAILURE;
}

int cd(int argc, char* argv[], int stdout_fd, int stderr_fd) {
  argc--; argv++;
  char *home;

	out_fd = stdout_fd;
	err_fd = stderr_fd;

  if (argc > 1) {
    return usage();
  }
  if (argc == 0) {
    home = getenv("HOME");
    if (home == NULL) {
      home = getpwuid(getuid())->pw_dir;
    }
    if (chdir(home) < 0) {
      dprintf(err_fd,"mash: cd: %s: No such directory\n", home);
      return EXIT_FAILURE;
    }
  } else {
    if (strcmp(argv[0],"--help") == 0) {
      return help();
    }

    if (chdir(argv[0]) < 0) {
      dprintf(err_fd,"mash: cd: %s: No such directory\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}