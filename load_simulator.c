/****************************************************************************
 * The program at a high level spawns a set of
 * processes and threads. These 'tasks' perform
 * certain activities which increases the "anon",
 * and "file" pages in the system. What has to be
 * increased, and by what level can be decided by
 * the user when invoking the program.
 *
 * The program was initially written to:
 * 1) test and tune the android lowmemory killer.
 * 2) test zram and to tune its size.
 * 3) test the behaviour of Linux scheduler with
 *    different CPU loads.
 *
 * The user definable parameters are:
 *
 * 1) The number of processes.
 * 2) The number of threads.
 * 3) Anon pages
 * 4) File pages
 * 5) The kind of test.
 *
 * we may need to run the program with different
 * oom_score_adj depending on what kind of test is
 * performed. May be invoking the program with a
 * script of tis form.
 *
 * busybox chmod 777 /data/load_simulator
 * /data/load_simulator &
 * PID=$(busybox pgrep -f /data/load_simulator)
 * echo 1000 > /proc/$PID/oom_score_adj
 *
 * Examples:
 *
 * 1) To test the ALMK by creating 3 groups of
 *    threads, each thread of each group taking
 *    up anon pages 1000, 2000, and 3000, and file
 *    pages 500, 600 and 700. And we
 *    want to run these 3 groups of threads with
 *    different oom_score_adj. Thread group 1 with
 *    50 threads, 2 with 100 and 3 with 300.
 *
 *    Steps:
 *    a) Run the program with arguments,
 *    	 -o 1 -p 1 -t 50 -a 1000 -f 500. And set the oom_score_adj
 *    	 'X' using the script explained above.
 *
 *    	 o -> The type of test, 0 is TASKS_FOR_ALMK
 *    	 p -> Number of processes.
 *    	 t -> Number of threads.
 *    	 a -> anon pages to be consumed by each thread of the group.
 *    	 f -> file pages to be consumed by each thread of the group.
 *    b) Similarly invoke the program again to create
 *       another instance of it, with arguments to
 *       match that of the second thread group.
 *
 *  2) Reduce the free memory in the system by spawning
 *     a set of processes and threads.
 *
 *     Steps:
 *     b) Run the program with arguments,
 *        -o 0 -p 5 -t 100 -a 2000
 *
 *     Different system behaviours can be created by
 *     changing the number of processes and threads.
 * Author: Vinayak.Menon <vinayakm.list@gmail.com>
 * Version: 1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

int test_type = -1;
int num_proc = -1;
int num_threads = -1;
int anon_pages = -1;
int file_pages = -1;
int ion_pages = -1;

/*
 * REDUCE_FREE -Reduces the free memory by allocating the
 * 		memory by an amount as per user input. The
 * 		memory will be allocated as per user input.
 * 		Make sure the oom_score_adj of the task is
 * 		negative or 0, to make sure ALMK doesnt kill
 * 		it. The user can input the number of processes
 * 		and threads in each.
 * TASKS_FOR_ALMK -Spawns n and m number of processes and
 * 		   threads as specified by user. The adj val can
 * 		   be set from the invoking script.
 * SCHED - Spwans 3 processes and n threads as
 *         specified by the user. The first group does malloc
 *         and free in a loop, second does sleep in a loop, and
 *         third a busy loop.
 */
enum test {
	REDUCE_FREE,
	TASKS_FOR_ALMK,
	SCHED,
};

void *malloc_thread(void *arg)
{
	void *p;
	if (anon_pages > 0) {
		p = malloc(anon_pages * 4 * 1024);
		/* write to get pages allocated */
		memset(p, 0xffffffff, anon_pages * 4 * 1024);
		/* We dont have to cleanup anything */
	}

	while(1)
		sleep(2);
}

void *busy_thread(void *arg)
{
	for (;;);
}

void *sleep_thread(void *arg)
{
	for (;;)
		sleep(1);
}

