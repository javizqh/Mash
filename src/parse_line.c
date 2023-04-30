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
#include "parse_line.h"
#include "show_prompt.h"
#include "exec_cmd.h"
#include "builtin/jobs.h"

// DECLARE STATIC FUNCTIONS
// Tokenize types
static char *start_squote(char *line, struct exec_info *exec_info);
static char *end_squote(char *line, struct exec_info *exec_info);
static char *start_dquote(char *line, struct exec_info *exec_info);
static char *end_dquote(char *line, struct exec_info *exec_info);
static char *pipe_tok(char *line, struct exec_info *exec_info);
static char *start_sub(char *line, struct exec_info *exec_info);
static char *end_sub(char *line, struct exec_info *exec_info);
static char *start_file_in(char *line, struct exec_info *exec_info);
static char *start_file_out(char *line, struct exec_info *exec_info);
static char *end_file(char *line, struct exec_info *exec_info);
static char *end_file_started(char *line, struct exec_info *exec_info);
static char *background(char *line, struct exec_info *exec_info);
static char *escape(char *line, struct exec_info *exec_info);
static char *esp_escape(char *line, struct exec_info *exec_info);
static char *blank(char *line, struct exec_info *exec_info);
static char *end_line(char *line, struct exec_info *exec_info);
static char *comment(char *line, struct exec_info *exec_info);
static char *end_pipe(char *line, struct exec_info *exec_info);
static char *copy(char *line, struct exec_info *exec_info);
static char *copy_and_end(char *line, struct exec_info *exec_info);
static char *error(char *line, struct exec_info *exec_info);
static char *tilde_tokenize(char *line, struct exec_info *exec_info);	// TODO: do not use 
static char *start_subexec(char *line, struct exec_info *exec_info);
static char *start_math(char *line, struct exec_info *exec_info);
static char *request_new_line(char *line, struct exec_info *exec_info);
static char *here_doc(char *line, struct exec_info *exec_info);

static char *exec_lexer(char *line, struct exec_info *exec_info);

static int copy_substitution(struct parse_info *parse_info,
			     const char *sub_buffer);
static void new_argument(struct exec_info *exec_info);
static char *error_token(char token, char *line);
static int seek(char *line);

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
	std[';'] = end_pipe;
	std['<'] = start_file_in;
	std['>'] = start_file_out;
	std['\\'] = escape;
	std['{'] = here_doc;
	std['}'] = error;
	std['|'] = pipe_tok;
	std['~'] = tilde_tokenize;
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
	sub['#'] = copy_and_end;
	sub['$'] = copy_and_end;
	sub['&'] = end_sub;
	sub['\''] = end_sub;
	sub['('] = end_sub;
	sub[')'] = end_sub;
	sub['-'] = copy_and_end;
	sub[';'] = end_sub;
	sub['<'] = end_sub;
	sub['>'] = end_sub;
	sub['?'] = copy_and_end;
	sub['@'] = copy_and_end;
	sub['\\'] = end_sub;
	sub['_'] = copy_and_end;
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
	file['*'] = error;
	file[';'] = end_file;
	file['<'] = end_file;
	file['>'] = end_file;
	file['?'] = error;
	file['['] = error;
	file['\\'] = escape;
	file['{'] = error;
	file['}'] = error;
	file['|'] = end_file;
	file['~'] = tilde_tokenize;
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
	int i;

	for (i = 0; i < ASCII_CHARS; i++)
		if ((*table)[i] == NULL)
			(*table)[i] = fun;
	return 0;
}

// ------ Parse info --------

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
	parse_info->request_line = 0;
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer = &std;
	parse_info->old_lexer = parse_info->curr_lexer;
};

// ---------- Commands -------------
int
find_command(char *line, char *buffer, FILE * src_file,
	     struct exec_info *prev_exec_info, char *to_free_excess)
{
	int status = 0;
	int status_for_next_cmd = DO_NOT_MATTER_TO_EXEC;
	char *orig_line_ptr = line;
	char cwd[MAX_ENV_SIZE];
	char result[4];
	struct exec_info *exec_info = new_exec_info(orig_line_ptr);

