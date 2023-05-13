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

extern char *builtin_use;
extern char *builtin_description;
extern char *builtin_help;

extern int N_BUILTINS;

// Builtin command
int builtin(Command * command);

// ---------------

int has_builtin_modify_cmd(Command * command);
int modify_cmd_builtin(Command * modify_command);

/**
 * @brief Executes builtins that need to be executed in the parent process
 * 
 * @param command 
 * @return 1 = Need to execute in child | 0 = Executed in parent process
 */
int has_builtin_exec_in_shell(Command * command);
int exec_builtin_in_shell(Command * command);

int find_builtin(Command * command);
void exec_builtin(Command * start_scommand, Command * command);
