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
#include <unistd.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/exit.h"
#include "open_files.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "exec_cmd.h"
#include "mash.h"

// DECLARE STATIC FUNCTIONS
static char *copy(char *line, ExecInfo * exec_info);

// static char *copy_and_end_sub(char *line, ExecInfo * exec_info);
static char *do_glob(char *line, ExecInfo * exec_info);
static char *basic_start_sub(char *line, ExecInfo * exec_info);
static char *end_sub(char *line, ExecInfo * exec_info);
static char *basic_pipe_tok(char *line, ExecInfo * exec_info);
static char *basic_start_file_in(char *line, ExecInfo * exec_info);
static char *basic_start_file_out(char *line, ExecInfo * exec_info);
static char *here_doc(char *line, ExecInfo * exec_info);
static char *end_file(char *line, ExecInfo * exec_info);
static char *end_basic_file(char *line, ExecInfo * exec_info);
static char *end_basic_file_started(char *line, ExecInfo * exec_info);
static char *blank(char *line, ExecInfo * exec_info);
static char *basic_background(char *line, ExecInfo * exec_info);
static char *end_line(char *line, ExecInfo * exec_info);

// static char *error(char *line, ExecInfo * exec_info);

static char *parse_ch(char *line, ExecInfo * exec_info);

static int substitute(char *to_substitute);
static void start_file(ExecInfo * exec_info);
static void new_argument(ExecInfo * exec_info);
static char *error_token(char token, char *line);
static int seek(char *line);
static int seekcmd(char *line);
static int seekfile(char *line, char filetype);

static int load_basic_std_table();
static int load_sub_table();
static int load_basic_file_table();

// GLOBAL VARIABLES
static int has_redirect_to_file = 0;
static int require_glob = 0;
static int syntax_error = 0;

static spec_char std[ASCII_CHARS];
static spec_char sub[ASCII_CHARS];
static spec_char file[ASCII_CHARS];

int
load_basic_lex_tables()
{
	load_basic_std_table();
	load_sub_table();
	load_basic_file_table();
	return 0;
}

int
load_basic_std_table()
{
	std['\0'] = end_line;
	std['\t'] = blank;
	std['\n'] = blank;
	std[' '] = blank;
	std['$'] = basic_start_sub;
	std['&'] = basic_background;
	std['*'] = do_glob;
	std['<'] = basic_start_file_in;
	std['>'] = basic_start_file_out;
	std['?'] = do_glob;
	std['['] = do_glob;
	std['{'] = here_doc;
	std['|'] = basic_pipe_tok;
	return 0;
}

int
load_sub_table()
{
	sub['\0'] = end_sub;
	sub['\t'] = end_sub;
	sub['\n'] = end_sub;
	sub[' '] = end_sub;
	sub['&'] = end_sub;
	sub['\''] = end_sub;
	sub['<'] = end_sub;
	sub['>'] = end_sub;
	sub['{'] = end_sub;
	sub['|'] = end_sub;
	return 0;
}

int
load_basic_file_table()
{
	file['\0'] = end_basic_file;
	file['\t'] = end_basic_file_started;
	file['\n'] = end_basic_file;
	file[' '] = end_basic_file_started;
	file['$'] = basic_start_sub;
	file['&'] = end_basic_file;
	file['*'] = do_glob;
	file['<'] = end_basic_file;
	file['>'] = end_basic_file;
	file['?'] = do_glob;
	file['['] = do_glob;
	file['|'] = end_basic_file;
	return 0;
}

