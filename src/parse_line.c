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
#include "builtin/export.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "open_files.h"
#include "parse_line.h"
#include "show_prompt.h"
#include "exec_cmd.h"

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
	parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
	parse_info->do_not_expect_new_cmd = 0;
	parse_info->copy = NULL;

	return parse_info;
}

void
restore_parse_info(struct parse_info *parse_info)
{
	parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
	parse_info->do_not_expect_new_cmd = 0;
	parse_info->copy = NULL;
};

// ---------- Commands -------------

struct command *
get_pipe(char *line)
{
	// Parsing information
	struct parse_info *parse_info = new_parse_info();

	// Commands 
	struct command *command = new_command();

	// Substitution variables
	struct sub_info *sub_info = new_sub_info();

	// File variables
	struct file_info *file_info = new_file_info();

// ---------------------------------------------------------------
	line = cmd_tokenize(line, parse_info, command, file_info, sub_info);
	if (line == NULL) {
		free(parse_info);
		free(file_info);
		free(sub_info);
		return NULL;
	}
	free(parse_info);
	free(file_info);
	free(sub_info);
	return command;
}

int
find_command(char *line, char *buffer, FILE * src_file)
{
	int status = 0;
	int status_for_next_cmd = DO_NOT_MATTER_TO_EXEC;
	pid_t pid;
	char cwd[MAX_ENV_SIZE];
	char result[4];
	struct command *command = new_command();

	if (buffer != NULL) {
		set_buffer_cmd(command, buffer);
	}
	// Parsing information
	struct parse_info *parse_info = new_parse_info();
	struct sub_info *sub_info = new_sub_info();
	struct file_info *file_info = new_file_info();

// ---------------------------------------------------------------

	while ((line =
		cmd_tokenize(line, parse_info, command, file_info, sub_info))) {
		switch (status_for_next_cmd) {
		case DO_NOT_MATTER_TO_EXEC:
			status = exec_command(command, src_file);
			break;
		case EXECUTE_IN_SUCCESS:
			if (status == 0) {
				status = exec_command(command, src_file);
			}
			break;
		case EXECUTE_IN_FAILURE:
			if (status != 0) {
				status = exec_command(command, src_file);
			} else {
				status = 0;
			}
			break;
		}
		status_for_next_cmd = command->next_status_needed_to_exec;
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
		reset_command(command);
		restore_parse_info(parse_info);
		restore_file_info(file_info);
		restore_sub_info(sub_info);
	}

	sprintf(result, "%d", status);
	add_env_by_name("result", result);

	free(parse_info);
	free(file_info);
	free(sub_info);
	free_command(command);
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
	sub_info->ptr = sub_info->buffer;

	return sub_info;
}

