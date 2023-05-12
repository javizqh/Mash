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
#include "builtin/sleep.h"

char * sleep_use = "sleep NUMBER[SUFFIX]...";
char * sleep_description = "Pause for NUMBER seconds.";
char * sleep_help = 
"    Pause for NUMBER seconds.  SUFFIX may be 's' for seconds (default),\n"
"    'm' for minutes, 'h' for hours or 'd' for days.  NUMBER need to be an\n"
"    integer.  Given two or more arguments, pause for the amount of time\n"
"    specified by the sum of their values.\n\n"
"    Exit Status:\n"
"    Returns success unless an invalid option or time is given.\n";

static int help() {
	printf("sleep: %s\n", sleep_use);
	printf("    %s\n\n%s", sleep_description, sleep_help);
	return EXIT_SUCCESS;
}

static int usage() {
	fprintf(stderr,"Usage: %s\n",sleep_use);
	return EXIT_FAILURE;
}

static int invalid_time(char *time) {
	fprintf(stderr,"sleep: invalid time interval '%s'\n",time);
	return -1;
}

static int get_time(char *time) {
  char *ptr;
  int total_time = 0;
  int has_time_unit = 0;
  for ( ptr = time; *ptr != '\0' ; ptr++)
  {
    if (*ptr >= '0' && *ptr <= '9') {
      total_time = total_time * 10 + (*ptr - '0');
    } else if (*ptr == 's' && !has_time_unit) {
      has_time_unit = 1;
    } else if (*ptr == 'm' && !has_time_unit) {
      has_time_unit = 1;
      total_time *= 60;
    } else if (*ptr == 'h' && !has_time_unit) {
      has_time_unit = 1;
      total_time *= 60 * 60;
    } else if (*ptr == 'd' && !has_time_unit) {
      has_time_unit = 1;
      total_time *= 60 * 60 * 24;
    } else {
      return invalid_time(time);
    }
  }

  if (total_time == 0) {
    return invalid_time(time);
  }
  return total_time;
} 

int mash_sleep(int argc, char* argv[]) {
  argc--; argv++;
  int i;
  int time_to_add = 0;
  unsigned int time = 0;

  if (argc == 0) {
    return usage();
  } else if (argc == 1) {
    if (strcmp(argv[0],"--help") == 0) {
      return help();
    }
  }

  for (i = 0; i < argc; i++)
  {
    if ((time_to_add = get_time(argv[i])) > 0) {
      time += time_to_add;
    } else {
      return EXIT_FAILURE;
    }
  }

  fprintf(stderr, "Sleeping %d seconds",time);
  if (time == 0) {
    return usage();
  }
  

  while (time > 0)
  {
    time = sleep(time);
  }

  return EXIT_SUCCESS;
}
