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

#include "mash.h"

int
main(int argc, char **argv)
{
	// First set all enviroment variables
	if (set_env("env/env_variables.dash")) {
		printf("Error while setting the enviroment variables\n");
		exit(EXIT_FAILURE);
	}
	// Then set aliases
	struct alias **aliases = init_aliases("env/aliases.dash");

	if (aliases == NULL) {
		printf("Error while setting the aliases\n");
		exit(EXIT_FAILURE);
	}
	// ---------- Read command line
	// ------ Buffer
	// Initialize buffer
	char *buf = malloc(sizeof(char[1024]));

	// Check if malloc failed
	if (buf == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	// Initialize buffer to 0
	memset(buf, 0, 1024);
	// ------------

	printf("\033[01;35m%s \033[0m", getenv("PROMPT"));
	while (fgets(buf, 1024, stdin)) {	/* break with ^D or ^Z */
		if (find_command(buf, aliases, NULL) == -1) {
			exit_dash(aliases);
			free(buf);
			return 0;
		}
		//fwrite(buf, 1, strlen(buf), stdout);
		// Print Prompt
		printf("\033[01;35m%s \033[0m", getenv("PROMPT"));
	}
	exit_dash(aliases);
	free(buf);
	return 0;
}

// --------- Enviroment ------------

int
set_env(const char *env_file)
{
	// Open and read env file
	FILE *fd_env = fopen(env_file, "r");

	// Create Buffer
	char line[LINE_SIZE];

	// ----------- Read file and write

	while (fgets(line, sizeof(line), fd_env)) {
		/* note that fgets don't strip the terminating \n, checking its
		   presence would allow to handle lines longer that sizeof(line) */
		// If NULL then not matched
		add_env(line + strlen("export") + 1);
	}
	// -----------------------------------------

	if (fclose(fd_env)) {
		err(EXIT_FAILURE, "close failed");
	}
	return 0;
}

// ----------- Builtin -------------

int
find_builtin(struct command *command, struct alias **aliases)
{
	// If the argc is 1 and contains = then export
	//if (command->argc == 1 && strrchr(command->argv[0], '=')) {
	//      return add_env(command->argv[0]);
	//}
//
	//if (strcmp(command->argv[0], "alias") == 0) {
	//      // If doesn't contain alias
	//      add_alias(command->argv[0] + strlen("alias") + 1, aliases);
	//      return 0;
	//} else if (strcmp(command->argv[0], "export") == 0) {
	//      // If doesn't contain alias
	//      return add_env(command->argv[0] + strlen("export") + 1);
	//} else if (strcmp(command->argv[0], "echo") == 0) {
	//      // If doesn't contain alias
	//      int i;
//
	//      for (i = 1; i < command->argc; i++) {
	//              if (i > 1) {
	//                      dprintf(command->output, " %s",
	//                              command->argv[i]);
	//              } else {
	//                      dprintf(command->output, "%s",
	//                              command->argv[i]);
	//              }
	//      }
	//      if (command->output == STDOUT_FILENO) {
	//              printf("\n");
	//      }
	//      return 0;
	//} else 
	if (strcmp(command->argv[0], "exit") == 0) {
		// If doesn't contain alias
		return -1;
	}
	return 1;
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
	parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
	parse_info->do_not_expect_new_cmd = 0;
	parse_info->copy = NULL;

	return parse_info;
}

// ---------- Commands -------------

struct cmd_array *
new_commands()
{
	struct cmd_array *commands =
	    (struct cmd_array *)malloc(sizeof(struct cmd_array));
	// Check if malloc failed
	if (commands == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(commands, 0, sizeof(struct cmd_array));

	commands->n_cmd = 0;
	commands->status = 0;

	return commands;
}

void
free_cmd_array(struct cmd_array *cmd_array)
{
	int i;

	for (i = 0; i < cmd_array->n_cmd; i++) {
		free_command(cmd_array->commands[i]);
		cmd_array->commands[i] = NULL;
	}
	free(cmd_array);
	return;
}

int
add_command(struct command *command_to_add, struct cmd_array *commands)
{
	if (commands->commands[commands->n_cmd] == NULL) {
		commands->commands[commands->n_cmd] = command_to_add;
		return ++commands->n_cmd;
	}
	return -1;
}

int
do_not_wait_commands(struct cmd_array *cmd_array)
{
	int i;

	for (i = 0; i < cmd_array->n_cmd; i++) {
		set_to_background_cmd(cmd_array->commands[i]);
	}
	return 0;
}

struct cmd_array *
set_commands(char *line, struct alias **aliases)
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
	if (cmd_tokenize
	    (line, parse_info, commands, file_info, sub_info, aliases) < 0) {
		commands->status = -1;
	}
	free(parse_info);
	free(file_info);
	free(sub_info);
	return commands;
}

int
find_command(char *line, struct alias **aliases, char *buffer)
{
	struct cmd_array *commands = set_commands(line, aliases);

	if (commands->status < 0) {
		free_cmd_array(commands);
		return 1;
	}

	if (buffer != NULL) {
		commands->commands[commands->n_cmd - 1]->output_buffer = buffer;
	}
	// char *command_with_path;
	int status = 0;
	int i;

	// Alias -> builtin -> PWD -> PATH -> exec
	// If alias check if exists command
	for (i = 0; i < commands->n_cmd; i++) {
		// Check if the cmd array is empty
		if (*commands->commands[i]->argv[0] == '\0') {
			free_cmd_array(commands);
			return 1;
		}
		switch (commands->commands[i]->prev_status_needed_to_exec) {
			// TODO: add check for exit
		case DO_NOT_MATTER_TO_EXEC:
			status = find_builtin(commands->commands[i], aliases);
			if (status > 0) {
				status = exec_command(commands->commands[i]);
			} else if (status == -1) {
				free_cmd_array(commands);
				return status;
			}
			break;
		case EXECUTE_IN_SUCCESS:
			if (status == 0) {
				status =
				    find_builtin(commands->commands[i],
						 aliases);
				if (status > 0) {
					status =
					    exec_command(commands->commands[i]);
				} else if (status == -1) {
					free_cmd_array(commands);
					return status;
				}
			}
			break;
		case EXECUTE_IN_FAILURE:
			if (status != 0) {
				status =
				    find_builtin(commands->commands[i],
						 aliases);
				if (status > 0) {
					status =
					    exec_command(commands->commands[i]);
				} else if (status == -1) {
					free_cmd_array(commands);
					return status;
				}
			} else {
				status = 0;
			}
			break;
		}
		//command_with_path = get_alias(commands->commands[i]->argv[0], aliases);

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

// ------------ Exit ---------------

int
exit_dash(struct alias **aliases)
{
	int i;

	for (i = 0; i < 2; i++) {
		free(aliases[i]);
	}
	free(aliases);
	return 0;
}

// New TOKENIZATION Recursive

int
cmd_tokenize(char *line, struct parse_info *parse_info,
	     struct cmd_array *cmd_array, struct file_info *file_info,
	     struct sub_info *sub_info, struct alias **aliases)
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
		new_cmd = cmd_array->commands[cmd_array->n_cmd - 1];
		if (*new_cmd->current_arg != '\0') {
			add_arg(new_cmd);
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
						file_info, sub_info, aliases);
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
					  file_info, sub_info, aliases);
			if (set_file_cmd
			    (cmd_array->commands[cmd_array->n_cmd - 1],
			     INPUT_READ, file_info->buffer)) {
				return -1;
			}
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '>':
			parse_info->copy = file_info->buffer;
			ptr =
			    file_tokenize(++ptr, parse_info, cmd_array,
					  file_info, sub_info, aliases);
			if (set_file_cmd
			    (cmd_array->commands[cmd_array->n_cmd - 1],
			     OUTPUT_WRITE, file_info->buffer) < 0) {
				return -1;
			}

			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
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
						  file_info, sub_info, aliases);
			// Copy now sub_info to parse_info->copy
			if (copy_substitution(parse_info, sub_info->buffer) < 0) {
				return -1;
			}
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		case ' ':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				add_arg(new_cmd);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
			}
			break;
		case '\n':
			if (parse_info->has_arg_started == PARSE_ARG_STARTED) {
				add_arg(new_cmd);
				parse_info->copy = new_cmd->current_arg;
				parse_info->has_arg_started =
				    PARSE_ARG_NOT_STARTED;
			}
			break;
		case ';':
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			new_cmd = new_command();
			if (add_command(new_cmd, cmd_array) < 0) {
				fprintf(stderr, "Failed to add command");
			}
			parse_info->copy = new_cmd->current_arg;
			parse_info->has_arg_started = PARSE_ARG_NOT_STARTED;
			break;
		case '&':
			// Check if next char is &
			if (*++ptr == '&') {
				// Add an argument to old command
				old_cmd = new_cmd;
				if (parse_info->has_arg_started ==
				    PARSE_ARG_STARTED) {
					add_arg(old_cmd);
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
			if (parse_info->do_not_expect_new_cmd)
				return error_token('&', ptr);
			*parse_info->copy++ = *ptr;
			parse_info->has_arg_started = PARSE_ARG_STARTED;
			break;
		}
		if (ptr == NULL)
			return -1;
	}
	// End of substitution
	if (new_cmd->current_arg[0] != '\0') {
		add_arg(new_cmd);
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
		    struct sub_info *sub_info, struct alias **aliases)
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
							  sub_info, aliases);
				// Copy now sub_info to parse_info->copy
				if (copy_substitution
				    (parse_info, sub_info->buffer) < 0) {
					return NULL;
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
		      struct sub_info *sub_info, struct alias **aliases)
{
	sub_info->ptr = sub_info->buffer;
	// Can't call any other function
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		*sub_info->ptr++ = *ptr;
		switch (*ptr) {
		case '"':
		case '\'':
		case ' ':
		case ';':
		case '<':
		case '>':
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
					  file_info, sub_info, aliases);
			ptr--;
			break;
		case ')':
			error_token(*ptr, ptr);
			return NULL;
			break;
			// REVIEW: Special cases
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
	      struct sub_info *sub_info, struct alias **aliases)
{
	// Check if next char is \0
	char *ptr;

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '"':
			ptr =
			    soft_apost_tokenize(++ptr, parse_info, cmd_array,
						file_info, sub_info, aliases);
			break;
		case '\'':
			ptr = hard_apost_tokenize(++ptr, parse_info);
			break;
		case '|':
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
						  file_info, sub_info, aliases);
			// Copy now sub_info to parse_info->copy
			if (copy_substitution(parse_info, sub_info->buffer) < 0) {
				return NULL;
			}
			break;
		case ' ':
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
			error_token(*ptr, ptr);
			return NULL;
			break;
		default:
			*parse_info->copy++ = *ptr;
			break;
		}
		if (ptr == NULL)
			return ptr;
	}
	// Ask for more
	return --ptr;
}

char *
execute_token(char *line, struct parse_info *parse_info,
	      struct cmd_array *cmd_array, struct file_info *file_info,
	      struct sub_info *sub_info, struct alias **aliases)
{
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

	for (ptr = line; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '\\':
			*line_buf_ptr++ = *++ptr;
			break;
		case ')':
			*line_buf_ptr = '\0';
			*ptr-- = '\0';
			break;
		case '\n':
			*line_buf_ptr = '\0';
			request_new_line(line);
			ptr = line;
			ptr--;
			break;
		default:
			*line_buf_ptr++ = *ptr;
			break;
		}
	}
	// Copy result into sub_info->buffer
	find_command(line_buf, aliases, buffer);
	cmd_tokenize(buffer, parse_info, cmd_array, file_info, sub_info,
		     aliases);
	free(line_buf);
	free(buffer);
	return ptr;
}

void
request_new_line(char *line)
{
	printf("> ");
	fgets(line, 1024, stdin);
}

int
error_token(char token, char *line)
{
	fprintf(stderr,
		"dash: syntax error in '%c' near unexpected token `%s", token,
		line);
	return -1;
}
