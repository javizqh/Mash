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

#include <stdlib.h>
#include "builtin/exit.h"
#include "builtin/source.h"
#include "builtin/alias.h"

int has_to_exit = 0;

int
exit_mash()
{
	int i;

  for (i = 0; i < ALIAS_MAX; i++) {
		if (aliases[i] == NULL) break;
		free(aliases[i]);
	}

  free_source_file();

	has_to_exit = 1;
  return EXIT_SUCCESS;
}