void *almk_thread(void *arg)
{
	int fd, i;
	int ion_fd;
	unsigned int data = 0xFFFFFFFF;
	unsigned int n_to_write;
	void* p;
	char *temp_name;

	if (anon_pages > 0) {
		p = malloc(anon_pages * 4 * 1024);
		/* write to get pages allocated */
		memset(p, 0xffffffff, anon_pages * 4 * 1024);
		/* We dont have to cleanup anything */
	}

	if (file_pages > 0) {
		n_to_write = (file_pages * 4 * 1024);
		temp_name = tempnam("/data/", "load_simulate");
		if (!temp_name) {
			printf("Failed to create a unique filename\n");
		} else {
			fd = open(temp_name, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
			if (!fd) {
				printf("Failed to open file\n");
			} else {
				for (i = 0; i < (n_to_write/sizeof(unsigned int)); i++) {
					if (sizeof(unsigned int) != write(fd, &data, sizeof(unsigned int))) {
						printf("Failed to write to file\n");
						break;
					}
				}
			}
		}
	}

	for(;;)
		sleep(2);
}

void call_reduce_free(void)
{
	int i, j;
	pid_t pid;
	pthread_t x;

	if (num_threads > 1) {
		for (j = 0; j < (num_threads - 1); j++) {
			pthread_create(&x, NULL, malloc_thread, NULL);
		}
	}

	if (num_proc < 2)
		goto end;

	for (i = 0; i < (num_proc - 1); i++) {
		pid = fork();
		if (!pid) {
			pthread_t x;
			if (num_threads > 1) {
				for (j = 0; j < (num_threads - 1); j++) {
					pthread_create(&x, NULL, malloc_thread, NULL);
				}
			}
			while(1)
				sleep(2);
		}

	}
end:
	while(1)
		sleep(2);
}

void call_tasks_for_almk(void)
{
	int i, j;
	pid_t pid;
	pthread_t x;

	if (num_threads > 1) {
		for (j = 0; j < (num_threads - 1); j++) {
			pthread_create(&x, NULL, almk_thread, NULL);
		}
	}

	if (num_proc < 2)
		goto end;

	for (i = 0; i < (num_proc - 1); i++) {
		pid = fork();
		if (!pid) {
			pthread_t x;
			if (num_threads > 1) {
				for (j = 0; j < (num_threads - 1); j++) {
					pthread_create(&x, NULL, almk_thread, NULL);
				}
			}
			while(1)
				sleep(2);
		}

	}
end:
	while(1)
		sleep(2);
}

void call_sched(void)
{
	int i, j;
	pid_t pid;

	if (num_proc < 1)
		num_proc = 1;
	if (num_threads < 0)
		num_threads = 1;

	setpriority(PRIO_PROCESS, 0, -8);

	for (i = 0; i < num_proc; i++) {
		pid = fork();
		if (!pid) {
			pthread_t x;
			setpriority(PRIO_PROCESS, 0, -8);
			for (j = 0; j < num_threads; j++) {
				pthread_create(&x, NULL, malloc_thread, NULL);
			}
		}
	}

	for (i = 0; i < num_proc; i++) {
		pid = fork();
		if (!pid) {
			pthread_t x;
			setpriority(PRIO_PROCESS, 0, 0);
			for (j = 0; j < num_threads; j++) {
				pthread_create(&x, NULL, sleep_thread, NULL);
			}
		}
	}

	for (i = 0; i < num_proc; i++) {
		pid = fork();
		if (!pid) {
			pthread_t x;
			setpriority(PRIO_PROCESS, 0, 0);
			for (j = 0; j < num_threads; j++) {
				pthread_create(&x, NULL, busy_thread, NULL);
			}
		}
	}

	while(1)
		sleep(2);
}

int main(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, ":o:p:t:a:f:i:")) != -1) {
		switch(c) {
			case 'o':
				test_type = atoi(optarg);
				break;
			case 'p':
				num_proc = atoi(optarg);
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'a':
				anon_pages = atoi(optarg);
				break;
			case 'f':
				file_pages = atoi(optarg);
				break;
			case 'i':
				ion_pages = atoi(optarg);
				break;
			case '?':
				printf("Unknown option\n");
				exit(2);
		}
	}

	/* Give time for external invokers
	 * to change any parameters.
	 */
	sleep(3);

	if (test_type == REDUCE_FREE) {
		call_reduce_free();
	} else if (test_type == TASKS_FOR_ALMK) {
		call_tasks_for_almk();
	} else if (test_type == SCHED) {
		call_sched();
	} else {
		printf("Wrong test type\n");
		exit(0);
	}
	return 0;
}
