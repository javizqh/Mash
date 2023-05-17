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

typedef struct SubInfo {
	char *old_ptr;
	char buffer[MAX_ARGUMENT_SIZE];
	 spec_char(*old_lexer)[ASCII_CHARS];
} SubInfo;

SubInfo *new_sub_info();
void restore_sub_info(SubInfo *sub_info);

typedef struct FileInfo {
	int mode;
	char *ptr;
	char buffer[MAX_ARGUMENT_SIZE];
} FileInfo;

FileInfo *new_file_info();
void restore_file_info(FileInfo *file_info);

typedef struct ExecInfo {
	Command *command;
	Command *last_command;
	ParseInfo *parse_info;
	FileInfo *file_info;
	SubInfo *sub_info;
	char *line;
} ExecInfo;

ExecInfo *new_exec_info(char *line);
void reset_exec_info(ExecInfo *exec_info);
void free_exec_info(ExecInfo *exec_info);
