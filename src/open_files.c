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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "open_files.h"

int
open_read_file(char *filename)
{
	int fd;

	fd = open(filename, O_RDONLY);

	// Check if error occurred
	if (fd == -1) {
		fprintf(stderr, "dash: %s: No such file or directory\n",
			filename);
	}
	return fd;
}

int
open_write_file(char *filename)
{
	int fd;

	fd = open(filename, O_TRUNC | O_WRONLY);
	// Check if error occurred, file not found
	if (fd == -1) {
		fd = open(filename, O_CREAT | O_WRONLY, 0777);
	}
	// Check if error occurred again
	if (fd == -1) {
		err(EXIT_FAILURE, "open failed");
	}
	return fd;
}

char *
new_here_doc_buffer()
{
	char *hd_buffer = malloc(MAX_HERE_DOC_BUFFER);

	if (hd_buffer == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(hd_buffer, 0, MAX_HERE_DOC_BUFFER);

	return hd_buffer;
}
