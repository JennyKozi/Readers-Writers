#include "header.h"

#define NUM_ARGS_READER 10
#define NUM_ARGS_WRITER 12
# define SEGMENTPERM 0666

int main(int argc, char *argv[]) {

	if (argc != 2) {
		perror("Wrong number of arguments!\n");
		exit(1);
	}

	int count = 0, status, retval, shmid, err;
	char *exec_file, *reader_args[NUM_ARGS_READER], *writer_args[NUM_ARGS_WRITER], temp_string[NAME_SIZE];
	pid_t child;
	FILE *fp;
	shared_mem_seg *sh_mem;

	srand(time(NULL));

	// Create shared memory segment
	CHECK_CALL(shmid = shmget(IPC_PRIVATE, sizeof(shared_mem_seg), SEGMENTPERM), -1);
	printf("Allocated memory segment %d\n\n", shmid);

	// Attach the segment
	CHECK_CALL(sh_mem = shmat(shmid, (void *) 0, 0), (void *) -1);

	// Initialize data of shared memory segment
	sh_mem->total_readers = 0;
	sh_mem->total_writers = 0;
	sh_mem->total_recs_processed = 0;

	// Initialize the arrays
	for (int i = 0; i < ARRAY_SIZE; i++) {
		sh_mem->readers_pid[i] = 0;
		sh_mem->writers_pid[i] = 0;
		sh_mem->readers_recs[i][0] = 0;
		sh_mem->readers_recs[i][1] = 0;
		sh_mem->writers_recs[i] = 0;
		sh_mem->time_readers[i] = 0;
		sh_mem->time_writers[i] = 0;
	}

	// Initialize the semaphores
	CHECK_SEM(sem_init(&(sh_mem->mutex), 1, 1));
	CHECK_SEM(sem_init(&(sh_mem->sem_sum), 1, 1));
	CHECK_SEM(sem_init(&(sh_mem->sem_new_reader), 1, 1));
	CHECK_SEM(sem_init(&(sh_mem->sem_new_writer), 1, 1));
	for (int i = 0; i < ARRAY_SIZE; i++) {
		CHECK_SEM(sem_init(&(sh_mem->sem_readers_recs[i]), 1, 1));
		CHECK_SEM(sem_init(&(sh_mem->sem_writers_recs[i]), 1, 1));
	}

	// Allocate memory
	for (int i = 0; i < NUM_ARGS_READER - 1; i++) {
		CHECK_CALL(reader_args[i] = malloc(NAME_SIZE * sizeof(char)), NULL);
	}
	for (int i = 0; i < NUM_ARGS_WRITER - 1; i++) {
		CHECK_CALL(writer_args[i] = malloc(NAME_SIZE * sizeof(char)), NULL);
	}

	// Open exec file
	exec_file = argv[1];
	CHECK_CALL(fp = fopen(exec_file, "r"), NULL);

	// Read exec file
	while (fscanf(fp, "%s", temp_string) == 1) { // Scan program (reader / writer)

		// Exec line to run a reader program
		if (strcmp("./reader", temp_string) == 0) {

			strcpy(reader_args[0], temp_string);
			for (int i = 1; i <= 6; i++) { // 1) "-f" 2) datafile 3) "-l" 4) recid 5) "-d" 6) time
				fscanf(fp, "%s", reader_args[i]);
			}
			strcpy(reader_args[7], "-s");
			snprintf(reader_args[8], sizeof(reader_args[8]), "%d", shmid);
			reader_args[9] = NULL;

			// Create a new process that will become a reader
			child = fork();
			if (child == -1) {
				perror("Failed to fork!\n");
				exit(1);
			}

			// Child -> reader
			if (child == 0) {
				execvp("./reader", reader_args);
				perror("exec failure!\n");
				exit(1);
			}
		}

		// Exec line to run a writer program
		else {
			strcpy(writer_args[0], temp_string);

			for (int i = 1; i <= 8; i++) { // 1) "-f" 2) datafile 3) "-l" 4) recid 5) "-v" 6) value 7) "-d" 8) time
				fscanf(fp, "%s", writer_args[i]);
			}
			strcpy(writer_args[9], "-s");
			snprintf(writer_args[10], sizeof(writer_args[10]), "%d", shmid);
			writer_args[11] = NULL;

			// Create a new process that will become a writer
			child = fork();
			if (child == -1) {
				perror("Failed to fork!\n");
				exit(1);
			}

			// Child -> writer
			if (child == 0) {
				execvp("./writer", writer_args);
				perror("exec failure!\n");
				exit(1);
			}
		}
		count++;
	}
	CHECK_CALL(fclose(fp), EOF); // Close exec file

	// Wait for readers and writers to finish
	for (int i = 0; i < count; i++) {
		wait(&status);
	}

	// Calculate statistics
	double average_time_readers = 0, average_time_writers = 0, max_time = 0.0, sum_readers = 0.0, sum_writers = 0.0;

	// Readers
	for (int i = 0; i < sh_mem->total_readers; i++) {
		sum_readers += sh_mem->time_readers[i];
		if (max_time < sh_mem->time_readers[i])
			max_time = sh_mem->time_readers[i];
	}
	average_time_readers = (double)(sum_readers / sh_mem->total_readers);

	// Writers
	for (int i = 0; i < sh_mem->total_writers; i++) {
		sum_writers += sh_mem->time_writers[i];
		if (max_time < sh_mem->time_writers[i])
			max_time = sh_mem->time_writers[i];
	}
	average_time_writers = (double)(sum_writers / sh_mem->total_writers);

	// Print statistics
	printf("\nSTATISTICS:\nTotal readers: %d\n", sh_mem->total_readers);
	printf("Total writers: %d\n", sh_mem->total_writers);
	printf("Total records processed: %d\n", sh_mem->total_recs_processed);
	printf("Readers Average Time: %f\n", average_time_readers);
	printf("Writers Average Time: %f\n", average_time_writers);
	printf("Maximum time for a process: %f\n", max_time);

	// Destroy semaphores
	sem_destroy(&(sh_mem->sem_new_reader));
	sem_destroy(&(sh_mem->sem_new_writer));
	sem_destroy(&(sh_mem->sem_sum));
	sem_destroy(&(sh_mem->mutex));
	for (int i = 0; i < ARRAY_SIZE; i++) {
		sem_destroy(&(sh_mem->sem_readers_recs[i]));
		sem_destroy(&(sh_mem->sem_writers_recs[i]));
	}

	// Destroy shared memory segment
	CHECK_CALL(err = shmctl(shmid, IPC_RMID, 0), -1);
	printf("\nRemoved shared memory segment %d\n\n", shmid);

	// Free memory
	for (int i = 0; i < NUM_ARGS_READER - 1; i++) {
		free(reader_args[i]);
	}
	for (int i = 0; i < NUM_ARGS_WRITER - 1; i++) {
		free(writer_args[i]);
	}
	
    return 0;
}
