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
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "open_files.h"
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "show_prompt.h"

// DECLARE STATIC FUNCTIONS
static char *copy(char *line, struct exec_info *exec_info);
static char *copy_and_end_sub(char *line, struct exec_info *exec_info);
static char *do_glob(char *line, struct exec_info *exec_info);
static char *start_squote(char *line, struct exec_info *exec_info);
static char *end_squote(char *line, struct exec_info *exec_info);
static char *start_dquote(char *line, struct exec_info *exec_info);
static char *end_dquote(char *line, struct exec_info *exec_info);
static char *start_sub(char *line, struct exec_info *exec_info);
static char *tilde_tok(char *line, struct exec_info *exec_info);
static char *end_sub(char *line, struct exec_info *exec_info);
static char *pipe_tok(char *line, struct exec_info *exec_info);
static char *start_file_in(char *line, struct exec_info *exec_info);
static char *start_file_out(char *line, struct exec_info *exec_info);
static char *here_doc(char *line, struct exec_info *exec_info);
static char *end_file(char *line, struct exec_info *exec_info);
static char *end_file_started(char *line, struct exec_info *exec_info);
static char *blank(char *line, struct exec_info *exec_info);
static char *escape(char *line, struct exec_info *exec_info);
static char *esp_escape(char *line, struct exec_info *exec_info);
static char *background(char *line, struct exec_info *exec_info);
static char *subexec(char *line, struct exec_info *exec_info);
static char *or(char *line, struct exec_info *exec_info);
static char *and(char *line, struct exec_info *exec_info);
static char *end_pipe(char *line, struct exec_info *exec_info);
static char *end_line(char *line, struct exec_info *exec_info);
static char *comment(char *line, struct exec_info *exec_info);
static char *request_new_line(char *line, struct exec_info *exec_info);
static char *error(char *line, struct exec_info *exec_info);

static char *parse_ch(char *line, struct exec_info *exec_info);

static int substitute(char *to_substitute);
static void start_file(struct exec_info *exec_info);
static void new_argument(struct exec_info *exec_info);
static char *error_token(char token, char *line);
static int seek(char *line);
static int seekcmd(char *line);

static int load_std_table();
static int load_sub_table();
static int load_file_table();
static int load_sq_table();
static int load_dq_table();
static int load_defaults(spec_char fun, spec_char(*table)[ASCII_CHARS]);

// GLOBAL VARIABLES
static int has_redirect_to_file = 0;
static int require_glob = 0;
static int syntax_error = 0;
static int exec_depth = 0;

static spec_char std[ASCII_CHARS];
static spec_char sub[ASCII_CHARS];
static spec_char file[ASCII_CHARS];
static spec_char sq[ASCII_CHARS];
static spec_char dq[ASCII_CHARS];

int
load_lex_tables()
{
	load_std_table();
	load_sub_table();
	load_file_table();
	load_sq_table();
	load_dq_table();
	return 0;
}

int
load_std_table()
{
	std['\0'] = end_line;
	std['\t'] = blank;
	std['\n'] = blank;
	std[' '] = blank;
	std['"'] = start_dquote;
	std['#'] = comment;
	std['$'] = start_sub;
	std['&'] = background;
	std['\''] = start_squote;
	std['('] = error;
	std[')'] = error;
	std['*'] = do_glob;
	std[';'] = end_pipe;
	std['<'] = start_file_in;
	std['>'] = start_file_out;
	std['?'] = do_glob;
	std['['] = do_glob;
	std['\\'] = escape;
	std['{'] = here_doc;
	std['}'] = error;
	std['|'] = pipe_tok;
	std['~'] = tilde_tok;
	load_defaults(copy, &std);
	return 0;
}

