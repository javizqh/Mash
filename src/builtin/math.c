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
#include "builtin/math.h"

char * math_use = "math [-n] [arg ...]";
char * math_description = "Write arguments to the standard output.";
char * math_help = 
"    Display the ARGs, separated by a single space character and followed by a\n"
"    newline, on the standard output.\n\n"
"    Options:\n"
"      -n    do not append a newline\n"
"    Exit Status:\n"
"    Returns success unless a write error occurs.\n";

static int help() {
	printf("math: %s\n", math_use);
	printf("    %s\n\n%s", math_description, math_help);
	return EXIT_SUCCESS;
}

static int usage() {
  fprintf(stderr,"Usage: %s\n",math_use);
	return EXIT_FAILURE;
}

int math(int argc, char* argv[]) {
  argc--; argv++;

  printf("math: %s\n",argv[0]);
  return EXIT_SUCCESS;
}