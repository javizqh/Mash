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


enum parse_info_enum {
	PARSE_ARG_NOT_STARTED,
	PARSE_ARG_STARTED
};

struct parse_info {
	int has_arg_started;
	int do_not_expect_new_cmd;
	char *copy;
};

struct parse_info *new_parse_info();
void restore_parse_info(struct parse_info *parse_info);

// --------------- End Parse Arguments -----------

// --------- Command ----------


// ------- Substitution -------

struct sub_info {
	char last_alias[ALIAS_MAX_COMMAND];
	char *ptr;
	char buffer[MAX_ENV_SIZE];
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

char * cmd_tokenize(char *line, struct exec_info *exec_info);

// Tokenize types
char *hard_apost_tokenize(char *line, struct parse_info *parse_info);

char *soft_apost_tokenize(char *line, struct exec_info *exec_info);

char *substitution_tokenize(char *line, struct exec_info *exec_info);

int copy_substitution(struct parse_info *parse_info, const char *sub_buffer);

char *file_tokenize(char *line, struct exec_info *exec_info);

char *glob_tokenize(char *line,struct exec_info *exec_info);

char *execute_token(char *line, struct exec_info *exec_info);

void request_new_line(char *line);

void new_argument(struct exec_info *exec_info);

char * error_token(char token, char *line);

int check_here_doc(char *line, struct command *command);

char * cmdtok_redirect_in(char *line, struct exec_info *exec_info);

char * cmdtok_redirect_out(char *line, struct exec_info *exec_info);

char * tilde_tokenize(char *line, struct exec_info *exec_info);