	if (prev_exec_info != NULL) {
		exec_info->prev_exec_info = prev_exec_info;
	}

	if (buffer != NULL) {
		set_buffer_cmd(exec_info->command, buffer);
	}
// ---------------------------------------------------------------

	while ((line = parse(line, exec_info))) {
		switch (status_for_next_cmd) {
		case DO_NOT_MATTER_TO_EXEC:
			status =
			    launch_job(src_file, exec_info, to_free_excess);
			break;
		case EXECUTE_IN_SUCCESS:
			if (status == 0) {
				status =
				    launch_job(src_file, exec_info,
					       to_free_excess);
			}
			break;
		case EXECUTE_IN_FAILURE:
			if (status != 0) {
				status =
				    launch_job(src_file, exec_info,
					       to_free_excess);
			} else {
				status = 0;
			}
			break;
		}
		status_for_next_cmd =
		    exec_info->command->next_status_needed_to_exec;
		sprintf(result, "%d", status);
		add_env_by_name("result", result);
		// Update cwd
		if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
			// TODO: error, load from home
		}
		add_env_by_name("PWD", cwd);
		if (has_to_exit) {
			break;
		}
		reset_exec_info(exec_info);
	}

	sprintf(result, "%d", status);
	add_env_by_name("result", result);

	free(exec_info->parse_info);
	free(exec_info->file_info);
	free(exec_info->sub_info);
	free_command(exec_info->command);
	memset(exec_info->line, 0, MAX_ARGUMENT_SIZE);
	free(exec_info);
	return status;
}

// --------- Substitutions ---------

