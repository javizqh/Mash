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
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "builtin/source.h"

// DECLARE GLOBAL VARIABLE
char *source_use = "source filename";
char *source_description = "Execute commands from a file in the current shell.";
char *source_help =
    "    Read and execute commands from FILENAME in the current shell.  The\n"
    "    entries in $PATH are used to find the directory containing FILENAME.\n\n"
    "    Exit Status:\n"
    "    Returns success unless FILENAME cannot be read.\n";

struct source_file *sources[MAX_SOURCE_FILES];

static int out_fd;
static int err_fd;

static int
help()
{
	dprintf(out_fd, "source: %s\n", source_use);
	dprintf(out_fd, "    %s\n\n%s", source_description, source_help);
	return EXIT_SUCCESS;
}

static int
usage()
{
	dprintf(err_fd, "Usage: %s\n", source_use);
	return EXIT_FAILURE;
}

int
source(int argc, char *argv[], int stdout_fd, int stderr_fd)
{
	argc--;
	argv++;

	out_fd = stdout_fd;
	err_fd = stderr_fd;

	if (argc != 1) {
		return usage();
	}
	if (strcmp(argv[0], "--help") == 0) {
		return help();
	}

	return add_source(argv[0], stderr_fd);
}

struct source_file *
new_source_file(char *source_file_name)
{
	struct source_file *source_file =
	    (struct source_file *)malloc(sizeof(struct source_file));
	// Check if malloc failed
	if (source_file == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	strcpy(source_file->file, source_file_name);

	return source_file;
}

void
free_source_file()
{
	int i;

	for (i = 0; i < MAX_SOURCE_FILES; i++) {
		if (sources[i] == NULL) {
			break;
		}
		free(sources[i]);
	}
}

int
add_source(char *source_file_name, int error_fd)
{
	int index;

	char *source_name = malloc(MAX_FILE_LENGTH);

	if (source_name == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(source_name, 0, MAX_FILE_LENGTH);

	strcpy(source_name, source_file_name);

	if (!find_path_srcfile(source_name)) {
		dprintf(err_fd, "Mash: source: %s no such file in directory\n",
			source_file_name);
		free(source_name);
		return EXIT_FAILURE;
	}

	struct source_file *source_file = new_source_file(source_name);

	free(source_name);

	for (index = 0; index < MAX_SOURCE_FILES; index++) {
		if (sources[index] == NULL) {
			sources[index] = source_file;
			return 0;
		}
	}
	dprintf(error_fd, "Failed to add new source. Already at limit.\n");
	return 1;
}

int
exec_sources()
{
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

int
find_path_srcfile(char *filename)
{
	int i;
	char *cwd;
	char *cwd_ptr;

	char *path, *orig_path;
	char *path_ptr;
	int path_len = 0;
	char path_tok[MAX_PATH_LOCATIONS][MAX_ENV_SIZE];
	char *token;

	// Check if the first character is /
	if (*filename == '/' && file_exists(filename)) {
		return 1;
	}
	// CHECK IN CWD
	cwd = malloc(MAX_ENV_SIZE);

	if (cwd == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(cwd, 0, MAX_ENV_SIZE);
	// Copy the path
	if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
		free(cwd);
		return 0;
	}
	cwd_ptr = cwd;

	strcat(cwd_ptr, "/");
	strcat(cwd_ptr, filename);

	if (file_exists(cwd_ptr)) {
		strcpy(filename, cwd_ptr);
		free(cwd);
		return 1;
	}
	free(cwd);

	// SEARCH IN PATH
	// First get the path from env PATH
	path = malloc(MAX_PATH_SIZE);

	if (path == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(path, 0, MAX_PATH_SIZE);
	// Copy the path
	orig_path = getenv("PATH");
	if (orig_path == NULL || strlen(orig_path) > MAX_PATH_SIZE - 1) {
		free(path);
		return 0;
	}
	strcpy(path, orig_path);
	path_ptr = path;

	// Then separate the path by : using strtok
	while ((token = strtok_r(path_ptr, ":", &path_ptr))) {
		strcpy(path_tok[path_len], token);
		path_len++;
	}
	// Loop through the path until we find an exec

	for (i = 0; i < path_len; i++) {
		strcat(path_tok[i], "/");
		strcat(path_tok[i], filename);
		if (file_exists(path_tok[i])) {
			strcpy(filename, path_tok[i]);
			free(path);
			return 1;
		}
	}
	free(path);
	return 0;
}

int
file_exists(char *path)
{
	struct stat buf;

	return (stat(path, &buf) == 0);
}