int
load_sub_table()
{
	sub['\0'] = end_sub;
	sub['\t'] = end_sub;
	sub['\n'] = end_sub;
	sub[' '] = end_sub;
	sub['"'] = end_sub;
	sub['#'] = copy_and_end_sub;
	sub['$'] = copy_and_end_sub;
	sub['&'] = end_sub;
	sub['\''] = end_sub;
	sub['('] = end_sub;
	sub[')'] = end_sub;
	sub['-'] = copy_and_end_sub;
	sub[';'] = end_sub;
	sub['<'] = end_sub;
	sub['>'] = end_sub;
	sub['?'] = copy_and_end_sub;
	sub['@'] = copy_and_end_sub;
	sub['\\'] = end_sub;
	sub['_'] = copy_and_end_sub;
	sub['{'] = end_sub;
	sub['}'] = end_sub;
	sub['|'] = end_sub;
	sub['~'] = end_sub;
	load_defaults(copy, &sub);
	return 0;
}

int
load_file_table()
{
	file['\0'] = end_file;
	file['\t'] = end_file_started;
	file['\n'] = end_file;
	file[' '] = end_file_started;
	file['"'] = start_dquote;
	file['#'] = end_file;
	file['$'] = start_sub;
	file['&'] = end_file;
	file['\''] = start_squote;
	file['('] = error;
	file[')'] = error;
	file['*'] = do_glob;
	file[';'] = end_file;
	file['<'] = end_file;
	file['>'] = end_file;
	file['?'] = do_glob;
	file['['] = do_glob;
	file['\\'] = escape;
	file['{'] = error;
	file['}'] = error;
	file['|'] = end_file;
	file['~'] = tilde_tok;
	load_defaults(copy, &file);
	return 0;
}

int
load_sq_table()
{
	sq['\0'] = request_new_line;
	sq['\''] = end_squote;
	load_defaults(copy, &sq);
	return 0;
}

int
load_dq_table()
{
	dq['\0'] = request_new_line;
	dq['"'] = end_dquote;
	dq['$'] = start_sub;
	dq['\\'] = esp_escape;
	load_defaults(copy, &dq);
	return 0;
}

int
load_defaults(spec_char fun, spec_char(*table)[ASCII_CHARS])
{
	// REVIEW: could change
	//int i;

	//for (i = 0; i < ASCII_CHARS; i++)
	//      if ((*table)[i] == NULL)
	//              (*table)[i] = fun;
	return 0;
}

struct parse_info *
new_parse_info()
{
	struct parse_info *parse_info =
	    (struct parse_info *)malloc(sizeof(struct parse_info));

	// Check if malloc failed
	if (parse_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(parse_info, 0, sizeof(struct parse_info));
	parse_info->exec_depth = 0;
	parse_info->request_line = 0;
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer = &std;
	parse_info->old_lexer = parse_info->curr_lexer;

	return parse_info;
}

void
restore_parse_info(struct parse_info *parse_info)
{
	parse_info->exec_depth = 0;
	parse_info->request_line = 0;
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer = &std;
	parse_info->old_lexer = parse_info->curr_lexer;
};

char *
parse(char *ptr, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *cmd = exec_info->last_command;

	has_redirect_to_file = 0;
	syntax_error = 0;
	parse_info->finished = 0;

	parse_info->exec_depth = exec_depth;

	// IF first char is \0 exit immediately
	if (ptr == NULL)
		return NULL;
	if (*ptr == '\0' || *ptr == '#' || *ptr == '\n')
		return NULL;

	cmd->current_arg += strlen(cmd->current_arg);
	parse_info->copy = cmd->current_arg;

	if (strlen(ptr) == MAX_ARGUMENT_SIZE - 1) {
		exec_info->parse_info->request_line = 1;
	}

	for (; !parse_info->finished; ptr++) {
		ptr = parse_ch(ptr, exec_info);

		if (ptr == NULL)
			return ptr;
	}

	cmd = exec_info->last_command;

	if (cmd->current_arg[0] != '\0') {
		new_argument(exec_info);
	} else {
		reset_last_arg(cmd);
	}

	parse_info->finished = 0;
	return ptr;
}

char *
parse_ch(char *line, struct exec_info *exec_info)
{
	// REVIEW: could change
	int index = *line % ASCII_CHARS;
	spec_char fun = (*exec_info->parse_info->curr_lexer)[index];

	if (fun) {
		line = fun(line, exec_info);
	} else {
		line = copy(line, exec_info);
	}
	return line;
}

char *
start_squote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &sq;

	parse_info->has_arg_started = 1;
	return line;
}

