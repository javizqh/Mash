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

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "open_files.h"
#include "builtin/command.h"
#include "builtin/builtin.h"
#include "builtin/export.h"
#include "builtin/source.h"
#include "builtin/alias.h"
#include "builtin/exit.h"
#include "parse_line.h"
#include "mash.h"
#include "exec_cmd.h"
#include "builtin/jobs.h"

// DECLARE STATIC FUNCTIONS
static int usage();
static void print_job_builtin(struct job * job, int flag_only_run, int flag_only_stop, int flag_only_id, int flag_print_id);

struct job_list jobs_list;

static int usage() {
	fprintf(stderr,"Usage: jobs [-lprs] [jobspec]\n");
	return EXIT_FAILURE;
}

int jobs(int argc, char *argv[]) {
	argc--;argv++;
	int i;
	char *arg_ptr;

	// Flags
	int print_id = 0;
	int only_id = 0;
	int only_run = 0;
	int only_stop = 0;
	pid_t only_job = 0;

	struct job * current;
	remove_job(get_job(get_relevance_job_pid(0)));  // Remove itself
	if (argc > 2) {
		return usage();
	}
	for (i = 0; i < argc; i++)
	{
		arg_ptr = argv[i];
		arg_ptr++;
		if (*argv[i] == '-') {
			for (;*arg_ptr != '\0';arg_ptr++) {
				switch (*arg_ptr)
				{
				case 'l':
					print_id = 1;
					break;
				case 'p':
					only_id = 1;
					break;
				case 'r':
					only_run = 1;
					break;
				case 's':
					only_stop = 1;
					break;
				default:
					return usage();
					break;
				}
			}
		} else if (*argv[i] == '%') {
			if ((only_job = substitute_jobspec(arg_ptr)) < 0) {
				return usage();
			}
		} else {
			return usage();
		}
	}

	if (only_job) {
		current = get_job(only_job);
		print_job_builtin(current, only_run, only_stop, only_id, print_id);
		return EXIT_SUCCESS;
	} else {
		for (current = jobs_list.head; current; current = current->next_job) {
			print_job_builtin(current, only_run, only_stop, only_id, print_id);
		}
	}
	return EXIT_SUCCESS;
}

static void print_job_builtin(struct job * job, int flag_only_run, int flag_only_stop, int flag_only_id, int flag_print_id) {
	if (flag_only_id) {
		if (flag_only_run || flag_only_stop) {
			if (flag_only_run) {
				if (job->state == RUNNING) {
					printf("%d\n", job->pid);
				}
			}
			if (flag_only_stop) {
				if (job->state == STOPPED) {
					printf("%d\n", job->pid);
				}
			}
		} else {
			printf("%d\n", job->pid);
		}
	} else {
		if (flag_only_run || flag_only_stop) {
			if (flag_only_run) {
				if (job->state == RUNNING) {
					print_job(job,flag_print_id);
				}
			}
			if (flag_only_stop) {
				if (job->state == STOPPED) {
					print_job(job,flag_print_id);
				}
			}
		} else {
			print_job(job,flag_print_id);
		}
	}
}

pid_t substitute_jobspec(char* jobspec) {
	if (strlen(jobspec) == 0) {
		return get_relevance_job_pid(0);
	}
	if (strlen(jobspec) == 1) {
		if (*jobspec == '+' || *jobspec == '%') {
			return get_relevance_job_pid(0);
		} else if (*jobspec == '-'){
			return get_relevance_job_pid(1);
		}
	}
	if (atoi(jobspec) > 0) {
		return get_pos_job_pid(atoi(jobspec));
	} 
	return -1;
}

int
launch_job(FILE * src_file, struct exec_info *exec_info, char * to_free_excess){

	if (has_builtin_modify_cmd(exec_info->command)) {
		switch (modify_cmd_builtin(exec_info->command)) {
			case CMD_EXIT_FAILURE:
				return CMD_EXIT_FAILURE;
				break;
			case CMD_EXIT_NOT_EXECUTE:
				return EXIT_SUCCESS; // Not execute more
				break;
		}
	}

	if (search_in_builtin && has_builtin_exec_in_shell(exec_info->command)) {
		close_all_fd(exec_info->command);
		return exec_builtin_in_shell(exec_info->command);
	}

	struct job * job = new_job(exec_info->line);

	if (exec_info->command->do_wait == DO_NOT_WAIT_TO_FINISH) {
		job->execution = BACKGROUND;
	}
	
	add_job(job);

	int a = exec_job(src_file, exec_info, job, to_free_excess);

	search_in_builtin = 1;

	return a;
}

