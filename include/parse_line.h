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

// ---------- Tokenization -----------

struct exec_info;

typedef struct lexer {
	char token;
	char *(* fun)(char *, struct exec_info *);
} lexer;

struct parse_info {
	int request_line;
	int has_arg_started;
	int finished;
	char *copy;
	int curr_lexer_size;
	const lexer (*curr_lexer)[];
	int old_lexer_size;
	const lexer (*old_lexer)[];
};

struct parse_info *new_parse_info();
void restore_parse_info(struct parse_info *parse_info);

// --------------- End Parse Arguments -----------

// --------- Command ----------


// ------- Substitution -------

struct sub_info {
	char last_alias[ALIAS_MAX_COMMAND];
	char *old_ptr;
	char buffer[MAX_ENV_SIZE];
	char *exec_buffer;
	int exec_depth;
	int old_lexer_size;
	const lexer (*old_lexer)[];
};

struct sub_info *new_sub_info();
void restore_sub_info(struct sub_info *sub_info);

char *substitute(const char *to_substitute);

// --------- Files ------------

struct file_info {
	int mode;
	char *ptr;
	char buffer[MAX_ARGUMENT_SIZE];
};

struct file_info *new_file_info();
void restore_file_info(struct file_info *file_info);
struct exec_info {
	struct command *command;
	struct command *last_command;
	struct parse_info *parse_info;
	struct file_info *file_info;
	struct sub_info *sub_info;
	char *line;
	struct exec_info *prev_exec_info;
};

struct exec_info * new_exec_info(char *line);
void reset_exec_info(struct exec_info * exec_info);
void free_exec_info(struct exec_info * exec_info);

// New TOKENIZATION Recursive
int find_command(char *line, char *buffer, FILE * src_file, struct exec_info * prev_exec_info, char * to_free_excess);

enum token {
	CREATE_COMMAND_TO_START,
	USE_PREVIOUS_COMMAND_TO_START
};

