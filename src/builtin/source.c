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

// DECLARE STATIC FUNCTIONS
static int usage();

// DECLARE GLOBAL VARIABLE
struct source_file *sources[MAX_SOURCE_FILES];

static int usage() {
	fprintf(stderr,"Usage: source filename\n");
	return EXIT_FAILURE;
}

int source(int argc, char *argv[]) {
	struct stat buffer;
	argc--; argv++;
	if (argc != 1) {
		return usage();
	}
	if (strcmp(argv[0], "--help") == 0) {
		usage();
		return EXIT_SUCCESS;
	}
	// Check if file exists
	if (stat(argv[0], &buffer) < 0) {
		fprintf(stderr,"Mash: source: %s no such file in directory\n", argv[0]);
		return EXIT_FAILURE;
	}
	return add_source(argv[0]);
}

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
	fprintf(stderr,"Failed to add new source. Already at limit.\n");
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
	char *buf = malloc(MAX_ARGUMENT_SIZE);

	if (buf == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buf, 0, MAX_ARGUMENT_SIZE);
	FILE *f = fopen(filename, "r");

	while (fgets(buf, MAX_ARGUMENT_SIZE, f) != NULL) {	/* break with ^D or ^Z */
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
