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

#include <stdio.h>
#include <string.h>
#include "exec_cmd.h"

enum {
	MAX_COMMANDS_PER_LINE = 32
};

int main(int argc, char **argv);

// ------- Source files -------

int read_source_file(char *filename);

// --------------- Parse Arguments -----------
// -------- Command Array -----------

struct cmd_array {
	struct command *commands[MAX_COMMANDS_PER_LINE];
	int n_cmd;
	int status;
};

struct cmd_array *new_commands();

int add_command(struct command *command_to_add, struct cmd_array *commands);

int do_not_wait_commands(struct cmd_array *cmd_array);

void free_cmd_array(struct cmd_array *cmd_array);

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

struct cmd_array *set_commands(char *line);

// --------------- End Parse Arguments -----------

// --------- Command ----------

int find_command(char *line, char *buffer, FILE * src_file);

int set_command_file(struct cmd_array *commands, int file_type, char *file);

// -------- Exit -----------
// TODO: temporary solution
int is_exit(struct command *command);

// ------- Substitution -------

struct sub_info {
	char *ptr;
	char buffer[MAX_ENV_SIZE];
};

struct sub_info *new_sub_info();

char *substitute(const char *to_substitute);

// --------- Files ------------

struct file_info {
	int mode;
	char *ptr;
	char buffer[MAX_ARGUMENT_SIZE];
};

struct file_info *new_file_info();

// ---------- Exit ------------

int exit_dash();

// New TOKENIZATION Recursive

enum token {
	CREATE_COMMAND_TO_START,
	USE_PREVIOUS_COMMAND_TO_START
};

int cmd_tokenize(char *line, struct parse_info *parse_info,
		 struct cmd_array *cmd_array, struct file_info *file_info,
		 struct sub_info *sub_info);

// Tokenize types
char *hard_apost_tokenize(char *line, struct parse_info *parse_info);

char *soft_apost_tokenize(char *line, struct parse_info *parse_info,
			  struct cmd_array *cmd_array,
			  struct file_info *file_info,
			  struct sub_info *sub_info);

char *substitution_tokenize(char *line, struct parse_info *parse_info,
			    struct cmd_array *cmd_array,
			    struct file_info *file_info,
			    struct sub_info *sub_info);

int copy_substitution(struct parse_info *parse_info, const char *sub_buffer);

char *file_tokenize(char *line, struct parse_info *parse_info,
		    struct cmd_array *cmd_array, struct file_info *file_info,
		    struct sub_info *sub_info);

char *execute_token(char *line, struct parse_info *parse_info,
		    struct cmd_array *cmd_array, struct file_info *file_info,
		    struct sub_info *sub_info);

void request_new_line(char *line);

int error_token(char token, char *line);
