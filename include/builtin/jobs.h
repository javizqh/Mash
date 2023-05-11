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
  RUNNING = -2,
  STOPPED = -1,
  DONE = 0,
  FOREGROUND,
  BACKGROUND,
  SUB_EXECUTION
};
typedef struct Job {
  int execution; 
  int pos;
  int relevance;
  pid_t pid;
  pid_t end_pid;
  int state;
  char *command;
  struct Job *next_job;
} Job;

typedef struct JobList {
  Job * head;
  Job * last;
  int n_jobs;
} JobList;

int jobs(int argc, char *argv[]);
int no_job(char * command);
pid_t substitute_jobspec(char* jobspec);

int launch_job(FILE * src_file, ExecInfo *exec_info, char * to_free_excess);
int exec_job(FILE * src_file, ExecInfo *exec_info, Job * job, char * to_free_excess);
int wait_job(Job * job);

Job * new_job(char * line);
void print_job(Job * job, int print_id);

int init_jobs_list();
void free_jobs_list();

int add_job(Job * job);
Job * get_job(pid_t job_pid);
int remove_job(Job * job);
int remove_done_jobs();
pid_t get_pos_job_pid(int pos);
pid_t get_relevance_job_pid(int relevance);
int are_jobs_stopped();

void stop_job(pid_t job_pid);
void end_current_job();
void wait_all_jobs();
void update_jobs();