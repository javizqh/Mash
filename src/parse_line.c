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

// DECLARE STATIC FUNCTION
static char *cmd_tokenize(char *line, struct exec_info *exec_info);

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
static char *end_pipe(char *line, struct exec_info *exec_info);
static char *copy(char *line, struct exec_info *exec_info);
static char *copy_and_end(char *line, struct exec_info *exec_info);
static char *error(char *line, struct exec_info *exec_info);
static char *tilde_tokenize(char *line, struct exec_info *exec_info);	// TODO: do not use 
static char *start_subexec(char *line, struct exec_info *exec_info);
static char *start_math(char *line, struct exec_info *exec_info);
static char *end_math(char *line, struct exec_info *exec_info);
static char *request_new_line(char *line, struct exec_info *exec_info);
static char *here_doc(char *line, struct exec_info *exec_info);
static char *exec_lexer(char *line, struct exec_info *exec_info);
static int copy_substitution(struct parse_info *parse_info,
			     const char *sub_buffer);
static void new_argument(struct exec_info *exec_info);
static char *error_token(char token, char *line);
static int seek(char *line);

static const lexer sb_a = { '\0', end_sub };
static const lexer sb_b = { '\t', end_sub };
static const lexer sb_c = { '\n', end_sub };
static const lexer sb_d = { ' ', end_sub };
static const lexer sb_e = { '"', end_sub };
static const lexer sb_f = { '#', copy_and_end };
static const lexer sb_g = { '$', copy_and_end };
static const lexer sb_h = { '&', end_sub };
static const lexer sb_i = { '\'', end_sub };
static const lexer sb_j = { '(', start_subexec };
static const lexer sb_k = { ')', end_sub };
static const lexer sb_l = { '-', copy_and_end };
static const lexer sb_m = { ';', end_sub };
static const lexer sb_n = { '<', end_sub };
static const lexer sb_o = { '>', end_sub };
static const lexer sb_p = { '?', copy_and_end };
static const lexer sb_q = { '@', copy_and_end };
static const lexer sb_r = { '\\', end_sub };
static const lexer sb_s = { '_', copy_and_end };
static const lexer sb_t = { '{', end_sub };
static const lexer sb_u = { '}', end_sub };
static const lexer sb_v = { '|', end_sub };
static const lexer sb_w = { '~', end_sub };

static const lexer h_a = { '\0', request_new_line };
static const lexer h_b = { '\'', end_squote };

static const lexer s_b = { '"', end_dquote };
static const lexer s_d = { '\\', esp_escape };

static const lexer m_a = { '\0', request_new_line };
static const lexer m_b = { '"', start_dquote };
static const lexer m_c = { '$', start_sub };
static const lexer m_d = { '\'', start_squote };
static const lexer m_e = { ')', end_math };

static const lexer n_a = { '\0', end_line };
static const lexer n_b = { '\t', blank };
static const lexer n_c = { '\n', blank };
static const lexer n_d = { ' ', blank };
static const lexer n_e = { '"', start_dquote };
static const lexer n_f = { '#', end_line };
static const lexer n_g = { '$', start_sub };
static const lexer n_h = { '&', background };
static const lexer n_i = { '\'', start_squote };
static const lexer n_j = { '(', error };
static const lexer n_k = { ')', error };
static const lexer n_l = { ';', end_pipe };
static const lexer n_m = { '<', start_file_in };
static const lexer n_n = { '>', start_file_out };
static const lexer n_o = { '\\', escape };
static const lexer n_p = { '{', here_doc };
static const lexer n_q = { '}', error };
static const lexer n_r = { '|', pipe_tok };
static const lexer n_s = { '~', tilde_tokenize };

static const lexer f_a = { '\0', end_file };
static const lexer f_b = { '\t', end_file_started };
static const lexer f_c = { '\n', end_file };
static const lexer f_d = { ' ', end_file_started };
static const lexer f_e = { '"', start_dquote };
static const lexer f_f = { '#', end_file };
static const lexer f_g = { '$', start_sub };
static const lexer f_h = { '&', end_file };
static const lexer f_i = { '\'', start_squote };
static const lexer f_j = { '(', error };
static const lexer f_k = { ')', error };
static const lexer f_l = { '*', error };
static const lexer f_m = { ';', end_file };
static const lexer f_n = { '<', end_file };
static const lexer f_o = { '>', end_file };
static const lexer f_p = { '?', error };
static const lexer f_q = { '[', error };
static const lexer f_r = { '\\', escape };
static const lexer f_s = { '{', error };
static const lexer f_t = { '}', error };
static const lexer f_u = { '|', end_file };
static const lexer f_v = { '~', tilde_tokenize };

static const lexer normal[22] =
    { n_a, n_b, n_c, n_d, n_e, n_f, n_g, n_h, n_i, n_j, n_k, n_l, n_m, n_n, n_o,
	n_p, n_q, n_r, n_s
};
static const lexer hard[2] = { h_a, h_b };
static const lexer soft[4] = { h_a, s_b, n_g, s_d };
static const lexer math[5] = { m_a, m_b, m_c, m_d, m_e };

static const lexer sub[23] =
    { sb_a, sb_b, sb_c, sb_d, sb_e, sb_f, sb_g, sb_h, sb_i, sb_j, sb_k, sb_l,
	sb_m, sb_n, sb_o,
	sb_p, sb_q, sb_r, sb_s, sb_t, sb_u, sb_v, sb_w
};