ParseInfo *
new_parse_info()
{
	ParseInfo *parse_info = (ParseInfo *) malloc(sizeof(ParseInfo));

	// Check if malloc failed
	if (parse_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(parse_info, 0, sizeof(ParseInfo));
	parse_info->request_line = 0;
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer = &std;
	parse_info->old_lexer = parse_info->curr_lexer;

	return parse_info;
}

void
restore_parse_info(ParseInfo * parse_info)
{
	parse_info->request_line = 0;
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer = &std;
	parse_info->old_lexer = parse_info->curr_lexer;
};

char *
parse(char *ptr, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	Command *cmd = exec_info->last_command;

	has_redirect_to_file = 0;
	syntax_error = 0;
	parse_info->finished = 0;

	// IF first char is \0 exit immediately
	if (ptr == NULL)
		return NULL;
	if (*ptr == '\0' || *ptr == '#' || *ptr == '\n')
		return NULL;

	cmd->current_arg += strlen(cmd->current_arg);
	parse_info->copy = cmd->current_arg;

	if (strlen(ptr) == MAX_ARGUMENT_SIZE - 1) {
		parse_info->request_line = 1;
	}

	for (; !parse_info->finished; ptr++) {
		ptr = parse_ch(ptr, exec_info);

		if (ptr == NULL) {
			close_all_fd(exec_info->command);
			return ptr;
		}
	}

	cmd = exec_info->last_command;

	if (strlen(cmd->current_arg) > 0) {
		new_argument(exec_info);
	} else {
		reset_last_arg(cmd);
	}

	if (strlen(exec_info->command->argv[0]) == 0) {
		return NULL;
	}

	parse_info->finished = 0;
	return ptr;
}

char *
parse_ch(char *line, ExecInfo * exec_info)
{
	int index = *line % ASCII_CHARS;

	if (index < 0)
		index += ASCII_CHARS;
	spec_char fun = (*exec_info->parse_info->curr_lexer)[index];

	if (fun) {
		line = fun(line, exec_info);
	} else {
		line = copy(line, exec_info);
	}
	return line;
}

char *
basic_start_sub(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	SubInfo *sub_info = exec_info->sub_info;

	memset(sub_info->buffer, 0, MAX_ENV_SIZE);

	parse_info->has_arg_started = 1;

	sub_info->old_ptr = parse_info->copy;
	parse_info->copy = sub_info->buffer;

	sub_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &sub;

	return line;
}

char *
end_sub(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	SubInfo *sub_info = exec_info->sub_info;
	Command *cmd = exec_info->last_command;

	parse_info->curr_lexer = sub_info->old_lexer;
	parse_info->copy = sub_info->old_ptr;

	switch (substitute(sub_info->buffer)) {
	case 1:
		if (parse_info->curr_lexer != &std) {
			strcpy(parse_info->copy, sub_info->buffer);
			cmd->current_arg += strlen(cmd->current_arg);
		} else {
			parse(sub_info->buffer, exec_info);
			parse_info->has_arg_started = 0;
		}
		break;
	case 2:
		strcpy(parse_info->copy, sub_info->buffer);
		cmd->current_arg += strlen(cmd->current_arg);
		break;
	default:
		return NULL;
		break;
	}

	parse_info->copy = cmd->current_arg;

	return --line;
}

int
substitute(char *to_substitute)
{
	char *sub_result;

	if (strlen(to_substitute) == 0) {
		memset(to_substitute, 0, MAX_ENV_SIZE);
		strcpy(to_substitute, "$");
		return 2;
	}

	sub_result = getenv(to_substitute);
	if (sub_result == NULL) {
		fprintf(stderr, "error: var %s does not exist\n",
			to_substitute);
		return 0;
	} else {
		memset(to_substitute, 0, MAX_ENV_SIZE);
		strcpy(to_substitute, sub_result);
	}
	return 1;
}

void
new_argument(ExecInfo * exec_info)
{
	Command *cmd = exec_info->last_command;

	if (require_glob) {
		require_glob = 0;
		// Do substitution and update command
		glob_t gstruct;

		if (glob(cmd->current_arg, GLOB_ERR, NULL, &gstruct) ==
		    GLOB_NOESCAPE) {
			fprintf(stderr, "Error: glob failed");
			globfree(&gstruct);
			return;
		}

		char **found;

		found = gstruct.gl_pathv;

		if (found != NULL && strcmp(cmd->current_arg, *found) != 0) {
			while (*found != NULL) {
				strcpy(cmd->current_arg, *found);
				add_arg(cmd);
				found++;
			}
		}

		globfree(&gstruct);
		add_arg(cmd);
		return;
	}

	add_arg(exec_info->last_command);
}

char *
here_doc(char *line, ExecInfo * exec_info)
{
	Command *cmd = exec_info->last_command;

	if (strcmp(cmd->current_arg, "HERE") == 0) {
		if (seek(++line)) {
			fprintf(stderr,
				"Mash: Error: text behind here document\n");
			return NULL;
		}
		line--;

		if (has_redirect_to_file) {
			fprintf(stderr,
				"Mash: Error: redirection before here document\n");
			return NULL;
		}

		if (set_file_cmd(exec_info->command, HERE_DOC_READ, "") < 0) {
			fprintf(stderr,
				"Mash: Error: failed redirection to here document\n");
			return NULL;
		}
		reset_last_arg(cmd);
		cmd->argc--;
		exec_info->parse_info->copy = cmd->current_arg;

		return line;
	}
	error_token('{', line);
	return NULL;
}

void
start_file(ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &file;

	parse_info->copy = exec_info->file_info->buffer;

	parse_info->has_arg_started = 0;
	has_redirect_to_file = 1;

	return;
}

char *
basic_start_file_in(char *line, ExecInfo * exec_info)
{
	start_file(exec_info);
	exec_info->file_info->mode = INPUT_READ;

	return line;
}

char *
basic_start_file_out(char *line, ExecInfo * exec_info)
{
	start_file(exec_info);
	exec_info->file_info->mode = OUTPUT_WRITE;
	return line;
}

char *
end_file(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	FileInfo *file_info = exec_info->file_info;
	Command *cmd;
	glob_t gstruct;
	char **found;
	int glob_found = 0;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &std;

	if (!parse_info->has_arg_started) {
		return error_token(*line, line);
	}

	if (file_info->mode == INPUT_READ) {
		cmd = exec_info->command;
	} else {
		cmd = exec_info->last_command;
	}

	if (require_glob) {
		require_glob = 0;
		if (glob(file_info->buffer, GLOB_ERR, NULL, &gstruct) ==
		    GLOB_NOESCAPE) {
			fprintf(stderr, "Error: glob failed");
			globfree(&gstruct);
			return NULL;
		}

		found = gstruct.gl_pathv;

		if (found != NULL && strcmp(cmd->current_arg, *found) != 0) {
			while (*found != NULL) {
				glob_found++;
				if (glob_found > 1) {
					fprintf(stderr,
						"Mash: error: ambiguous redirect\n");
					globfree(&gstruct);
					return NULL;
				}
				strcpy(file_info->buffer, *found);
				found++;
			}
		}
		globfree(&gstruct);
	}

	if (set_file_cmd(cmd, file_info->mode, file_info->buffer) < 0) {
		return NULL;
	}

	cmd->current_arg += strlen(cmd->current_arg);
	parse_info->copy = cmd->current_arg;
	parse_info->has_arg_started = 0;

	return --line;
}

char *
end_basic_file(char *line, ExecInfo * exec_info)
{
	char filetype;

	if (exec_info->file_info->mode == INPUT_READ) {
		filetype = '>';
	} else {
		filetype = '<';
	}
	if (seekfile(line, filetype)) {
		return error_token(*line, line);
	}
	return end_file(line, exec_info);
}

char *
end_basic_file_started(char *line, ExecInfo * exec_info)
{
	if (!exec_info->parse_info->has_arg_started) {
		return line;
	}
	return end_basic_file(line, exec_info);
}

char *
basic_pipe_tok(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	Command *old_cmd = exec_info->last_command;

	if (strlen(old_cmd->argv[0]) == 0) {
		return error_token('|', line);
	}

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
	}

	if (!seekcmd(line)) {
		exec_info->parse_info->request_line = 1;
	}

	exec_info->last_command = new_command();

	// Update old_cmd pipe
	pipe_command(old_cmd, exec_info->last_command);
	parse_info->copy = exec_info->last_command->current_arg;

	parse_info->has_arg_started = 0;
	return line;
}

