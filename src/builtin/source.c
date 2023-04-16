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
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "builtin/command.h"
#include "builtin/export.h"
#include "builtin/alias.h"
#include "parse_line.h"
#include "builtin/source.h"

struct source_file *sources[MAX_SOURCE_FILES];

struct source_file *new_source_file(char *source_file_name)
{
	struct source_file *source_file = (struct source_file *)malloc(sizeof(struct source_file));
	// Check if malloc failed
	if (source_file == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	strcpy(source_file->file, source_file_name);

	return source_file;
}


void free_source_file() {
	int i;
	for (i = 0; i < MAX_SOURCE_FILES; i++) {
		if (sources[i] == NULL) {
			break;
		}
		free(sources[i]);
  }
}

int
add_source(char *source_file_name)
{
	int index;
  struct source_file *source_file = new_source_file(source_file_name);

  for (index = 0; index < MAX_SOURCE_FILES; index++) {
    if (sources[index] == NULL) {
      sources[index] = source_file;
      return 0;
    }
  }
  perror("Failed to add new source. Already at limit.\n");
	return 1;
}

int exec_sources() {
  int index;

  for (index = 0; index < MAX_SOURCE_FILES; index++) {
    if (sources[index] == NULL) {
      break;
    }
    read_source_file(sources[index]->file);
  }
  return 0;
}

int
read_source_file(char *filename)
{
	char *buf = malloc(sizeof(char[1024]));

	if (buf == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buf, 0, 1024);
	FILE *f = fopen(filename, "r");

	while (fgets(buf, 1024, f) != NULL) {	/* break with ^D or ^Z */
		if (find_command(buf, NULL, f, NULL, NULL) == -1) {
			//exit_dash();
			fclose(f);
			free(buf);
			return 0;
		}
	}
	if (ferror(f)) {
		err(EXIT_FAILURE, "fgets failed");
	}
	fclose(f);
	free(buf);
	return 1;
}
