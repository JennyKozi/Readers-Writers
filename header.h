#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/times.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define NAME_SIZE 20
#define ARRAY_SIZE 50

typedef struct {
	int customer_id;
	char last_name[NAME_SIZE];
	char first_name[NAME_SIZE];
	int	balance;
} Record;

typedef struct {
	pid_t readers_pid[ARRAY_SIZE];
	pid_t writers_pid[ARRAY_SIZE];
	int total_readers;
	int total_writers;
	int total_recs_processed;
	int readers_recs[ARRAY_SIZE][2];
	int writers_recs[ARRAY_SIZE];
	double time_readers[ARRAY_SIZE];
	double time_writers[ARRAY_SIZE];
	sem_t mutex;
	sem_t sem_sum;
	sem_t sem_new_reader;
	sem_t sem_new_writer;
	sem_t sem_readers_recs[ARRAY_SIZE];
	sem_t sem_writers_recs[ARRAY_SIZE];
} shared_mem_seg;

#define CHECK_CALL(call, error_value)                                                   \
	do {                                                                                \
		if ((call) == error_value) {                                                    \
			fprintf(stderr,                                                             \
				"Call %s failed with an error message: (%s) at file %s at line %d\n",   \
				#call, strerror(errno), __FILE__, __LINE__);                            \
			exit(EXIT_FAILURE);                                                         \
		}                                                                               \
	} while (0);

#define CHECK_SEM(retval)  \
	if (retval != 0) {  \
		perror("Couldn't initialize the semaphore!\n"); \
		exit(1); \
	}