void
restore_sub_info(struct sub_info *sub_info)
{
	memset(sub_info->last_alias, 0, MAX_ARGUMENT_SIZE);
	memset(sub_info->buffer, 0, MAX_ENV_SIZE);
	sub_info->ptr = sub_info->buffer;
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
cmd_tokenize(char *ptr, struct parse_info *parse_info,
	     struct command *command, struct file_info *file_info,
	     struct sub_info *sub_info)
{
	// IF first char is \0 exit immediately
	if (ptr == NULL)
		return NULL;
	if (*ptr == '\0' || *ptr == '#' || *ptr == '\n')
		return NULL;

	// Can call the other
	struct command *new_cmd = command;
	struct command *old_cmd = new_cmd;

	new_cmd->current_arg += strlen(new_cmd->current_arg);

	parse_info->copy = new_cmd->current_arg;

	for (; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, new_cmd,
						file_info, sub_info);
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '\'':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr = hard_apost_tokenize(++ptr, parse_info);
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '|':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			// Check if next char is |
			if (*++ptr == '|') {
				new_cmd->next_status_needed_to_exec =
				    EXECUTE_IN_FAILURE;
				return ++ptr;
			}
			ptr--;
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				// Add an argument
				new_argument(new_cmd, parse_info, file_info,
					     sub_info);
			}
			// New command
			old_cmd = new_cmd;
			new_cmd = new_command();
			// Update old_cmd pipe
			strcpy(sub_info->last_alias, "");
			pipe_command(old_cmd, new_cmd);
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '<':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr =
			    cmdtok_redirect_in(++ptr, parse_info, command,
					       file_info, sub_info);
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '>':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr =
			    cmdtok_redirect_out(++ptr, parse_info, new_cmd,
						file_info, sub_info);
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '*':
		case '?':
		case '[':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr = glob_tokenize(ptr, parse_info, new_cmd,
					    file_info, sub_info);
			break;
		case '\\':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			// Do NOT return if escape \n
			if (*++ptr == '\n') {
				request_new_line(ptr);
				ptr--;
			} else {
				*parse_info->copy++ = *++ptr;
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '$':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr =
			    substitution_tokenize(++ptr, parse_info, new_cmd,
						  file_info, sub_info);
			// Copy now sub_info to parse_info->copy
			if (strlen(sub_info->buffer) > 0) {
				if (copy_substitution
				    (parse_info, sub_info->buffer) < 0) {
					return NULL;
				}
			} else {
				ptr++;
			}
			new_cmd->current_arg += strlen(new_cmd->current_arg);
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '~':
			// Csheck if next is ' ' '\n' or '/'then ok if not as '\'                       
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				*parse_info->copy++ = *ptr;
			} else {
				ptr =
				    tilde_tokenize(ptr, parse_info, new_cmd,
						   file_info, sub_info);
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case ' ':
		case '\t':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				new_argument(new_cmd, parse_info,
					     file_info, sub_info);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
			}
			break;
		case '\n':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				new_argument(new_cmd, parse_info,
					     file_info, sub_info);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
				return ++ptr;
			}
			break;
		case ';':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				new_argument(new_cmd, parse_info, file_info,
					     sub_info);
			}
			return ++ptr;
			break;
		case '#':
			// END
			return ptr;
			break;
		case '&':
			// Check if next char is & or >
			if (*++ptr == '&') {
				// Add an argument to old command
				if (parse_info->has_arg_started ==
				    PARSE_ARG_STARTED) {
					new_argument(new_cmd, parse_info,
						     file_info, sub_info);
				}
				new_cmd->next_status_needed_to_exec =
				    EXECUTE_IN_SUCCESS;
				return ++ptr;
			} else if (*ptr == '>') {
				*parse_info->copy++ = *--ptr;
				parse_info->has_arg_started = PARSE_ARG_STARTED;
				break;
			} else {
				parse_info->do_not_expect_new_cmd = 1;
				//do_not_wait_commands(cmd_array);
				ptr--;
			}
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '(':
		case ')':
			return error_token(*ptr, ptr);
			break;
		default:
			// FIX: add error struct to handle differen error tokens
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			if (check_here_doc(ptr, new_cmd)) {
				parse_info->do_not_expect_new_cmd = 1;	// End parsing
				ptr += 4;
				new_cmd->argc--;
			} else {
				*parse_info->copy++ = *ptr;
			}

			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		}
		if (ptr == NULL)
			return ptr;
	}
	// End of substitution
	if (new_cmd->current_arg[0] != '\0') {
		new_argument(new_cmd, parse_info, file_info, sub_info);
	} else {
		reset_last_arg(new_cmd);
	}
	return ptr;
}

char *
hard_apost_tokenize(char *line, struct parse_info *parse_info)
{
	// Can't call any other function
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		if (*ptr == '\'') {
			return ptr;
		} else if (*ptr == '\n') {
			//*parse_info->copy++ = *ptr;
			request_new_line(line);
			ptr = line;
			ptr--;
		} else {
			*parse_info->copy++ = *ptr;
		}
	}
	// Ask for new line
	return --ptr;
}

