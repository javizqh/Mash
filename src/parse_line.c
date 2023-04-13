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
#include "cmd_array.h"
#include "builtin/exit.h"
#include "open_files.h"
#include "parse_line.h"
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

// ---------- Commands -------------

struct cmd_array *
get_commands(char *line)
{
	// Parsing information
	struct parse_info *parse_info = new_parse_info();

	// Commands 
	struct cmd_array *commands = new_commands();

	// Substitution variables
	struct sub_info *sub_info = new_sub_info();

	// File variables
	struct file_info *file_info = new_file_info();

// ---------------------------------------------------------------
	if (cmd_tokenize(line, parse_info, commands, file_info, sub_info) < 0) {
		commands->status = -1;
	}
	free(parse_info);
	free(file_info);
	free(sub_info);
	return commands;
}

int
find_command(char *line, char *buffer, FILE * src_file)
{
	int i;
	int status = 0;
	char cwd[MAX_ENV_SIZE];
	struct cmd_array *commands = get_commands(line);

	if (commands->status < 0) {
		free_cmd_array(commands);
		return 1;
	}

	if (buffer != NULL) {
		set_buffer_cmd(commands->commands[commands->n_cmd - 1], buffer);
	}
	// Alias -> builtin -> PWD -> PATH -> exec
	// If alias check if exists command
	for (i = 0; i < commands->n_cmd; i++) {
		// Check if the cmd array is empty
		if (*commands->commands[i]->argv[0] == '\0') {
			free_cmd_array(commands);
			return 1;
		}
		switch (commands->commands[i]->prev_status_needed_to_exec) {
		case DO_NOT_MATTER_TO_EXEC:
			status = exec_command(commands->commands[i], src_file);
			break;
		case EXECUTE_IN_SUCCESS:
			if (status == 0) {
				status = exec_command(commands->commands[i],
						      src_file);
			}
			break;
		case EXECUTE_IN_FAILURE:
			if (status != 0) {
				status = exec_command(commands->commands[i],
						      src_file);
			} else {
				status = 0;
			}
			break;
		}
		// Update cwd
		if (getcwd(cwd, MAX_ENV_SIZE) == NULL) {
			// TODO: error, load from home
		}
		add_env_by_name("PWD", cwd);

		if (has_to_exit) {
			break;
		}
	}
	free_cmd_array(commands);
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

// New TOKENIZATION Recursive

int
cmd_tokenize(char *line, struct parse_info *parse_info,
	     struct cmd_array *cmd_array, struct file_info *file_info,
	     struct sub_info *sub_info)
{
	// Can call the other
	struct command *old_cmd;
	struct command *new_cmd;

	if (cmd_array->n_cmd == 0) {
		// Create command
		new_cmd = new_command();
		if (add_command(new_cmd, cmd_array) < 0) {
			fprintf(stderr, "Failed to add command");
		}
	} else {
		if (cmd_array->commands[cmd_array->n_cmd - 1]->pipe_next != NULL
		    && get_last_command(cmd_array->commands[cmd_array->n_cmd -
							    1])->argc < 0) {
			new_argument(get_last_command
				     (cmd_array->commands
				      [cmd_array->n_cmd - 1]), parse_info,
				     cmd_array, file_info, sub_info);
		} else {
			new_cmd =
			    get_last_command(cmd_array->commands
					     [cmd_array->n_cmd - 1]);
			if (*new_cmd->current_arg != '\0') {
				new_argument(new_cmd, parse_info, cmd_array,
					     file_info, sub_info);
			}
		}
	}
	parse_info->copy = new_cmd->current_arg;

	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, cmd_array,
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
			// Add an argument to old command
			old_cmd = new_cmd;
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				add_arg(old_cmd);
			}
			// New command
			new_cmd = new_command();
			// Check if next char is |
			if (*++ptr == '|') {
				if (add_command(new_cmd, cmd_array) < 0) {
					fprintf(stderr,
						"Failed to add command");
				}
				new_cmd->prev_status_needed_to_exec =
				    EXECUTE_IN_FAILURE;
			} else {
				// Update old_cmd pipe
				strcpy(sub_info->last_alias, "");
				pipe_command(old_cmd, new_cmd);
				ptr--;
			}
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '<':
			parse_info->copy = file_info->buffer;
			ptr =
			    file_tokenize(++ptr, parse_info, cmd_array,
					  file_info, sub_info);
			if (set_file_cmd
			    (cmd_array->commands[cmd_array->n_cmd - 1],
			     INPUT_READ, file_info->buffer) < 0) {
				return -1;
			}
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '>':
			parse_info->copy = file_info->buffer;
			ptr =
			    file_tokenize(++ptr, parse_info, cmd_array,
					  file_info, sub_info);
			if (set_file_cmd
			    (cmd_array->commands[cmd_array->n_cmd - 1],
			     OUTPUT_WRITE, file_info->buffer) < 0) {
				return -1;
			}

			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '*':
		case '?':
		case '[':
			ptr = glob_tokenize(ptr, parse_info, cmd_array,
					    file_info, sub_info);
			break;
		case '\\':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			// Do NOT return if escape \n
			if (*++ptr == '\n') {
				request_new_line(line);
				ptr = line;
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
			    substitution_tokenize(++ptr, parse_info, cmd_array,
						  file_info, sub_info);
			// Copy now sub_info to parse_info->copy
			if (strlen(sub_info->buffer) > 0) {
				if (copy_substitution
				    (parse_info, sub_info->buffer) < 0) {
					return -1;
				}
			} else {
				ptr++;
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case '~':
			// Csheck if next is ' ' '\n' or '/'then ok if not as '\'                       
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				*parse_info->copy++ = *ptr;
			} else {
				ptr++;
				if (*ptr == '/' || *ptr == ' ' || *ptr == '\n') {
					ptr--;
					substitution_tokenize("HOME",
							      parse_info,
							      cmd_array,
							      file_info,
							      sub_info);
					// Copy now sub_info to parse_info->copy
					if (strlen(sub_info->buffer) > 0) {
						if (copy_substitution
						    (parse_info,
						     sub_info->buffer) < 0) {
							return -1;
						}
					} else {
						ptr++;
					}
				} else {
					ptr--;
					*parse_info->copy++ = *ptr;
					*parse_info->copy++ = *++ptr;
				}
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case ' ':
		case '\t':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				new_argument(new_cmd, parse_info, cmd_array,
					     file_info, sub_info);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
			}
			break;
		case '\n':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				new_argument(new_cmd, parse_info, cmd_array,
					     file_info, sub_info);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
			}
			break;
		case ';':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			new_argument(new_cmd, parse_info, cmd_array,
				     file_info, sub_info);
			new_cmd = new_command();
			if (add_command(new_cmd, cmd_array) < 0) {
				fprintf(stderr, "Failed to add command");
			}
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '&':
			// Check if next char is & or >
			if (*++ptr == '&') {
				// Add an argument to old command
				old_cmd = new_cmd;
				if (parse_info->has_arg_started ==
				    PARSE_ARG_STARTED) {
					new_argument(new_cmd, parse_info,
						     cmd_array, file_info,
						     sub_info);
				}
				// New command
				new_cmd = new_command();
				if (add_command(new_cmd, cmd_array) < 0) {
					fprintf(stderr,
						"Failed to add command");
				}
				new_cmd->prev_status_needed_to_exec =
				    EXECUTE_IN_SUCCESS;
				parse_info->copy = new_cmd->current_arg;
			} else if (*ptr == '>') {
				ptr--;
				// Update command
				if (set_file_cmd
				    (cmd_array->commands[cmd_array->n_cmd - 1],
				     ERROR_WRITE, file_info->buffer) < 0) {
					return -1;
				}
			} else {
				parse_info->do_not_expect_new_cmd = 1;
				do_not_wait_commands(cmd_array);
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
			if (check_here_doc(ptr, cmd_array)) {
				parse_info->do_not_expect_new_cmd = 1;	// End parsing
				ptr++;
				ptr++;
				ptr++;
				ptr++;
				new_cmd->argc--;
			} else {
				*parse_info->copy++ = *ptr;
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		}
		if (ptr == NULL)
			return -1;
	}
	// End of substitution
	if (new_cmd->current_arg[0] != '\0') {
		new_argument(new_cmd, parse_info, cmd_array, file_info,
			     sub_info);
	}
	return 0;
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
		    struct cmd_array *cmd_array, struct file_info *file_info,
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
							  cmd_array, file_info,
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
			//*parse_info->copy++ = *ptr;
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
		      struct cmd_array *cmd_array, struct file_info *file_info,
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
			    execute_token(++ptr, parse_info, cmd_array,
					  file_info, sub_info);
			ptr--;
			break;
		case ')':
		case '{':
		case '}':
			error_token(*ptr, ptr);
			return NULL;
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
	      struct cmd_array *cmd_array, struct file_info *file_info,
	      struct sub_info *sub_info)
{
	// Check if next char is \0
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, cmd_array,
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
				ptr++;
				if (*ptr == '/' || *ptr == ' ' || *ptr == '\n') {
					ptr--;
					substitution_tokenize("HOME",
							      parse_info,
							      cmd_array,
							      file_info,
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
					ptr--;
					*parse_info->copy++ = *ptr;
					*parse_info->copy++ = *++ptr;
				}
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
			    substitution_tokenize(++ptr, parse_info, cmd_array,
						  file_info, sub_info);
			// Copy now sub_info to parse_info->copy
			if (copy_substitution(parse_info, sub_info->buffer) < 0) {
				return NULL;
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
			error_token(*ptr, ptr);
			return NULL;
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
	      struct cmd_array *cmd_array, struct file_info *file_info,
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
			    soft_apost_tokenize(++ptr, parse_info, cmd_array,
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
			    substitution_tokenize(++ptr, parse_info, cmd_array,
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
	strcat(cmd_array->commands[cmd_array->n_cmd - 1]->current_arg,
	       line_buf);

	// Do substitution and update command
	glob_t gstruct;

	if (glob(cmd_array->commands[cmd_array->n_cmd - 1]->current_arg,
		 GLOB_ERR, NULL, &gstruct) == GLOB_NOESCAPE) {
		//TODO: add error
		return NULL;
	}

	char **found;

	found = gstruct.gl_pathv;
	if (found == NULL) {
		add_arg(cmd_array->commands[cmd_array->n_cmd - 1]);
	} else {
		while (*found != NULL) {
			strcpy(cmd_array->commands[cmd_array->n_cmd - 1]->
			       current_arg, *found);
			add_arg(cmd_array->commands[cmd_array->n_cmd - 1]);
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
	      struct cmd_array *cmd_array, struct file_info *file_info,
	      struct sub_info *sub_info)
{
	int n_parenthesis = 0;

	// Read again and parse until )
	char *buffer = malloc(sizeof(char) * 1024);

	// Check if malloc failed
	if (buffer == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
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
	cmd_tokenize(buffer, parse_info, cmd_array, file_info, sub_info);
	free(line_buf);
	free(buffer);
	return ptr;
}

void
request_new_line(char *line)
{
	printf("> ");
	fgets(line, 1024, stdin);
	if (ferror(stdin)) {
		err(EXIT_FAILURE, "fgets failed");
	}
}

void
new_argument(struct command *current_cmd, struct parse_info *parse_info,
	     struct cmd_array *cmd_array, struct file_info *file_info,
	     struct sub_info *sub_info)
{
	if (strcmp(sub_info->last_alias, current_cmd->argv[0]) != 0) {
		if (check_alias_cmd(current_cmd)) {
			strcpy(sub_info->last_alias, current_cmd->argv[0]);
			cmd_tokenize(get_alias
				     (current_cmd->argv[0]),
				     parse_info,
				     cmd_array, file_info, sub_info);
			return;
		}
	}
	add_arg(current_cmd);
}

int
error_token(char token, char *line)
{
	fprintf(stderr,
		"dash: syntax error in '%c' near unexpected token `%s", token,
		line);
	return -1;
}

int
check_here_doc(char *line, struct cmd_array *cmd_array)
{
	char *ptr = line;

	if (*ptr++ == 'H' && *ptr++ == 'E' && *ptr++ == 'R' && *ptr++ == 'E'
	    && *ptr == '{') {
		// TODO: check input
		if (set_file_cmd
		    (cmd_array->commands[cmd_array->n_cmd - 1],
		     HERE_DOC_READ, "") >= 0) {
			return 1;
		}
	}
	return 0;
}
