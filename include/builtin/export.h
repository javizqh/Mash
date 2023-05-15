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

enum {
  MAX_ENV_SIZE = 512,
  MAX_PATH_SIZE = MAX_ENV_SIZE * 32,
  MAX_PATH_LOCATIONS = 128
};

extern char * export_use;
extern char * export_description;
extern char * export_help;

int export(int argc, char* argv[], int stdout_fd, int stderr_fd);

int add_env(const char * line);

int add_env_by_name(const char *key, const char *value);

char * get_env_by_name(const char *key);

void print_env();