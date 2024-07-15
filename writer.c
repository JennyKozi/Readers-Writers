#include "header.h"

void main (int argc, char *argv[]) {

	if (argc != 11) {
		perror("Wrong number of arguments!\n");
		exit(1);
	}

	// Variables to count time
	double t1, t2, realtime, tics_per_sec;
	struct tms tb1, tb2;
	tics_per_sec = (double)sysconf(_SC_CLK_TCK);
	t1 = (double)times(&tb1);

	int recid, shmid, value, new_balance, prev_balance, rp, retval, err, proc_index, num_recs = 1, last_reader, max_time;
	char *filename;
	shared_mem_seg *sh_mem;
	Record rec;
	int pid = getpid();

	// Get arguments from command line
	for (int i = 1; i < argc; i += 2) {
		
		// Datafile
		if (strcmp("-f", argv[i]) == 0) {
			filename = argv[i + 1];
		}

		// Record
		else if (strcmp("-l", argv[i]) == 0) {
			recid = atoi(argv[i + 1]);
		}

		// Value
		else if (strcmp("-v", argv[i]) == 0) {
			value = atoi(argv[i + 1]);
		}

		// Time
		else if (strcmp("-d", argv[i]) == 0) {
			max_time = atoi(argv[i + 1]);
		}

		// Shared Memory Segment
		else if (strcmp("-s", argv[i]) == 0) {
			shmid = atoi(argv[i + 1]);
		}
	}

	// Attach shared memory segment
	CHECK_CALL(sh_mem = shmat(shmid, (void *) 0, 0), (void *) -1);

	// Open file
	CHECK_CALL(rp = open(filename, O_RDWR), -1);

	// Set pointer to the right record (reach the int balance)
	lseek(rp, (recid - 1) * sizeof(Record), SEEK_SET);

	// Enter CS (Insert rec id in the array)
	sem_wait(&(sh_mem->mutex));
	proc_index = sh_mem->total_writers; // Very important: variable proc_index indicates the index for the array of records for the writers
	last_reader = sh_mem->total_readers; // The last reader that arrived before this process
	sh_mem->total_writers++; // Increase number of writers
	sh_mem->writers_recs[proc_index] = recid; // Insert the record id of this writer in the array

	// Enter CS (Write)
	sem_wait(&(sh_mem->sem_writers_recs[proc_index]));
	sem_post(&(sh_mem->mutex));
	// Exit CS (Insert rec id in the array)
	
	// Enter CS (New writer)
	sem_wait(&(sh_mem->sem_new_writer));
	sh_mem->writers_pid[proc_index] = pid; // Add writers's pid to the array of writers' pids
	sem_post(&(sh_mem->sem_new_writer));
	// Exit CS (New writer)

	// Enter CS (Search records)
	sem_wait(&(sh_mem->mutex));

	// There is one or more writers who write on this record
	for (int i = 0; i < proc_index; i++) {
		if (sh_mem->writers_recs[i] == recid) {
			int temp_pid = sh_mem->writers_pid[i];
			sem_post(&(sh_mem->mutex)); // Leave mutex, don't hold back the other processes
			printf("Writer %d: waiting for writer %d to write on record %d\n", pid, sh_mem->writers_pid[i], recid);
			sem_wait(&(sh_mem->sem_writers_recs[i])); // Writer is suspended until writers (who came before) finish
			printf("Writer %d: writer %d released record %d\n", pid, temp_pid, recid);
			sem_post(&(sh_mem->sem_writers_recs[i])); // Release resource
			sem_wait(&(sh_mem->mutex)); // Take back mutex so that you can keep searching
		}
	}
	// There is one or more readers who read on this record
	for (int i = 0; i < last_reader; i++) {
		// Reader has one record
		if (sh_mem->readers_recs[i][1] == 0) {
			if (sh_mem->readers_recs[i][0] == recid) {
				int temp_pid = sh_mem->readers_pid[i];
				sem_post(&(sh_mem->mutex)); // Leave mutex, don't hold back the other processes
				printf("Writer %d: waiting for reader %d to read record %d\n", pid, temp_pid, recid);
				sem_wait(&(sh_mem->sem_readers_recs[i])); // Writer is suspended until readers (who came before) finish
				printf("Writer %d: reader %d released record %d\n", pid, temp_pid, recid);
				sem_post(&(sh_mem->sem_readers_recs[i])); // Release resource
				sem_wait(&(sh_mem->mutex)); // Take back mutex so that you can keep searching
			}
		}
		// Reader has many records
		else if ((sh_mem->readers_recs[i][0] <= recid) && (sh_mem->readers_recs[i][1] >= recid)) {
			int temp_pid = sh_mem->readers_pid[i];
			sem_post(&(sh_mem->mutex)); // Leave mutex, don't hold back the other processes
			printf("Writer %d: waiting for reader %d to read record %d\n", pid, temp_pid, recid);
			sem_wait(&(sh_mem->sem_readers_recs[i])); // Writer is suspended until readers (who came before) finish
			printf("Writer %d: reader %d released record %d\n", pid, temp_pid, recid);
			sem_post(&(sh_mem->sem_readers_recs[i])); // Release resource
			sem_wait(&(sh_mem->mutex)); // Take back mutex so that you can keep searching
		}
	}
	sem_post(&(sh_mem->mutex));
	// Exit CS (Search records)

	read(rp, &rec, sizeof(Record)); // Read record to get the current balance
	prev_balance = rec.balance;
	new_balance = rec.balance + value;
	rec.balance += value;
	lseek(rp, (recid - 1) * sizeof(Record), SEEK_SET);
	sleep((rand() % max_time) + 1); // Sleep for some seconds while holding the record
	CHECK_CALL(write(rp, &rec, sizeof(Record)), -1);
	printf("Writer %d: record id: %d, previous balance: %d, new balance: %d\n", pid, recid, prev_balance, new_balance);

	// Enter CS (Remove id of this rec from the array)
	sem_wait(&(sh_mem->mutex));
	sh_mem->writers_recs[proc_index] = 0;
	sem_post(&(sh_mem->sem_writers_recs[proc_index]));
	// Exit CS (Write)

	sem_post(&(sh_mem->mutex));
	// Exit CS (Remove id of this rec from the array)

	// Close file
	CHECK_CALL(close(rp), -1);

	// Enter CS (Increase records processed)
	sem_wait(&(sh_mem->sem_sum));
	sh_mem->total_recs_processed += num_recs; // Increase number of records that have been processed
	sem_post(&(sh_mem->sem_sum));
	// Exit CS (Increase records processed)

	// Enter CS (Remove writers's pid from the array of writers' pids)
	sem_wait(&(sh_mem->sem_new_writer));
	sh_mem->writers_pid[proc_index] = 0;
	sem_post(&(sh_mem->sem_new_writer));
	// Exit CS (Remove writers's pid from the array of writers' pids)

	// Calculate time
	t2 = (double)times(&tb2);
	realtime = (double)((t2 - t1) / tics_per_sec);
	sh_mem->time_writers[proc_index] = realtime;

	// Detach shared memory segment
	CHECK_CALL(err = shmdt((void *) sh_mem), -1);

	exit(0);
}
