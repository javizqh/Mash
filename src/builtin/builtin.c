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

//#include "builtin/builtin.h"

//int find_builtin(struct command *command) {
//// If the argc is 1 and contains = then export
//	if (command->argc == 1 && strrchr(command->argv[0], '=')) {
//		//return add_env(command->argv[0]);
//    // export a b
//    return 1;
//	}
//
//	if (strcmp(command->argv[0], "alias") == 0) {
//		// If doesn't contain alias
//		//add_alias(command->argv[0] + strlen("alias") + 1, aliases);
//		return 1;
//	} else if (strcmp(command->argv[0], "export") == 0) {
//		// If doesn't contain alias
//		//return add_env(command->argv[0] + strlen("export") + 1);
//    return 1;
//	} else if (strcmp(command->argv[0], "echo") == 0) {
//		// If doesn't contain alias
//		//int i;
////
//		//for (i = 1; i < command->argc; i++) {
//		//	if (i > 1) {
//		//		dprintf(command->output, " %s",
//		//			command->argv[i]);
//		//	} else {
//		//		dprintf(command->output, "%s",
//		//			command->argv[i]);
//		//	}
//		//}
//		//if (command->output == STDOUT_FILENO) {
//		//	printf("\n");
//		//}
//		return 1;
//	}
//	return 0;
//}
//
//void exec_builtin(struct command *command) {
//  if (strcmp(command->argv[0], "alias") == 0) {
//		// If doesn't contain alias
//		// add_alias(command->argv[0] + strlen("alias") + 1, aliases);
//	} else if (strcmp(command->argv[0], "export") == 0) {
//		// If doesn't contain alias
//		add_env(command->argv[0] + strlen("export") + 1);
//  } else if (strcmp(command->argv[0], "echo") == 0) {
//		// If doesn't contain alias
//		int i;
//
//		for (i = 1; i < command->argc; i++) {
//			if (i > 1) {
//				dprintf(command->output, " %s",
//					command->argv[i]);
//			} else {
//				dprintf(command->output, "%s",
//					command->argv[i]);
//			}
//		}
//		if (command->output == STDOUT_FILENO) {
//			printf("\n");
//		}
//	}
//  return;
//}