static const lexer file[22] =
    { f_a, f_b, f_c, f_d, f_e, f_f, f_g, f_h, f_i, f_j, f_k, f_l, f_m, f_n, f_o,
	f_p, f_q, f_r, f_s, f_t, f_u, f_v
};

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
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer_size = sizeof(normal) / sizeof(normal[0]);
	parse_info->curr_lexer = &normal;
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->old_lexer = parse_info->curr_lexer;

	return parse_info;
}

void
restore_parse_info(struct parse_info *parse_info)
{
	parse_info->has_arg_started = 0;
	parse_info->finished = 0;
	parse_info->copy = NULL;
	parse_info->curr_lexer_size = sizeof(normal) / sizeof(normal[0]);
	parse_info->curr_lexer = &normal;
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
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

	while ((line = cmd_tokenize(line, exec_info))) {
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
	sub_info->old_lexer_size = 0;
	sub_info->old_lexer = NULL;

	return sub_info;
}

void
restore_sub_info(struct sub_info *sub_info)
{
	memset(sub_info->last_alias, 0, MAX_ARGUMENT_SIZE);
	memset(sub_info->buffer, 0, MAX_ENV_SIZE);
	sub_info->old_ptr = NULL;
	sub_info->old_lexer_size = 0;
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
cmd_tokenize(char *ptr, struct exec_info *exec_info)
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
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->curr_lexer = &hard;
	parse_info->curr_lexer_size = sizeof(hard) / sizeof(hard[0]);

	parse_info->has_arg_started = 1;
	return line;
}

char *
end_squote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	const lexer(*tmp_lexer)[] = parse_info->curr_lexer;
	int tmp_lexer_size = parse_info->curr_lexer_size;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->curr_lexer_size = parse_info->old_lexer_size;
	parse_info->old_lexer = tmp_lexer;
	parse_info->old_lexer_size = tmp_lexer_size;
	parse_info->finished = 0;
	return line;
}

char *
start_dquote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;

	parse_info->old_lexer = parse_info->curr_lexer;
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->curr_lexer = &soft;
	parse_info->curr_lexer_size = sizeof(soft) / sizeof(soft[0]);

	parse_info->has_arg_started = 1;

	return line;
}

char *
end_dquote(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	const lexer(*tmp_lexer)[] = parse_info->curr_lexer;
	int tmp_lexer_size = parse_info->curr_lexer_size;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->curr_lexer_size = parse_info->old_lexer_size;
	parse_info->old_lexer = tmp_lexer;
	parse_info->old_lexer_size = tmp_lexer_size;
	return line;
}

char *
start_sub(char *line, struct exec_info *exec_info)
{
	struct parse_info *parse_info = exec_info->parse_info;
	struct sub_info *sub_info = exec_info->sub_info;

	sub_info->old_ptr = parse_info->copy;

	sub_info->old_lexer = parse_info->curr_lexer;
	sub_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->curr_lexer = &sub;
	parse_info->curr_lexer_size = sizeof(sub) / sizeof(sub[0]);

	parse_info->has_arg_started = 1;
	parse_info->copy = sub_info->buffer;

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
	parse_info->curr_lexer_size = sub_info->old_lexer_size;
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

	if (strstr(line, "((") == line) {
		//TODO: add math
		n_parenthesis++;
		line++;
	}
	parse_info->copy = line_buf;

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
	cmd_tokenize(buffer, exec_info);
	parse_info->copy = old_ptr;
	free(line_buf);
	free(buffer);
	return ptr;
}

static char *
start_math(char *line, struct exec_info *exec_info)
{
	// Has $, ", '
}

static char *
end_math(char *line, struct exec_info *exec_info)
{

}

char *
request_new_line(char *line, struct exec_info *exec_info)
{
	prompt_request();
	fgets(line, 1024, stdin);
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
			cmd_tokenize(get_alias(exec_info->sub_info->last_alias),
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
			// FIX: proper error
			return error_token('{', line);
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
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->curr_lexer = &file;
	parse_info->curr_lexer_size = sizeof(file) / sizeof(file[0]);

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
	parse_info->old_lexer_size = parse_info->curr_lexer_size;
	parse_info->curr_lexer = &file;
	parse_info->curr_lexer_size = sizeof(file) / sizeof(file[0]);

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
	const lexer(*tmp_lexer)[] = parse_info->curr_lexer;
	int tmp_lexer_size = parse_info->curr_lexer_size;

	parse_info->curr_lexer = parse_info->old_lexer;
	parse_info->curr_lexer_size = parse_info->old_lexer_size;
	parse_info->old_lexer = tmp_lexer;
	parse_info->old_lexer_size = tmp_lexer_size;

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
	// TODO: check for char, then error
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
		parse_info->has_arg_started = 1;
	}
	return line;
}

char *
end_line(char *line, struct exec_info *exec_info)
{
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
	int index;
	int size = exec_info->parse_info->curr_lexer_size;
	const lexer(*table)[] = exec_info->parse_info->curr_lexer;

	for (index = 0; index < (size + 1); index++) {
		if (index == size) {
			line = copy(line, exec_info);
		} else {
			if (*line == (*table)[index].token) {
				line = (*table)[index].fun(line, exec_info);
				break;
			}
		}
	}
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
