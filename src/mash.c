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

#include "mash.h"

int
main(int argc, char **argv)
{
	add_source("env/.mashrc");
	exec_sources();

	// ---------- Read command line
	// ------ Buffer
	// Initialize buffer
	char *buf = malloc(sizeof(char[1024]));

	// Check if malloc failed
	if (buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	// Initialize buffer to 0
	memset(buf, 0, 1024);
	// ------------

	printf("\033[01;35m%s \033[0m", getenv("PROMPT"));
	while (fgets(buf, 1024, stdin) != NULL) {	/* break with ^D or ^Z */
		find_command(buf, NULL, stdin);
		// Print Prompt
		printf("\033[01;35m%s \033[0m", getenv("PROMPT"));
	}
	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}

	exit_mash();
	free(buf);
	return 0;
}
