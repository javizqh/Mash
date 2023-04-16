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

int find_path(struct command *command);
int command_exists(char *path);

int exec_pipe(FILE * src_file, struct exec_info *exec_info, char * to_free_excess);

void exec_command(int buffer_pipe,struct command *command);

void exec_child(struct command *command, struct command *start_command,
		struct command *last_command);

void wait_childs(struct command *start_command, struct command *last_command,
		int n_cmds);

// Redirect input and output: Parent
enum iobuffer {
	MAX_BUFFER_IO_SIZE = 1024 * 4
};

void read_from_here_doc(struct command *start_command);
void read_from_file(struct command *start_command);
void write_to_file_or_buffer(int buffer_pipe, struct command *last_command);

// Redirect input and output: Child
void redirect_stdin(struct command *command, struct command *start_command);
void redirect_stdout(struct command *command, struct command *last_command);
void redirect_stderr(struct command *command, struct command *last_command);

// File descriptor
void set_input_shell_pipe(struct command *command);
void set_output_shell_pipe(struct command *command);
void set_err_output_shell_pipe(struct command *command);

int close_fd(int fd);
int close_all_fd(struct command *start_command);
int close_all_fd_io(struct command *start_command, struct command *last_command);
int close_all_fd_cmd(struct command *command, struct command *start_command);