char *
basic_background(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;
	Command *command = exec_info->last_command;

	line++;
	if (seek(line)) {
		return error_token('&', line);
	}
	line--;

	command->do_wait = DO_NOT_WAIT_TO_FINISH;

	if (command->input == STDIN_FILENO) {
		if (set_file_cmd(exec_info->command, INPUT_READ, "/dev/null") <
		    0) {
			return NULL;
		}
	}
	parse_info->has_arg_started = 0;
	return line;
}

char *
blank(char *line, ExecInfo * exec_info)
{
	ParseInfo *parse_info = exec_info->parse_info;

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
		parse_info->copy = exec_info->last_command->current_arg;

		parse_info->has_arg_started = 0;
	}
	return line;
}

char *
end_line(char *line, ExecInfo * exec_info)
{
	if (exec_info->parse_info->request_line) {
		memset(exec_info->line, 0, LINE_SIZE);
		prompt_request();
		fgets(exec_info->line, LINE_SIZE, stdin);

		if (ferror(stdin)) {
			fprintf(stderr, "Error: fgets failed");
			return NULL;
		}
		if (strlen(exec_info->line) < LINE_SIZE - 1) {
			exec_info->parse_info->request_line = 0;
		}
		line = exec_info->line;
	} else {
		exec_info->parse_info->finished = 1;
	}
	line--;
	return line;
}

char *
copy(char *line, ExecInfo * exec_info)
{
	*exec_info->parse_info->copy++ = *line;
	exec_info->parse_info->has_arg_started = 1;

	return line;
}

// TODO: maybe
//char *
//copy_and_end_sub(char *line, ExecInfo * exec_info)
//{
//      line = copy(line, exec_info);
//      line = end_sub(line, exec_info);
//      return ++line;
//}

static char *
do_glob(char *line, ExecInfo * exec_info)
{
	require_glob = 1;
	exec_info->parse_info->has_arg_started = 1;
	return copy(line, exec_info);
}

// TODO: maybe
//char *
//error(char *line, ExecInfo * exec_info)
//{
//      exec_info->parse_info->finished = 1;
//      error_token(*line, line);
//      return NULL;
//}

char *
error_token(char token, char *line)
{
	syntax_error = 1;
	if (token == '\n') {
		fprintf(stderr,
			"Mash: syntax error near unexpected token `newline'\n");
		return NULL;
	}

	strtok(line, "\n");
	fprintf(stderr,
		"Mash: syntax error in '%c' near unexpected token `%s'\n",
		token, line);
	return NULL;
}

int
seek(char *line)
{
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		if (*ptr != ' ' && *ptr != '\n' && *ptr != '\t')
			return 1;
	}
	return 0;
}

int
seekcmd(char *line)
{
	char *ptr;
	int index;
	spec_char fun;

	for (ptr = line; *ptr != '\0'; ptr++) {
		index = *ptr % ASCII_CHARS;
		if (index < 0)
			index += ASCII_CHARS;
		fun = std[index];
		if (fun == NULL)
			return 1;
	}
	return 0;
}

int
seekfile(char *line, char filetype)
{
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		if (*ptr != ' ' && *ptr != '\n' && *ptr != '\t') {
			if (*ptr == filetype || *ptr == '&') {
				return 0;
			} else {
				return 1;
			}
		}
	}
	return 0;
}
