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
#include "parse.h"
#include "exec_info.h"
#include "parse_line.h"
#include "mash.h"
#include "exec_cmd.h"
#include "builtin/jobs.h"

// DECLARE STATIC FUNCTIONS
static int usage();
static void print_job_builtin(Job * job, int flag_only_run, int flag_only_stop, int flag_only_id, int flag_print_id);

JobList jobs_list;

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

	Job * current;
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
			only_job = substitute_jobspec(argv[i]);
		} else {
			return usage();
		}
	}
	if (only_job) {
		current = get_job(only_job);
		if (current == NULL) {
			return no_job("jobs");
		}
		print_job_builtin(current, only_run, only_stop, only_id, print_id);
		return EXIT_SUCCESS;
	} else {
		for (current = jobs_list.head; current; current = current->next_job) {
			print_job_builtin(current, only_run, only_stop, only_id, print_id);
		}
	}
	return EXIT_SUCCESS;
}

static void print_job_builtin(Job * job, int flag_only_run, int flag_only_stop, int flag_only_id, int flag_print_id) {
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

int no_job(char *command) {
  fprintf(stderr,"mash: %s: no such a job\n",command);
	return EXIT_FAILURE;
}

pid_t substitute_jobspec(char* jobspec) {
	char *ptr = ++jobspec;
	if (strlen(ptr) == 0) {
		return get_relevance_job_pid(0);
	}
	if (strlen(ptr) == 1) {
		if (*ptr == '+' || *ptr == '%') {
			return get_relevance_job_pid(0);
		} else if (*ptr == '-'){
			return get_relevance_job_pid(1);
		}
	}
	if (atoi(ptr) > 0) {
		return get_pos_job_pid(atoi(ptr));
	} 
	return -1;
}

int
launch_job(FILE * src_file, ExecInfo *exec_info, char * to_free_excess){
	Command * cmd = exec_info->command;

	if (has_builtin_modify_cmd(cmd)) {
		switch (modify_cmd_builtin(cmd)) {
			case CMD_EXIT_FAILURE:
				return CMD_EXIT_FAILURE;
				break;
			case CMD_EXIT_NOT_EXECUTE:
				return EXIT_SUCCESS; // Not execute more
				break;
		}
	}

	Job * job = new_job(exec_info->line);

	if (search_in_builtin && has_builtin_exec_in_shell(cmd)) {
		if (cmd->do_wait == DO_NOT_WAIT_TO_FINISH) {
			job->execution = BACKGROUND;
			job->pid = getpid();
			add_job(job);
			printf("[%d]\t%d\n", job->pos,job->pid);
			print_job(job,0);
			remove_job(job);
			return EXIT_SUCCESS;
		}
		free(job->command);
		free(job);
		close_all_fd_no_fork(cmd);
		return exec_builtin_in_shell(cmd);
	}


	if (cmd->do_wait == DO_NOT_WAIT_TO_FINISH) {
		job->execution = BACKGROUND;
	}

	if (exec_info->parse_info->exec_depth) {
		job->execution = SUB_EXECUTION;
	}
	
	add_job(job);

	int a = exec_job(src_file, exec_info, job, to_free_excess);

	// FIX: solve searching pipe
	search_in_builtin = 1;

	return a;
}

int
exec_job(FILE * src_file, ExecInfo *exec_info, Job * job, char * to_free_excess)
{
	int exit_code = EXIT_FAILURE;
	Command *current_command;

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
		Command *start_command = exec_info->command;
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
		close_all_fd_io(exec_info->command, current_command);
		if (job->execution == BACKGROUND) {
			//kill(job->pid, SIGTTOU);
			if (exec_info->command->output_buffer != NULL) {
				write_to_buffer(current_command);
			}
			printf("[%d]\t%d\n", job->pos,job->pid);
			exit_code = EXIT_SUCCESS;
			break;
		} else if (job->execution == SUB_EXECUTION) {
			signal(SIGTSTP, SIG_IGN);
			setpgid(job->pid,0);
			if (exec_info->command->output_buffer != NULL) {
				write_to_buffer(current_command);
			}
			exit_code = wait_job(job);
			signal(SIGTSTP, sig_handler);
			break;
		}
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		setpgid(job->pid,0);
		if (exec_info->command->input == HERE_DOC_FILENO) {
			read_from_here_doc(exec_info->command);
		}
		// Pass foreground to
		if (!isatty(0)) {
			// FIX: error
		}
		tcsetpgrp(0, job->pid);
		if (exec_info->command->output_buffer != NULL) {
			write_to_buffer(current_command);
		}
		exit_code = wait_job(job);
		// Retrieve foreground
		tcsetpgrp(0, getpgrp());
		signal(SIGTTOU, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
		break;
	}
	return exit_code;
}

int
wait_job(Job * job)
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
			kill(job->pid, SIGTTOU);
			stop_job(wait_pid);
			return EXIT_SUCCESS;
		} else if (WIFSIGNALED(wstatus)) {
			if (wait_pid == job->end_pid) {
				remove_job(job);
				return EXIT_SUCCESS;
			}
		}
	}
	return EXIT_FAILURE;
}

Job * new_job(char * line) {
  Job * job = malloc(sizeof(Job));
  if (job == NULL) {
    err(EXIT_FAILURE, "malloc failed");
  }
  memset(job, 0, sizeof(Job));
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

void print_job(Job * job, int print_id) {
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
	default:
		state = strsignal(job->state);
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
	Job * current;
	Job * to_free;

	for (current = jobs_list.head; current;) {
		to_free = current;
		current = current->next_job;
		free(to_free->command);
		free(to_free);
	}
}

int add_job(Job * job) {
	int n_foregrounds = 0;
	int max_pos = 0;
	Job * current;

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

Job * get_job(pid_t job_pid) {
	Job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->pid == job_pid) {
			return current;
		}
	}
	return NULL;
}

int remove_job(Job * job) {
	Job * current;
	Job * previous = NULL;

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

int remove_all_status_jobs(int status) {
	Job * current;
	Job * previous = NULL;
	int add_relevance = 0;

	for (current = jobs_list.head; current; current = current->next_job) {
		if (status == ALL || current->state == status) {
			if (remove_job(current) < 0) {
				fprintf(stderr, "Mash: jobs failed to remove job\n");
				return 0;
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
	Job * current;
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
	Job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->relevance == relevance) {
			return current->pid;
		}
	}
	return -1;
}

int are_jobs_stopped() {
	int stopped_jobs = 0;
	Job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (current->state == STOPPED) {
			stopped_jobs++;
		}
	}
	return stopped_jobs;
}

void stop_job(pid_t job_pid) {
	Job * current_job = get_job(job_pid);
	current_job->state = STOPPED;
	printf("\n");
	print_job(current_job,0);
}

void end_current_job() {
	Job * current_job = get_job(get_relevance_job_pid(0));
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
			// perror("waitpid failed");
			return;
		}
		jobs_list.n_jobs--;
	}
	return;
}

void update_jobs() {
	// Check for finished jobs
	Job * current;
	for (current = jobs_list.head; current; current = current->next_job) {
		if (kill(current->pid,0) < 0) {
			current->state = DONE;
			print_job(current,0);
		}
	}
	remove_all_status_jobs(DONE);
}