int
exec_job(FILE * src_file, struct exec_info *exec_info, struct job * job, char * to_free_excess)
{
	struct command *current_command;

	if (set_input_shell_pipe(exec_info->command) || set_output_shell_pipe(exec_info->command)
	    || set_err_output_shell_pipe(exec_info->command)) {
		return 1;
	}
	// Make a loop fork each command
	for (current_command = exec_info->command; current_command; current_command = current_command->pipe_next)
	{
		current_command->pid = fork();
		if (current_command->pid == 0) {
			break;
		}
		if (current_command->pipe_next == NULL) {
			break;
		}
	}

	switch (current_command->pid) {
	case -1:
		close_all_fd(exec_info->command);
		remove_job(job);
		fprintf(stderr, "Mash: Failed to fork");
		return EXIT_FAILURE;
		break;
	case 0:
		struct command *start_command = exec_info->command;
		free_exec_info(exec_info);
		free(to_free_excess);
		if (src_file != stdin)
			fclose(src_file);
		if (reading_from_file) {
			fclose(stdin);
		}
		exec_cmd(current_command, start_command,
			   current_command->pipe_next);
		err(EXIT_FAILURE, "Failed to exec");
		break;
	default:
		job->pid = exec_info->command->pid;
		job->end_pid = exec_info->last_command->pid;
		setpgid(exec_info->command->pid,0);
		close_all_fd_io(exec_info->command, current_command);
		if (exec_info->command->input == HERE_DOC_FILENO) {
			read_from_here_doc(exec_info->command);
		} else {
			read_from_file(exec_info->command);
		}
		write_to_file_or_buffer(current_command);
		if (job->execution == FOREGROUND) {
			return wait_job(job);
		} else {
			printf("[%d]\t%d\n", job->pos,job->pid);
			return EXIT_SUCCESS;
		}
		break;
	}
	return 1;
}

int
wait_job(struct job * job)
{
	int wstatus;
	pid_t wait_pid;
	
	while (1) {
		wait_pid = waitpid(-1,&wstatus,WUNTRACED);
		if (WIFEXITED(wstatus)) {
			if (wait_pid == -1) {
				perror("waitpid failed 2");
				return EXIT_FAILURE;
			}
			// If last command then save return value
			if (wait_pid == job->end_pid) {
				remove_job(job);
				return WEXITSTATUS(wstatus);
			}
		} else if (WIFSTOPPED(wstatus)) {
			return EXIT_SUCCESS;
		} else if (WIFSIGNALED(wstatus)) {
			return EXIT_SUCCESS;
		}
	}
	return EXIT_FAILURE;
}

struct job * new_job(char * line) {
  struct job * job = malloc(sizeof(struct job));
  if (job == NULL) {
    err(EXIT_FAILURE, "malloc failed");
  }
  memset(job, 0, sizeof(struct job));
	job->execution = FOREGROUND;
  job->pos = 0;
  job->relevance = 0;
  job->pid = 0;
  job->state = RUNNING;
	char * command = malloc(1024);
  if (command == NULL) {
    err(EXIT_FAILURE, "malloc failed");
  }
  memset(command, 0, 1024);
	job->command = command;
  strcpy(job->command,line);
	if (job->command[strlen(job->command) - 1] == '\n') {
		job->command[strlen(job->command) - 1] = '\0';
	}
  job->next_job = NULL;
  return job;
}

void print_job(struct job * job, int print_id) {
	char relevance;
	char *state;
	switch (job->relevance) {
	case 0:
		relevance = '+';
		break;
	case 1:
		relevance = '-';
		break;
	default:
		relevance = ' ';
		break;
	}
	switch (job->state) {
	case RUNNING:
		state = "Running";
		break;
	case STOPPED:
		state = "Stopped";
		break;
	case DONE:
		state = "Done";
		break;
	}
	if (print_id) {
		printf("[%d]%c\t%d\t%s\t\t%s\n", job->pos, relevance,job->pid, state, job->command);
	} else {
		printf("[%d]%c\t%s\t\t%s\n", job->pos, relevance, state, job->command);
	}
}

int init_jobs_list() {
	jobs_list.head = NULL;
	jobs_list.last = NULL;
	jobs_list.n_jobs = 0;
	return 1;
}

