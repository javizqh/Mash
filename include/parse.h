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

enum ascii {
	ASCII_CHARS = 256
};

struct exec_info;

typedef char *(*spec_char)(char *, struct exec_info *);

int load_lex_tables();

struct parse_info {
	int request_line;
	int has_arg_started;
	int finished;
	char *copy;
	spec_char (*curr_lexer)[ASCII_CHARS];
	spec_char (*old_lexer)[ASCII_CHARS];
};

struct parse_info *new_parse_info();
void restore_parse_info(struct parse_info *parse_info);

char *parse(char *line, struct exec_info *exec_info);