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

char * echo_use = "echo [-n] [arg ...]";
char * echo_description = "Write arguments to the standard output.";
char * echo_help = 
"    Display the ARGs, separated by a single space character and followed by a\n"
"    newline, on the standard output.\n\n"
"    Options:\n"
"      -n    do not append a newline\n"
"    Exit Status:\n"
"    Returns success unless a write error occurs.\n";

int echo(int argc, char* argv[]) {
  int i;
  int print_newline = 1;
  argc--; argv++;

  if (argc > 0) {
    if (strcmp(argv[0],"-n") == 0) {
      print_newline = 0;
      argc--; argv++;
    }
  }
  
  if (argc) {
    for (i = 0; i < argc - 1; i++) {
      printf("%s ",argv[i]);
    }
    printf("%s",argv[i]);
  }


  if (print_newline) {
    printf("\n");
  }
  return EXIT_SUCCESS;
}