char *
soft_apost_tokenize(char *line, struct parse_info *parse_info,
		    struct command *command, struct file_info *file_info,
		    struct sub_info *sub_info)
{
	// Can call only substitution
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			return ptr;
			break;
		case '\\':
			// Do NOT return if escape \n
			if (*++ptr == '\n') {
				request_new_line(line);
				ptr = line;
				ptr--;
			}
			if (*ptr == '$' || *ptr == '"') {
				*parse_info->copy++ = *ptr;
			} else {
				*parse_info->copy++ = *--ptr;
				*parse_info->copy++ = *++ptr;
			}
			break;
		case '$':
			// If the next one is \ do nothing
			if (*++ptr != '\\') {
				ptr =
				    substitution_tokenize(ptr, parse_info,
							  command, file_info,
							  sub_info);
				// Copy now sub_info to parse_info->copy
				if (strlen(sub_info->buffer) > 0) {
					if (copy_substitution
					    (parse_info,
					     sub_info->buffer) < 0) {
						return NULL;
					}
				} else {
					ptr++;
				}
			} else {
				*parse_info->copy++ = *--ptr;
				ptr++;
			}
			break;
		case '\n':
			*parse_info->copy++ = *ptr;
			request_new_line(line);
			ptr = line;
			ptr--;
			break;
		default:
			*parse_info->copy++ = *ptr;
			break;
		}
	}
	// Ask for new line
	return --ptr;
}

char *
substitution_tokenize(char *line, struct parse_info *parse_info,
		      struct command *command, struct file_info *file_info,
		      struct sub_info *sub_info)
{
	sub_info->ptr = sub_info->buffer;
	// Can't call any other function
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		*sub_info->ptr++ = *ptr;
		switch (*ptr) {
		case '"':
		case '\'':
		case '~':
		case ' ':
		case '\t':
		case ';':
		case '<':
		case '>':
		case '*':
		case '\n':
			// Copy the $
			*--sub_info->ptr = '\0';
			return --ptr;
			break;
		case '\\':
			// Do NOT return if escape \n
			if (*++ptr == '\n') {
				*--sub_info->ptr = '\0';
				request_new_line(line);
				ptr = line;
				ptr--;
			}
			break;
		case '(':
			*--sub_info->ptr = '\0';
			ptr =
			    execute_token(++ptr, parse_info, command,
					  file_info, sub_info);
			ptr--;
			break;
		case ')':
		case '{':
		case '}':
			return error_token(*ptr, ptr);
			break;
		case '$':
		case '?':
		case '@':
		case '-':
		case '_':
			// Copy and exit
			return ptr;
			break;
		}
	}
	// Ask for more
	return --ptr;
}

int
copy_substitution(struct parse_info *parse_info, const char *sub_buffer)
{
	if (strlen(sub_buffer) <= 0) {
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
file_tokenize(char *line, struct parse_info *parse_info,
	      struct command *command, struct file_info *file_info,
	      struct sub_info *sub_info)
{
	// Check if next char is \0
	parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, command,
						file_info, sub_info);
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '\'':
			ptr = hard_apost_tokenize(++ptr, parse_info);
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '|':
			break;
		case '~':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				*parse_info->copy++ = *ptr;
			} else {
				ptr =
				    tilde_tokenize(ptr, parse_info, command,
						   file_info, sub_info);
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '<':
		case '>':
			return --ptr;
			break;
		case '\\':
			*parse_info->copy++ = *++ptr;
			break;
		case '$':
			ptr =
			    substitution_tokenize(++ptr, parse_info, command,
						  file_info, sub_info);
			if (strlen(sub_info->buffer) > 0) {
				if (copy_substitution
				    (parse_info, sub_info->buffer) < 0) {
					return NULL;
				}
			} else {
				ptr++;
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case ' ':
		case '\t':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				*parse_info->copy++ = '\0';
				return --ptr;
			}
			break;
		case '\n':
			*parse_info->copy++ = '\0';
			return ptr;
			break;
		case '(':
		case ')':
		case '{':
		case '}':
			return error_token(*ptr, ptr);
			break;
		default:
			*parse_info->copy++ = *ptr;
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		}
		if (ptr == NULL)
			return ptr;
	}
	// Ask for more
	return --ptr;
}

