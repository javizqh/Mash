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

#include "builtin/export.h"

int add_env(const char * line) {
  char *p = strchr(line, '=');
  if (p != NULL) {
    *p = '\0';
    // Remove \n
    char *eol = strchr(++p, '\n');
    if (eol != NULL) {
      *eol = '\0';
    }
    // Set environment variables
    setenv(line, p, 1);
    return 0;
  }
  return 1;
}

int add_env_by_name(const char *key, const char *value) {
  return setenv(key, value, 1);
}