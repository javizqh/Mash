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
#include <builtin/alias.h>
#include <builtin/bg.h>
#include <builtin/cd.h>
#include <builtin/color.h>
#include <builtin/command.h>
#include <builtin/disown.h>
#include <builtin/echo.h>
#include <builtin/exit.h>
#include <builtin/export.h>
#include <builtin/fg.h>
#include <builtin/ifnot.h>
#include <builtin/ifok.h>
#include <builtin/kill.h>
#include <builtin/mash_pwd.h>
#include <builtin/math.h>
#include <builtin/sleep.h>
#include <builtin/source.h>
#include <builtin/test.h>
#include <builtin/wait.h>
#include "parse.h"
#include "exec_info.h"
#include <builtin/jobs.h>
#include <builtin/builtin.h>
#include "builtin/help.h"

char * help_use = "help [-dms] [pattern ...]";
char * help_description = "Display information about builtin commands.";
char * help_help = 
"    Displays brief summaries of builtin commands.  If PATTERN is\n"
"    specified, gives detailed help on all commands matching PATTERN,\n"
"    otherwise the list of help topics is printed.\n\n"
"    Options:\n"
"      -d       output short description for each topic\n"
"      -m       display usage in pseudo-manpage format\n"
"      -s       output only a short usage synopsis for each topic matching PATTERN\n\n"
"    Arguments:\n"
"      PATTERN	Pattern specifying a help topic\n\n"
"    Exit Status:\n"
"    Returns success unless PATTERN is not found or an invalid option is given.\n";

static int print_help() {
	printf("help: %s\n", help_use);
	printf("    %s\n\n%s", help_description, help_help);
	return EXIT_SUCCESS;
}

static int usage() {
	fprintf(stderr,"Usage: %s\n",help_use);
	return EXIT_FAILURE;
}

static void print_usage(char *name) {
  if (name == NULL || strcmp(name, "alias") == 0) {
    printf("%s\n", alias_use);
  } 
  if (name == NULL || strcmp(name, "bg") == 0) {
    printf("%s\n", bg_use);
  } 
  if (name == NULL || strcmp(name, "builtin") == 0) {
    printf("%s\n", builtin_use);
  }  
  if (name == NULL || strcmp(name, "cd") == 0) {
    printf("%s\n", cd_use);
  } 
  if (name == NULL || strcmp(name, "command") == 0) {
    printf("%s\n", command_use);
  }   
  if (name == NULL || strcmp(name, "disown") == 0) {
    printf("%s\n", disown_use);
  } 
  if (name == NULL || strcmp(name, "echo") == 0) {
    printf("%s\n", echo_use);
  } 
  if (name == NULL || strcmp(name, "exit") == 0) {
    printf("%s\n", exit_use);
  } 
  if (name == NULL || strcmp(name, "export") == 0) {
    printf("%s\n", export_use);
  } 
  if (name == NULL || strcmp(name, "fg") == 0) {
    printf("%s\n", fg_use);
  } 
  if (name == NULL || strcmp(name, "help") == 0) {
    printf("%s\n", help_use);
  } 
  if (name == NULL || strcmp(name, "ifnot") == 0) {
    printf("%s\n", ifnot_use);
  }
  if (name == NULL || strcmp(name, "ifok") == 0) {
    printf("%s\n", ifok_use);
  }
  if (name == NULL || strcmp(name, "jobs") == 0) {
    printf("%s\n", jobs_use);
  }
  if (name == NULL || strcmp(name, "kill") == 0) {
    printf("%s\n", kill_use);
  }
  if (name == NULL || strcmp(name, "pwd") == 0) {
    printf("%s\n", pwd_use);
  }
  if (name == NULL || strcmp(name, "sleep") == 0) {
    printf("%s\n", sleep_use);
  }
  if (name == NULL || strcmp(name, "source") == 0) {
    printf("%s\n", source_use);
  }
  if (name == NULL || strcmp(name, "wait") == 0) {
    printf("%s\n", wait_use);
  }
}

int help(int argc, char* argv[]) {
  int i;
  argc--; argv++;

  if (argc == 0) {
  // Print short info about all builtins
    print_usage(NULL);
    return EXIT_SUCCESS; 
  } else if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
      return print_help();
    }
  }

  // Check for option or pattern

  for (i = 0; i < argc - 1; i++) {
    printf("%s ",argv[i]);
  }

  return EXIT_SUCCESS;
}