char *
glob_tokenize(char *line, struct parse_info *parse_info,
	      struct command *command, struct file_info *file_info,
	      struct sub_info *sub_info)
{
	int exit = 0;

	// Store all in line_buf
	char *line_buf = malloc(1024);

	if (line_buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(line_buf, 0, 1024);
	// Can call only substitution
	char *old_ptr = parse_info->copy;

	parse_info->copy = line_buf;
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, command,
						file_info, sub_info);
			break;
		case '\'':
			ptr = hard_apost_tokenize(++ptr, parse_info);
			break;
		case '\\':
			// Do NOT return if escape \n
			if (*++ptr == '\n') {
				request_new_line(line);
				ptr = line;
				ptr--;
			}
			// Do return if escape ' '
			if (*ptr == '$' || *ptr == '"') {
				*parse_info->copy++ = *ptr;
			} else {
				*parse_info->copy++ = *--ptr;
				*parse_info->copy++ = *++ptr;
			}
			break;
		case '$':
			ptr =
			    substitution_tokenize(++ptr, parse_info, command,
						  file_info, sub_info);
			// Copy now sub_info to parse_info->copy
			if (strlen(sub_info->buffer) > 0) {
				if (copy_substitution
				    (parse_info, sub_info->buffer) < 0) {
					return NULL;
				}
			} else {
				ptr++;
			}
			break;
		case '\n':
			// End line
			*parse_info->copy = '\0';
			exit = 1;
			ptr++;
			break;
		case ' ':
			// End line
			*parse_info->copy = '\0';
			exit = 1;
			break;
		default:
			*parse_info->copy++ = *ptr;
			break;
		}
		if (exit)
			break;
	}
	// Append the things in command
	strcat(command->current_arg, line_buf);

	// Do substitution and update command
	glob_t gstruct;

	if (glob(command->current_arg,
		 GLOB_ERR, NULL, &gstruct) == GLOB_NOESCAPE) {
		//TODO: add error
		return NULL;
	}

	char **found;

	found = gstruct.gl_pathv;
	if (found == NULL) {
		add_arg(command);
	} else {
		while (*found != NULL) {
			strcpy(command->current_arg, *found);
			add_arg(command);
			found++;
		}
	}

	free(line_buf);
	globfree(&gstruct);
	// Ask for new line
	parse_info->copy = old_ptr;
	return --ptr;
}

char *
execute_token(char *line, struct parse_info *parse_info,
	      struct command *command, struct file_info *file_info,
	      struct sub_info *sub_info)
{
	int n_parenthesis = 0;

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

	char *line_buf_ptr = line_buf;
	char *ptr;

	for (ptr = line; ptr != NULL; ptr++) {
		switch (*ptr) {
		case '\\':
			*line_buf_ptr++ = *++ptr;
			break;
		case '\0':
		case '\n':
			*line_buf_ptr = '\0';
			request_new_line(line);
			ptr = line;
			ptr--;
			break;
		case '(':
			n_parenthesis++;
			*line_buf_ptr++ = *ptr;
			break;
		case ')':
			n_parenthesis--;
			*line_buf_ptr++ = *ptr;
			break;
		default:
			*line_buf_ptr++ = *ptr;
			break;
		}
		if (n_parenthesis < 0) {
			*--line_buf_ptr = '\0';
			*ptr = '\0';
			break;
		}
	}
	// Copy result into sub_info->buffer
	// FIX: fix find_command call
	find_command(line_buf, buffer, stdin);
	// Remove \n from buffer to ' '
	char *current_pos = strchr(buffer, '\n');

