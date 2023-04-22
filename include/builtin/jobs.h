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

enum exec_state {
  RUNNING,
  STOPPED,
  DONE,
  FOREGROUND,
  BACKGROUND
};

struct job {
  int execution; 
  int pos;
  int relevance;
  pid_t pid;
  pid_t end_pid;
  int state;
  char *command;
  struct job *next_job;
};

struct job_list {
  struct job * head;
  struct job * last;
  int n_jobs;
};

int jobs(int argc, char *argv[]);
pid_t substitute_jobspec(char* jobspec);

int launch_job(FILE * src_file, struct exec_info *exec_info, char * to_free_excess);
int exec_job(FILE * src_file, struct exec_info *exec_info, struct job * job, char * to_free_excess);
int wait_job(struct job * job);

struct job * new_job(char * line);
void print_job(struct job * job, int print_id);

int init_jobs_list();
void free_jobs_list();

int add_job(struct job * job);
struct job * get_job(pid_t job_pid);
int remove_job(struct job * job);
int remove_done_jobs();
pid_t get_pos_job_pid(int pos);
pid_t get_relevance_job_pid(int relevance);
int are_jobs_stopped();

void stop_current_job();
void end_current_job();
void wait_all_jobs();
void update_jobs();