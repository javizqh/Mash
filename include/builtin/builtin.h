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

// Builtin command
int builtin(struct command * command);
// ---------------

int has_builtin_modify_cmd(struct command *command);
int modify_cmd_builtin(struct command *modify_command);

/**
 * @brief Executes builtins that need to be executed in the parent process
 * 
 * @param command 
 * @return 1 = Need to execute in child | 0 = Executed in parent process
 */
int has_builtin_exec_in_shell(struct command *command);
int exec_builtin_in_shell(struct command *command);

int find_builtin(struct command *command);
void exec_builtin(struct command *start_scommand, struct command *command);