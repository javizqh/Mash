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

extern int syntax_mode;

enum syntax_mode {
	BASIC_SYNTAX,
	EXTENDED_SYNTAX
};

enum ascii {
	ASCII_CHARS = 256
};

struct ExecInfo;

typedef char *(*spec_char)(char *, struct ExecInfo *);

int load_lex_tables();
int load_basic_lex_tables();

typedef struct ParseInfo {
	int exec_depth;
	int request_line;
	int has_arg_started;
	int finished;
	char *copy;
	spec_char (*curr_lexer)[ASCII_CHARS];
	spec_char (*old_lexer)[ASCII_CHARS];
} ParseInfo;

ParseInfo *new_parse_info();
void restore_parse_info(ParseInfo *parse_info);

char *parse(char *line, struct ExecInfo *exec_info);