char *
end_squote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	spec_char(*tmp_lexer)[256] = parse_info->curr_lexer;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->old_lexer = tmp_lexer;
	return line;
}

char *
start_dquote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &dq;

	parse_info->has_arg_started = 1;

	return line;
}

char *
end_dquote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	spec_char(*tmp_lexer)[256] = parse_info->curr_lexer;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->old_lexer = tmp_lexer;
	return line;
}

char *
start_sub(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct sub_info *sub_info = exec_info->sub_info;

	memset(sub_info->buffer, 0, MAX_ENV_SIZE);

	parse_info->has_arg_started = 1;

	if (strstr(line, "$(") == line) {
		line++;
		return subexec(line, exec_info);
	}

	sub_info->old_ptr = parse_info->copy;
	parse_info->copy = sub_info->buffer;

	sub_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &sub;

	return line;
}

char *
end_sub(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct sub_info *sub_info = exec_info->sub_info;
	struct command *cmd = exec_info->last_command;

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

	if (strlen(to_substitute) == 1) {
		// Could be ? or $ or # or @
		if (*to_substitute == '$') {
			memset(to_substitute, 0, MAX_ENV_SIZE);
			sprintf(to_substitute, "%d", getpid());
			return 2;
		} else if (*to_substitute == '?') {
			sub_result = getenv("result");
			if (sub_result == NULL) {
				return 0;
			}
			memset(to_substitute, 0, MAX_ENV_SIZE);
			strcpy(to_substitute, sub_result);
			return 2;
		} else if (*to_substitute == '#') {
			memset(to_substitute, 0, MAX_ENV_SIZE);
			strcpy(to_substitute, "0");
			return 2;
		}
		// TODO: add all of them
	} else if (strlen(to_substitute) == 0) {
		memset(to_substitute, 0, MAX_ENV_SIZE);
		//strcpy(to_substitute, "$");
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

char *
subexec(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *cmd = exec_info->last_command;
	int n_parenthesis = 1;

	exec_depth++;

	// Read again and parse until )
	char *buffer = malloc(1024);

	if (buffer == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buffer, 0, 1024);

	// Store all in line_buf
	char *line_buf = malloc(1024);

	// Check if malloc failed
	if (line_buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(line_buf, 0, 1024);

	char *ptr;
	char *old_ptr = parse_info->copy;

	parse_info->copy = line_buf;

	//if (strstr(line, "((") == line) {
	//      strcpy(line_buf, "math ");
	//      parse_info->copy += strlen(line_buf);
	//      line++;
	//}
	//FIX: resolve parenthesis
	line++;
	for (ptr = line; ptr != NULL; ptr++) {
		switch (*ptr) {
		case '\0':
			ptr = request_new_line(ptr, exec_info);

			break;
		case '(':
			n_parenthesis++;
			ptr = copy(ptr, exec_info);

			break;
		case ')':
			n_parenthesis--;
			ptr = copy(ptr, exec_info);

			break;
		default:
			ptr = copy(ptr, exec_info);

			break;
		}
		if (n_parenthesis <= 0) {
			*--parse_info->copy = '\0';
			break;
		}
	}
	find_command(line_buf, buffer, stdin, exec_info, NULL);

	if (syntax_error) {
		parse_info->finished = 1;
		free(line_buf);
		free(buffer);
		return NULL;
	}

	exec_depth--;
	parse_info->copy = old_ptr;

	if (parse_info->curr_lexer != &std) {
		strtok(buffer, "\n");
		strcpy(parse_info->copy, buffer);
		cmd->current_arg += strlen(cmd->current_arg);
		parse_info->copy = cmd->current_arg;
	} else {
		parse(buffer, exec_info);
		parse_info->has_arg_started = 0;
	}

	free(line_buf);
	free(buffer);
	return ptr++;
}

void
new_argument(struct exec_info *exec_info)
{
	// TODO: clean
	struct command *cmd = exec_info->last_command;

	if (require_glob && cmd->argc > 0) {
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

	if (strcmp(exec_info->sub_info->last_alias, cmd->argv[0]) != 0) {
		if (check_alias_cmd(cmd)) {
			strcpy(exec_info->sub_info->last_alias, cmd->argv[0]);

			reset_last_arg(cmd);
			cmd->argc = 0;
			cmd->current_arg = cmd->argv[0];
			parse(get_alias(exec_info->sub_info->last_alias),
			      exec_info);
			return;
		}
	}
	add_arg(exec_info->last_command);
}

char *
here_doc(char *line, struct exec_info *exec_info)
{
	struct command *cmd = exec_info->last_command;

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

		if (set_file_cmd(cmd, HERE_DOC_READ, "") < 0) {
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
start_file(struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &file;

	parse_info->copy = exec_info->file_info->buffer;

	parse_info->has_arg_started = 0;
	has_redirect_to_file = 1;

	return;
}

char *
start_file_in(char *line, struct exec_info *exec_info)
{
	start_file(exec_info);
	exec_info->file_info->mode = INPUT_READ;

	return line;
}

char *
start_file_out(char *line, struct exec_info *exec_info)
{
	struct file_info *file_info = exec_info->file_info;
	struct command *cmd = exec_info->last_command;

	start_file(exec_info);
	file_info->mode = OUTPUT_WRITE;
	if (strcmp(cmd->current_arg, "2") == 0) {
		reset_last_arg(cmd);
		file_info->mode = ERROR_WRITE;
	} else if (strcmp(cmd->current_arg, "&") == 0) {
		reset_last_arg(cmd);
		file_info->mode = ERROR_AND_OUTPUT_WRITE;
	} else if (strcmp(cmd->current_arg, "1") == 0) {
		reset_last_arg(cmd);
	}

	return line;
}

char *
end_file(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct file_info *file_info = exec_info->file_info;
	struct command *cmd;
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
end_file_started(char *line, struct exec_info *exec_info)
{
	if (!exec_info->parse_info->has_arg_started) {
		return line;
	}
	return end_file(line, exec_info);
}

char *
tilde_tok(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	// Check if next is ' ' '\n' or '/'then ok if not as '\'                       
	if (parse_info->has_arg_started) {
		line = copy(line, exec_info);

		return line;
	}

	line++;
	if (*line == '/' || *line == ' ' || *line == '\n' || *line == '\t') {
		line--;
		line = start_sub("HOME", exec_info);
	} else {
		line--;
		line = copy(line, exec_info);

		line++;
		line = copy(line, exec_info);
	}

	parse_info->has_arg_started = 1;
	return line;
}

char *
pipe_tok(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *old_cmd = exec_info->last_command;

	// Check if next char is |
	if (strstr(line, "||") == line) {
		return or(line, exec_info);
	}

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
	}

	if (!seekcmd(line)) {
		exec_info->parse_info->request_line = 1;
	}

	exec_info->last_command = new_command();

	// Update old_cmd pipe
	strcpy(exec_info->sub_info->last_alias, "");
	pipe_command(old_cmd, exec_info->last_command);
	parse_info->copy = exec_info->last_command->current_arg;

	parse_info->has_arg_started = 0;
	return line;
}

char *
background(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *command = exec_info->last_command;

	// Check if next char is & or >
	if (strstr(line, "&&") == line) {
		return and(line, exec_info);
	} else if (strstr(line, "&>") == line) {
		line = copy(line, exec_info);

		parse_info->has_arg_started = 1;
		return line;
	}

	line++;
	if (seek(line)) {
		return error_token('&', line);
	}
	line--;

	command->do_wait = DO_NOT_WAIT_TO_FINISH;

	if (set_file_cmd(command, INPUT_READ, "/dev/null") < 0) {
		return NULL;
	}
	parse_info->has_arg_started = 0;
	return line;
}

char *
escape(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	line++;
	// if escape \n do not copy
	if (*line != '\n') {
		line = copy(line, exec_info);
	}

	parse_info->has_arg_started = 1;
	return line;
}

char *
esp_escape(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	line++;

	if (*line == '$' || *line == '"') {
		line = copy(line, exec_info);
	} else if (*line != '\n') {
		line--;
		line = copy(line, exec_info);

		line++;
		line = copy(line, exec_info);
	}

	parse_info->has_arg_started = 1;
	return line;
}

char *
blank(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
		parse_info->copy = exec_info->last_command->current_arg;

		parse_info->has_arg_started = 0;
	}
	return line;
}

char *
end_line(char *line, struct exec_info *exec_info)
{
	if (exec_info->parse_info->request_line) {
		memset(exec_info->line, 0, MAX_ARGUMENT_SIZE);
		fgets(exec_info->line, MAX_ARGUMENT_SIZE, stdin);

		if (ferror(stdin)) {
			fprintf(stderr, "Error: fgets failed");
			return NULL;
		}
		if (strlen(exec_info->line) < MAX_ARGUMENT_SIZE - 1) {
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
comment(char *line, struct exec_info *exec_info)
{
	char *line_buf = malloc(1024);

	if (line_buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(line_buf, 0, 1024);
	while (!exec_info->parse_info->request_line) {
		fgets(line_buf, MAX_ARGUMENT_SIZE, stdin);

		if (ferror(stdin)) {
			fprintf(stderr, "Error: fgets failed");
			return NULL;
		}
		if (strlen(line_buf) < MAX_ARGUMENT_SIZE - 1) {
			exec_info->parse_info->request_line = 0;
		}
	}

	free(line_buf);
	exec_info->parse_info->finished = 1;

	line--;
	return line;
}

char *
end_pipe(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
	}
	parse_info->finished = 1;
	return line;
}

char *
copy(char *line, struct exec_info *exec_info)
{
	*exec_info->parse_info->copy++ = *line;
	exec_info->parse_info->has_arg_started = 1;

	return line;
}

char *
copy_and_end_sub(char *line, struct exec_info *exec_info)
{
	line = copy(line, exec_info);
	line = end_sub(line, exec_info);
	return ++line;
}

char *
request_new_line(char *line, struct exec_info *exec_info)
{
	prompt_request();
	fgets(line, MAX_ARGUMENT_SIZE, stdin);
	if (ferror(stdin)) {
		fprintf(stderr, "Error: fgets failed");
		return NULL;
	}
	exec_info->parse_info->has_arg_started = 0;

	return --line;
}

static char *
do_glob(char *line, struct exec_info *exec_info)
{
	require_glob = 1;
	exec_info->parse_info->has_arg_started = 1;
	return copy(line, exec_info);
}

char *
and(char *line, struct exec_info *exec_info)
{
	line++;
	// Add an argument to old command
	if (exec_info->parse_info->has_arg_started) {
		new_argument(exec_info);
	}
	exec_info->last_command->next_status_needed_to_exec =
	    EXECUTE_IN_SUCCESS;
	exec_info->parse_info->finished = 1;
	return ++line;
}

char *
or(char *line, struct exec_info *exec_info)
{
	line++;
	// Add an argument to old command
	if (exec_info->parse_info->has_arg_started) {
		new_argument(exec_info);
	}
	exec_info->last_command->next_status_needed_to_exec =
	    EXECUTE_IN_FAILURE;
	exec_info->parse_info->finished = 1;
	return line;
}

char *
error(char *line, struct exec_info *exec_info)
{
	exec_info->parse_info->finished = 1;
	syntax_error = 1;

	error_token(*line, line);
	return NULL;
}

char *
error_token(char token, char *line)
{
	strtok(line, "\n");
	fprintf(stderr,
		"Mash: syntax error in '%c' near unexpected token `%s\n", token,
		line);
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
	spec_char fun;

	for (ptr = line; *ptr != '\0'; ptr++) {
		fun = std[(int)*ptr];
		if (fun == NULL)
			return 1;
	}
	return 0;
}