	while (current_pos) {
		*current_pos = ' ';
		current_pos = strchr(current_pos, '\n');
	}
	cmd_tokenize(buffer, parse_info, command, file_info, sub_info);
	free(line_buf);
	free(buffer);
	return ptr;
}

void
request_new_line(char *line)
{
	prompt_request();
	fgets(line, 1024, stdin);
	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}
}

void
new_argument(struct command *current_cmd, struct parse_info *parse_info,
	     struct file_info *file_info, struct sub_info *sub_info)
{
	if (strcmp(sub_info->last_alias, current_cmd->argv[0]) != 0) {
		if (check_alias_cmd(current_cmd)) {
			strcpy(sub_info->last_alias, current_cmd->argv[0]);
			reset_last_arg(current_cmd);
			current_cmd->argc = 0;
			current_cmd->current_arg = current_cmd->argv[0];
			cmd_tokenize(get_alias
				     (sub_info->last_alias),
				     parse_info,
				     current_cmd, file_info, sub_info);
			return;
		}
	}
	add_arg(current_cmd);
}

char *
error_token(char token, char *line)
{
	fprintf(stderr,
		"dash: syntax error in '%c' near unexpected token `%s", token,
		line);
	return NULL;
}

int
check_here_doc(char *line, struct command *command)
{
	char *ptr = line;

	if (*ptr++ == 'H' && *ptr++ == 'E' && *ptr++ == 'R' && *ptr++ == 'E'
	    && *ptr == '{') {
		// TODO: check input
		if (set_file_cmd(command, HERE_DOC_READ, "") >= 0) {
			return 1;
		}
	}
	return 0;
}

char *
cmdtok_redirect_in(char *line, struct parse_info *parse_info,
		   struct command *command, struct file_info *file_info,
		   struct sub_info *sub_info)
{
	parse_info->copy = file_info->buffer;
	line = file_tokenize(line, parse_info, command, file_info, sub_info);
	// TODO: first command not new cmd
	if (set_file_cmd(command, INPUT_READ, file_info->buffer) < 0) {
		return NULL;
	}
	return line;
}

char *
cmdtok_redirect_out(char *line, struct parse_info *parse_info,
		    struct command *command, struct file_info *file_info,
		    struct sub_info *sub_info)
{
	parse_info->copy = file_info->buffer;
	line = file_tokenize(line, parse_info, command, file_info, sub_info);

	if (parse_info->has_arg_started) {
		if (strcmp(command->current_arg, "2") == 0) {
			reset_last_arg(command);
			if (set_file_cmd
			    (command, ERROR_WRITE, file_info->buffer) < 0) {
				return NULL;
			}
			return line;
		} else if (strcmp(command->current_arg, "&") == 0) {
			reset_last_arg(command);
			if (set_file_cmd
			    (command, OUTPUT_WRITE, file_info->buffer) < 0) {
				return NULL;
			}
			if (set_file_cmd
			    (command,
			     ERROR_AND_OUTPUT_WRITE, file_info->buffer) < 0) {
				return NULL;
			}
			return line;
		} else if (strcmp(command->current_arg, "1") == 0) {
			reset_last_arg(command);
		}
	}

	if (set_file_cmd(command, OUTPUT_WRITE, file_info->buffer) < 0) {
		return NULL;
	}
	return line;
}

char *
tilde_tokenize(char *line, struct parse_info *parse_info,
	       struct command *command, struct file_info *file_info,
	       struct sub_info *sub_info)
{
	line++;
	if (*line == '/' || *line == ' ' || *line == '\n') {
		line--;
		substitution_tokenize("HOME",
				      parse_info, command, file_info, sub_info);
		// Copy now sub_info to parse_info->copy
		if (strlen(sub_info->buffer) > 0) {
			if (copy_substitution(parse_info, sub_info->buffer) < 0) {
				return NULL;
			}
		} else {
			line++;
		}
	} else {
		*parse_info->copy++ = *--line;
		*parse_info->copy++ = *++line;
	}
	return line;
}