struct sub_info *
new_sub_info()
{
	struct sub_info *sub_info =
	    (struct sub_info *)malloc(sizeof(struct sub_info));

	// Check if malloc failed
	if (sub_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(sub_info, 0, sizeof(struct sub_info));
	sub_info->old_ptr = NULL;
	sub_info->old_lexer = NULL;

	return sub_info;
}

void
restore_sub_info(struct sub_info *sub_info)
{
	//memset(sub_info->last_alias, 0, MAX_ARGUMENT_SIZE);
	memset(sub_info->buffer, 0, MAX_ENV_SIZE);
	sub_info->old_ptr = NULL;
	sub_info->old_lexer = NULL;
}

char *
substitute(const char *to_substitute)
{
	char *ret = malloc(sizeof(char) * MAX_ENV_SIZE);

	// Check if malloc failed
	if (ret == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(ret, 0, sizeof(char) * MAX_ENV_SIZE);

	if (strlen(to_substitute) == 1) {
		// Could be ? or $ or # or @
		if (*to_substitute == '$') {
			sprintf(ret, "%d", getpid());
			return ret;
		} else if (*to_substitute == '?') {
			char *result = getenv("result");

			sprintf(ret, "%s", result);
			return ret;
		}
	} else if (strlen(to_substitute) == 0) {
		*ret = '$';
		return ret;
	}

	if (getenv(to_substitute) == NULL) {
		fprintf(stderr, "error: var %s does not exist\n",
			to_substitute);
		free(ret);
		return NULL;
	} else {
		strcpy(ret, getenv(to_substitute));
	}
	return ret;
}

// ------------ Files --------------

struct file_info *
new_file_info()
{
	struct file_info *file_info =
	    (struct file_info *)malloc(sizeof(struct file_info));

	// Check if malloc failed
	if (file_info == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(file_info, 0, sizeof(struct file_info));
	file_info->mode = NO_FILE_READ;
	file_info->ptr = file_info->buffer;

	return file_info;
}

void
restore_file_info(struct file_info *file_info)
{
	file_info->mode = NO_FILE_READ;
	memset(file_info->buffer, 0, MAX_ARGUMENT_SIZE);
	file_info->ptr = file_info->buffer;
}

// New TOKENIZATION Recursive

char *
parse(char *ptr, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *cmd = exec_info->last_command;

	parse_info->finished = 0;

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
		ptr = exec_lexer(ptr, exec_info);

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
	parse_info->finished = 0;
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

	parse_info->has_arg_started = 1;

	if (strstr(line, "$(") == line) {
		line++;
		return start_subexec(line, exec_info);
	}

	sub_info->old_ptr = parse_info->copy;
	parse_info->copy = sub_info->buffer;

	sub_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &sub;

	return line;
}

int
copy_substitution(struct parse_info *parse_info, const char *sub_buffer)
{
	if (strlen(sub_buffer) <= 0) {
		//*parse_info->copy++ = '$';
		return 0;
	}
	char *subst = substitute(sub_buffer);

	if (subst != NULL) {
		char *ptr;

		for (ptr = subst; *ptr != '\0'; ptr++) {
			*parse_info->copy++ = *ptr;
		}
		ptr = NULL;
		free(subst);
		return 0;
	}
	return -1;
}

char *
end_sub(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct sub_info *sub_info = exec_info->sub_info;
	struct command *cmd = exec_info->last_command;

	parse_info->curr_lexer = sub_info->old_lexer;
	parse_info->copy = sub_info->old_ptr;

	if (copy_substitution(parse_info, sub_info->buffer) < 0) {
		line = NULL;
	}

	cmd->current_arg += strlen(cmd->current_arg);
	parse_info->copy = cmd->current_arg;
	return --line;
}

char *
start_subexec(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	int n_parenthesis = 1;

	// Read again and parse until )
	char *buffer = malloc(sizeof(char) * 1024);

	if (buffer == NULL)
		err(EXIT_FAILURE, "malloc failed");
	memset(buffer, 0, sizeof(char) * 1024);

	// Store all in line_buf
	char *line_buf = malloc(sizeof(char) * 1024);

	// Check if malloc failed
	if (line_buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(line_buf, 0, sizeof(char) * 1024);

	char *ptr;
	char *old_ptr = parse_info->copy;

	parse_info->copy = line_buf;

	if (strstr(line, "((") == line) {
		strcpy(line_buf, "math ");
		parse_info->copy += strlen(line_buf);
		line++;
		n_parenthesis++;
	}
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
	exec_info->parse_info->finished = 0;
	parse(buffer, exec_info);

	parse_info->copy = old_ptr;
	free(line_buf);
	free(buffer);
	return ptr++;
}

static char *
start_math(char *line, struct exec_info *exec_info)
{

//static const lexer m_a = { '\0', request_new_line };
//static const lexer m_b = { '"', start_dquote };
//static const lexer m_c = { '$', start_sub };
//static const lexer m_d = { '\'', start_squote };
//static const lexer m_e = { ')', end_math };
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

void
new_argument(struct exec_info *exec_info)
{
	struct command *cmd = exec_info->last_command;

	if (cmd->argc > 0) {
		// Do substitution and update command
		glob_t gstruct;

		if (glob(cmd->current_arg, GLOB_ERR, NULL, &gstruct) ==
		    GLOB_NOESCAPE) {
			fprintf(stderr, "Error: glob failed");
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
error_token(char token, char *line)
{
	fprintf(stderr,
		"dash: syntax error in '%c' near unexpected token `%s", token,
		line);
	return NULL;
}

char *
here_doc(char *line, struct exec_info *exec_info)
{
	struct command *cmd = exec_info->last_command;

	if (strcmp(cmd->current_arg, "HERE") == 0) {
		if (seek(++line)) {
			fprintf(stderr, "Mash: error text behind here doc\n");
			return NULL;
		}
		line--;
		// TODO: check input
		if (set_file_cmd(cmd, HERE_DOC_READ, "") >= 0) {
			reset_last_arg(cmd);
			cmd->argc--;
			exec_info->parse_info->copy = cmd->current_arg;

			return line;
		}
	}
	error_token('{', line);
	return NULL;
}

char *
start_file_in(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &file;

	parse_info->copy = exec_info->file_info->buffer;

	parse_info->has_arg_started = 0;
	exec_info->file_info->mode = INPUT_READ;

	return line;
}

char *
start_file_out(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct file_info *file_info = exec_info->file_info;
	struct command *cmd = exec_info->last_command;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->curr_lexer = &file;

	parse_info->copy = file_info->buffer;
	parse_info->has_arg_started = 0;

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

	spec_char(*tmp_lexer)[256] = parse_info->curr_lexer;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->old_lexer = tmp_lexer;

	if (!parse_info->has_arg_started) {
		return error_token(*line, line);
	}

	if (file_info->mode == INPUT_READ) {
		cmd = exec_info->command;
	} else {
		cmd = exec_info->last_command;
	}

	if (set_file_cmd(cmd, file_info->mode, file_info->buffer) < 0) {
		return NULL;
	}

	cmd->current_arg += strlen(cmd->current_arg);
	parse_info->copy = cmd->current_arg;

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
tilde_tokenize(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	// Check if next is ' ' '\n' or '/'then ok if not as '\'                       
	if (parse_info->has_arg_started) {
		line = copy(line, exec_info);

		return line;
	}

	line++;
	if (*line == '/' || *line == ' ' || *line == '\n') {
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

struct exec_info *
new_exec_info(char *line)
{
	struct exec_info *exec_info = malloc(sizeof(struct exec_info));

	if (exec_info == NULL) {
		err(EXECUTE_IN_FAILURE, "malloc failed");
	}
	memset(exec_info, 0, sizeof(struct exec_info));
	exec_info->command = new_command();
	exec_info->last_command = exec_info->command;
	exec_info->parse_info = new_parse_info();
	exec_info->sub_info = new_sub_info();
	exec_info->file_info = new_file_info();
	exec_info->line = line;
	exec_info->prev_exec_info = NULL;
	return exec_info;
}

void
reset_exec_info(struct exec_info *exec_info)
{
	reset_command(exec_info->command);
	restore_parse_info(exec_info->parse_info);
	restore_file_info(exec_info->file_info);
	restore_sub_info(exec_info->sub_info);
}

void
free_exec_info(struct exec_info *exec_info)
{
	free(exec_info->parse_info);
	free(exec_info->file_info);
	free(exec_info->sub_info);
	free(exec_info->line);

	if (exec_info->prev_exec_info != NULL) {
		free_command_with_buf(exec_info->prev_exec_info->command);
		free_exec_info(exec_info->prev_exec_info);
	}
	free(exec_info);
};

char *
pipe_tok(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct command *old_cmd = exec_info->last_command;

	// Check if next char is |
	if (strstr(line, "||") == line) {
		old_cmd->next_status_needed_to_exec = EXECUTE_IN_FAILURE;
		line++;
		return line;
	}

	if (parse_info->has_arg_started) {
		new_argument(exec_info);
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
		// Add an argument to old command
		if (exec_info->parse_info->has_arg_started) {
			new_argument(exec_info);
		}
		command->next_status_needed_to_exec = EXECUTE_IN_SUCCESS;
		parse_info->finished = 1;
		line++;
		return line;
	} else if (strstr(line, "&>") == line) {
		line = copy(line, exec_info);

		parse_info->has_arg_started = 1;
		return line;
	}

	if (seek(line)) {
		return error_token('&', line);
	}

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
	while (!exec_info->parse_info->request_line) {
		// FIX: create a buffer to hold it
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
	}

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
	line++;
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
copy_and_end(char *line, struct exec_info *exec_info)
{
	line = copy(line, exec_info);
	line = end_line(line, exec_info);

	return line;
}

char *
error(char *line, struct exec_info *exec_info)
{
	exec_info->parse_info->finished = 1;

	error_token(*line, line);
	return NULL;
}

char *
exec_lexer(char *line, struct exec_info *exec_info)
{
	int index = *line % ASCII_CHARS;

	line = (*exec_info->parse_info->curr_lexer)[index] (line, exec_info);
	return line;
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
