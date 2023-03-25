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

#include "builtin/command.h"

struct command *
new_command()
{
	struct command *command =
	    (struct command *)malloc(sizeof(struct command));
	// Check if malloc failed
	if (command == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(command, 0, sizeof(struct command));

	command->argc = 0;
	command->current_arg = command->argv[0];
  command->pid = 0;
  command->prev_status_needed_to_exec = DO_NOT_MATTER_TO_EXEC;
  command->do_wait = WAIT_TO_FINISH;
	command->input = STDIN_FILENO;
	command->output = STDOUT_FILENO;
  command->fd_pipe_input[0] = -1;
  command->fd_pipe_input[1] = -1;
	command->fd_pipe_output[0] = -1;
	command->fd_pipe_output[1] = -1;
  command->pipe_next = NULL;
  command->output_buffer = NULL;

	return command;
}

void 
free_command(struct command *command) {
  struct command * to_free = command;
  struct command * next = command;
  while (next != NULL)
  { 
    to_free = next;
    next = to_free->pipe_next;
    free(to_free);
  }
}

int
add_arg(struct command *command)
{
	if (++command->argc > MAX_ARGUMENTS)
		return 0;
	command->current_arg = command->argv[command->argc];
	return 1;
}

int set_file_cmd(struct command *command,int file_type, char *file) {
  switch (file_type) {
	case INPUT_READ:
		if (command->input != STDIN_FILENO) {
			close(command->input);
		}
		command->input = open_read_file(file);
		return command->input;
		break;
	case OUTPUT_WRITE:
    // GO TO LAST CMD IN PIPE
    struct command * last_cmd = get_last_command(command);
		if (last_cmd->output !=
		    STDOUT_FILENO) {
			close(last_cmd->output);
		}
		last_cmd->output = open_write_file(file);
		return last_cmd->output;
		break;
	}
	return -1;
}

int set_to_background_cmd(struct command *command) {
  command->do_wait = DO_NOT_WAIT_TO_FINISH;
  if (command->input == STDIN_FILENO) {
    set_file_cmd(command, INPUT_READ, "/dev/null");
  }
  return 0;
}

struct command * get_last_command(struct command *command) {
  struct command * current_command = command;

  while (current_command->pipe_next != NULL) {
    current_command = current_command->pipe_next;
  } 
  return current_command;
}

int pipe_command(struct command *in_command, struct command * out_command) {
  int fd[2];
  if (pipe(fd) < 0) {
    err(EXIT_FAILURE, "Failed to pipe");
  }
  in_command->pipe_next = out_command;
  in_command->fd_pipe_output[0] = fd[0];
  in_command->fd_pipe_output[1] = fd[1];
  out_command->fd_pipe_input[0] = fd[0];
  out_command->fd_pipe_input[1] = fd[1];
  return 1;
}

int set_alias_in_cmd(struct command * command, struct alias **aliases);

int find_path(struct command *command) {
  // Check if the first character is /
  if (*command->argv[0] == '/') {
    return command_exists(command->argv[0]);
  }

  // THEN CHECK IN PWD
  char *pwd = malloc(sizeof(char) * MAX_ENV_SIZE);
  if (pwd == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(pwd, 0, sizeof(char) * MAX_ENV_SIZE);
  // Copy the path
  strcpy(pwd, getenv("PWD"));
  if (pwd == NULL) {
    free(pwd);
    return -1;
  }
  char *pwd_ptr = pwd;
  strcat(pwd_ptr,command->argv[0]);

  if (command_exists(pwd_ptr)) {
    strcpy(command->argv[0],pwd_ptr);
    free(pwd);
    return 1;
  }
  free(pwd);
  // First get the path from env PATH
  char *path = malloc(sizeof(char) * MAX_ENV_SIZE);
  if (path == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(path, 0, sizeof(char) * MAX_ENV_SIZE);
  // Copy the path
  strcpy(path, getenv("PATH"));
  if (path == NULL) {
    free(path);
    return -1;
  }
  char *path_ptr = path;
  // Then separate the path by : using strtok
  char path_tok[MAX_ENV_SIZE][MAX_PATH_SIZE];

  char *token;
  int path_len = 0;
  while ((token = strtok_r(path_ptr, ":", &path_ptr))) {
    strcpy(path_tok[path_len], token);
    path_len++;
  }
  // Loop through the path until we find an exec
  int i;
  for ( i = 0; i < path_len; i++) {
    strcat(path_tok[i],command->argv[0]);

    if (command_exists(path_tok[i])) {
      strcpy(command->argv[0],path_tok[i]);
      free(path);
      return 1;
    }
  }
  free(path);
  return 0;
}

int command_exists(char *path) {
  struct stat buf;

  if (stat(path, &buf) != 0) {
      return 0;
  }

  // Check the `st_mode` field to see if the `S_IXUSR` bit is set
  if (buf.st_mode & S_IXUSR) {
    return 1;
  }
  return 0;
}

int exec_command(struct command *command) {
  int wstatus = 0;
  int i;
  char *args[command->argc];
  int cmd_to_wait = 0;
  int proc_finished;
  int status = 0;
  pid_t wait_pid;

  struct command * current_command = command;
  struct command * last_command = command;

  // SET INPUT PIPE
  if ( command->input != STDIN_FILENO ) {
    int fd_write_shell[2] = {-1,-1};
    if (pipe(fd_write_shell) < 0) {
      err(EXIT_FAILURE, "Failed to pipe");
    }
    command->fd_pipe_input[0] = fd_write_shell[0];
    command->fd_pipe_input[1] = fd_write_shell[1];
  }

  // SET OUTOUT PIPE
  // Find last one and add it
  last_command = get_last_command(command);
  
  if ( last_command->output != STDOUT_FILENO || last_command->output_buffer != NULL) {
    int fd_read_shell[2] = {-1,-1}; 
    if (pipe(fd_read_shell) < 0) {
      err(EXIT_FAILURE, "Failed to pipe");
    }
    last_command->fd_pipe_output[0] = fd_read_shell[0];
    last_command->fd_pipe_output[1] = fd_read_shell[1];
  }

  current_command = command;

  // Make a loop fork each command
  do {
		current_command->pid = fork();
    last_command = current_command;
    current_command = last_command->pipe_next;
    cmd_to_wait++;
	} while (last_command->pid != 0 && current_command != NULL);
  // Find the command
  // Search in the path
  // Pipe the command to the next
  // Check if read from shell

  switch (last_command->pid)
  {
  case -1:
    close_fd(last_command->fd_pipe_output[0]);
    close_fd(last_command->fd_pipe_output[1]);
    close_fd(last_command->fd_pipe_input[0]);
    close_fd(last_command->fd_pipe_input[1]);
    err(EXIT_FAILURE, "Failed to fork");
    break;
  case 0:
    // CLOSE ALL PIPES EXCEPT MINE INPUT 0 OUTPUT 1
    // PREVIOUS CMD OUTPUT 0
    // NEXT CMD INPUT 1
    struct command * new_cmd = command;
    while (new_cmd != NULL)
    {
      // IF fd is the same don't close it
      if (new_cmd == last_command) {
        close_fd(new_cmd->fd_pipe_input[1]);
        close_fd(new_cmd->fd_pipe_output[0]);
      } else {
        if (new_cmd->fd_pipe_input[1] != last_command->fd_pipe_output[1] && new_cmd == last_command->pipe_next) {
          close_fd(new_cmd->fd_pipe_input[1]);
        }
        if (new_cmd->fd_pipe_output[0] != last_command->fd_pipe_input[0] && new_cmd->pipe_next == last_command) {
          close_fd(new_cmd->fd_pipe_output[0]);
        }
        close_fd(new_cmd->fd_pipe_input[0]);
        close_fd(new_cmd->fd_pipe_output[1]);
      }
      new_cmd = new_cmd->pipe_next;
    }
    // FIND PATH
    if (find_builtin2(last_command)) {
      // NOT INPUT COMMAND OR INPUT COMMAND WITH FILE
      if (last_command->pid != command->pid || last_command->input != STDIN_FILENO) {
        if (dup2(last_command->fd_pipe_input[0],STDIN_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 1");
        }
        close_fd(last_command->fd_pipe_input[0]);
      }
      
      // NOT LAST COMMAND OR LAST COMMAND WITH FILE OR BUFFER
      if (current_command != NULL || last_command->output_buffer != NULL || last_command->output != STDOUT_FILENO) {
        // redirect stdout
        if (dup2(last_command->fd_pipe_output[1],STDOUT_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 2");
        }
        // redirect stderr
        if (dup2(last_command->fd_pipe_output[1],STDERR_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 3");
        }
        close_fd(last_command->fd_pipe_output[1]);
      }
      exec_builtin(last_command);
    } else {
      if (!find_path(last_command)) {
        fprintf(stderr, "%s: command not found\n",
          last_command->argv[0]);
        close_fd(last_command->fd_pipe_input[0]);
        close_fd(last_command->fd_pipe_output[1]);
        exit(EXIT_FAILURE);
      }

      // NOT INPUT COMMAND OR INPUT COMMAND WITH FILE
      if (last_command->pid != command->pid || last_command->input != STDIN_FILENO) {
        if (dup2(last_command->fd_pipe_input[0],STDIN_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 1");
        }
        close_fd(last_command->fd_pipe_input[0]);
      }
      
      // NOT LAST COMMAND OR LAST COMMAND WITH FILE OR BUFFER
      if (current_command != NULL || last_command->output_buffer != NULL || last_command->output != STDOUT_FILENO) {
        // redirect stdout
        if (dup2(last_command->fd_pipe_output[1],STDOUT_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 2");
        }
        // redirect stderr
        if (dup2(last_command->fd_pipe_output[1],STDERR_FILENO) == -1) {
          err(EXIT_FAILURE, "Failed to dup 3");
        }
        close_fd(last_command->fd_pipe_output[1]);
      }


      for ( i = 0; i < last_command->argc; i++) {
        args[i] = last_command->argv[i];
      }
      args[last_command->argc] = NULL;

      execv(last_command->argv[0], args);
    }
    err(EXIT_FAILURE, "Failed to exec");
    break;
  default:
    // CLOSE ALL PIPES
    current_command = command;
    while (current_command != NULL)
    {
      close_fd(current_command->fd_pipe_input[0]);
      close_fd(current_command->fd_pipe_input[1]);
      if (current_command->pid != last_command->pid) {
        close_fd(current_command->fd_pipe_output[0]);
      }
      close_fd(current_command->fd_pipe_output[1]);
      current_command = current_command->pipe_next;
    } 

    ssize_t count = 4096;
    // READ FROM FILE
    if (command->input != STDIN_FILENO) {
      // Create stdin buffer
      ssize_t bytes_stdin = 4096;
      char *buffer_stdin = malloc(sizeof(char[4096]));
      if (buffer_stdin == NULL) {
        err(EXIT_FAILURE, "malloc failed");
      }
      // Initialize buffer to 0
      memset(buffer_stdin, 0, 4096);
      do {
        bytes_stdin = read(command->input, buffer_stdin, count);
        write(STDOUT_FILENO, buffer_stdin, bytes_stdin);
      } while (bytes_stdin > 0);

      free(buffer_stdin);
    }

    // WRITE TO FILE OR BUFFER DOES NOT FULLLY WORK
    if ( !(last_command->output == STDOUT_FILENO && last_command->output_buffer == NULL)) {
      ssize_t bytes_stdout = 4096;
      // Create stdout buffer
      char *buffer_stdout = malloc(sizeof(char[4096]));
      if (buffer_stdout == NULL) {
        err(EXIT_FAILURE, "malloc failed");
      }
      // Initialize buffer to 0
      memset(buffer_stdout, 0, 4096);
      do {
        bytes_stdout = read(last_command->fd_pipe_output[0], buffer_stdout, count);
        if (bytes_stdout > 0) {
          if (last_command->output_buffer == NULL) {
            write(last_command->output, buffer_stdout, bytes_stdout);
          } else {
            if (strlen(last_command->output_buffer) > 0) {
              strcat(last_command->output_buffer, buffer_stdout);
            } else {
              strcpy(last_command->output_buffer, buffer_stdout);
            }
          }
        }
      } while (bytes_stdout > 0);
      free(buffer_stdout);
      close_fd(last_command->fd_pipe_output[0]);
    }

    // WAIT FOR ALL CHILDS
    if (command->do_wait == WAIT_TO_FINISH) {
      current_command = command;
      // REVIEW: PROBLEM WITH NOT WAITING FOR COMMANDS AFTER USING BACKGROUND BECAUSE IS WAITING FOR BACKGROUND
      for (proc_finished = 0; proc_finished < cmd_to_wait; proc_finished++) {
        wait_pid = waitpid(current_command->pid,&wstatus,WUNTRACED);
        if (wait_pid == -1) {
          perror("waitpid failed");
          return EXIT_FAILURE;
        }
        if (WIFEXITED(wstatus)) {
          if (WEXITSTATUS(wstatus)) {
            // If last command then save return value
            if (wait_pid == last_command->pid) {
              status = 1;
            }
          }
        } else if (WIFSIGNALED(wstatus)) {
          printf("Killed by signal %d\n", WTERMSIG(wstatus));
          return EXIT_FAILURE;
        }
        current_command = current_command->pipe_next;
      }
    } else {
      //TODO: have a list to store the surpressed command and later check
      printf("[1] %d\n", command->pid);
    }
    break;
  }
  return status;
}

int close_fd(int fd) {
  if (fd >= 0) {
    close(fd);
    return 0;
  }
  return 1;
}

// BUILTIN

int find_builtin2(struct command *command) {
// If the argc is 1 and contains = then export
	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
		//return add_env(command->argv[0]);
    // export a b
    return 1;
	}

	if (strcmp(command->argv[0], "alias") == 0) {
		// If doesn't contain alias
		//add_alias(command->argv[0] + strlen("alias") + 1, aliases);
		return 1;
	} else if (strcmp(command->argv[0], "export") == 0) {
		// If doesn't contain alias
		//return add_env(command->argv[0] + strlen("export") + 1);
    return 1;
	} else if (strcmp(command->argv[0], "echo") == 0) {
		// If doesn't contain alias
		//int i;
//
		//for (i = 1; i < command->argc; i++) {
		//	if (i > 1) {
		//		dprintf(command->output, " %s",
		//			command->argv[i]);
		//	} else {
		//		dprintf(command->output, "%s",
		//			command->argv[i]);
		//	}
		//}
		//if (command->output == STDOUT_FILENO) {
		//	printf("\n");
		//}
		return 1;
	}
	return 0;
}

void exec_builtin(struct command *command) {
  if (strcmp(command->argv[0], "alias") == 0) {
		// If doesn't contain alias
		// add_alias(command->argv[0] + strlen("alias") + 1, aliases);
	} else if (strcmp(command->argv[0], "export") == 0) {
		// If doesn't contain alias
		add_env(command->argv[0] + strlen("export") + 1);
  } else if (strcmp(command->argv[0], "echo") == 0) {
		// If doesn't contain alias
		int i;

		for (i = 1; i < command->argc; i++) {
			if (i > 1) {
				dprintf(command->output, " %s",
					command->argv[i]);
			} else {
				dprintf(command->output, "%s",
					command->argv[i]);
			}
		}
		if (command->output == STDOUT_FILENO) {
			printf("\n");
		}
	}
  return;
}