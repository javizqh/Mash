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

enum {
	MAX_SOURCE_FILES = 64,	// In bytes
	MAX_FILE_LENGTH = 256  // In bytes
};

struct source_file {
	char file[MAX_FILE_LENGTH];
};

struct source_file *new_source_file(char *source_file_name);

void free_source_file();

int add_source(char *source_file_name);

int exec_sources();

int read_source_file(char *filename);