void free_jobs_list() {
	struct job * current;
	struct job * to_free;

	for (current = jobs_list.head; current;) {
		to_free = current;
		current = current->next_job;
		free(to_free->command);
		free(to_free);
	}
}

int add_job(struct job * job) {
	int n_foregrounds = 0;
	int max_pos = 0;
	struct job * current;

	if (job->execution == FOREGROUND) {
		for (current = jobs_list.head; current; current = current->next_job) {
			if (current->execution == FOREGROUND) {
				n_foregrounds++;
			}
			current->relevance++;
			if (current->pos > max_pos) {
				max_pos = current->pos;
			}
		}
	} else {
		for (current = jobs_list.head; current; current = current->next_job) {
			if (current->execution == FOREGROUND) {
				n_foregrounds++;
			}
			if (current->execution == BACKGROUND) {
				current->relevance++;
			}
			if (current->pos > max_pos) {
				max_pos = current->pos;
			}
		}
	}
	if (jobs_list.head == NULL) {
		jobs_list.head = job;
	} else {
		jobs_list.last->next_job = job;
	}
	jobs_list.last = job;
	jobs_list.n_jobs++;
	job->pos = ++max_pos;
	if (job->execution == FOREGROUND) {
		job->relevance = 0;
	} else {
		job->relevance = n_foregrounds;
	}
	return jobs_list.n_jobs;
}

struct job * get_job(pid_t job_pid) {
	struct job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->pid == job_pid) {
			return current;
		}
	}
	return NULL;
}

int remove_job(struct job * job) {
	struct job * current;
	struct job * previous = NULL;

	// Update relevance
	for (current = jobs_list.head; current; current = current->next_job) {
		current->relevance--;
	}

	if (jobs_list.head->pid == job->pid) {
		jobs_list.head = job->next_job;
		if (jobs_list.last->pid == job->pid) {
			jobs_list.last = NULL;
		}
		jobs_list.n_jobs--;
		free(job->command);
		free(job);
		return 0;
	}

	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->pid == job->pid) {
			if (jobs_list.last->pid == job->pid) {
				jobs_list.last = previous;
			}
			previous->next_job = current->next_job;
			jobs_list.n_jobs--;
			free(current->command);
			free(current);
			return 0;
		}
		previous = current;
	}
	return -1;
}

int remove_done_jobs() {
	struct job * current;
	struct job * previous = NULL;
	int add_relevance = 0;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->state == DONE) {
			if (remove_job(current) < 0) {
				// TODO: add error
			}
			if (previous == NULL) {
				current = jobs_list.head;
			} else {
				current = previous;
			}
		}
		previous = current;
		if (current == NULL) {
			break;
		}
	}
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->relevance < 0) {
			add_relevance = 1;
		}
	}
	if (add_relevance) {
		for (current = jobs_list.head; current; current = current->next_job) {
			current->relevance++;
		}
	}
	return 1;
}

pid_t get_pos_job_pid(int pos) {
	struct job * current;
	if (jobs_list.n_jobs < pos) {
		return 0;
	}

	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->pos == pos) {
			return current->pid;
		}
	}
	return 0;
}

pid_t get_relevance_job_pid(int relevance) {
	struct job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->relevance == relevance) {
			return current->pid;
		}
	}
	return -1;
}

int are_jobs_stopped() {
	int stopped_jobs = 0;
	struct job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->state == STOPPED) {
			stopped_jobs++;
		}
	}
	return stopped_jobs;
}

void stop_current_job() {
	struct job * current_job = get_job(get_relevance_job_pid(0));
	if (current_job != NULL) {
		kill(current_job->pid, SIGTSTP);
		current_job->state = STOPPED;
		printf("\n");
		print_job(current_job,0);
	}
}

void end_current_job() {
	struct job * current_job = get_job(get_relevance_job_pid(0));
	if (current_job != NULL) {
		kill(current_job->pid, SIGINT);
		remove_job(current_job);
	}
}

void wait_all_jobs() {
	int wstatus;
	pid_t wait_pid;

	if (are_jobs_stopped()) {
		return;
	}
	
	while (jobs_list.n_jobs > 0) {
		wait_pid = waitpid(-1,&wstatus,WUNTRACED);
		if (wait_pid == -1) {
			perror("waitpid failed");
			return;
		}
		jobs_list.n_jobs--;
	}
	return;
}

void update_jobs() {
	// Check for finished jobs
	struct job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if ( current->state == RUNNING && kill(current->pid,0) < 0) {
			current->state = DONE;
			print_job(current,0);
		}
	}
	remove_done_jobs();
}