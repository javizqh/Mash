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

enum {
	LINE_SIZE = 1024,	// In bytes
	ALIAS_MAX = 256,	// In bytes
	ALIAS_MAX_COMMAND = 64,	// In bytes
	ALIAS_MAX_REFERENCE = 128	// In bytes
};

extern struct alias *aliases[ALIAS_MAX];

struct alias {
	char command[ALIAS_MAX_COMMAND];
	struct command *reference;
};

extern struct alias *new_alias(const char *command, struct command *reference);

extern int add_alias(struct command *reference);

extern char* get_alias(const char* name);

extern struct alias **init_aliases();

int set_alias_in_cmd(struct command *command);
