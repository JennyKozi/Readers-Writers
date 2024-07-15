#include "header.h"

void main (int argc, char *argv[]) {

	if (argc != 9) {
		perror("Wrong number of arguments!\n");
		exit(1);
	}

	// Variables to count time
	double t1, t2, realtime, tics_per_sec;
	struct tms tb1, tb2;
	tics_per_sec = (double)sysconf(_SC_CLK_TCK);
	t1 = (double)times(&tb1);

	int recid, recid_min, recid_max, shmid, rp, retval, err, proc_index, num_recs = 1, last_writer, max_time;
	char *filename, temp_string[NAME_SIZE];
	bool flag_many_records = false;
	shared_mem_seg *sh_mem;
	Record rec;
	int pid = getpid();

	// Get arguments from command line
	for (int i = 1; i < argc; i += 2) {
		
		// Datafile
		if (strcmp("-f", argv[i]) == 0) {
			filename = argv[i + 1];
		}

		// Record(s)
		else if (strcmp("-l", argv[i]) == 0) {

			int size = strlen(argv[i + 1]);
			strcpy(temp_string, argv[i + 1]);

			// Many records
			if (temp_string[0] == '[') {

				flag_many_records = true;
				char min_s[10], max_s[10];

				// Find first record
				int j = 1, count = 0;
				while (isdigit(temp_string[j])) {
					min_s[count] = temp_string[j];
					count++;
					j++;
				}
				min_s[count] = '\0';
				recid_min = atoi(min_s);

				// Find last record 
				count = 0;
				j++;
				while (isdigit(temp_string[j])) {
					max_s[count] = temp_string[j];
					count++;
					j++;
				}
				max_s[count] = '\0';
				recid_max = atoi(max_s);
				num_recs = recid_max - recid_min + 1;
			}
			// One record
			else {
				recid = atoi(argv[i + 1]);
			}
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
	CHECK_CALL(rp = open(filename, O_RDONLY), -1);

	// Set pointer to the right record
	if (flag_many_records)
		lseek(rp, (recid_min - 1) * sizeof(Record), SEEK_SET);
	else
		lseek(rp, (recid - 1) * sizeof(Record), SEEK_SET);

	// Enter CS (insert rec id in the array)
	sem_wait(&(sh_mem->mutex));
	proc_index = sh_mem->total_readers; // Very important: variable proc_index indicates the index for the array of records for the readers
	last_writer = sh_mem->total_writers; // The last writer that arrived before this process
	sh_mem->total_readers++; // Increase number of readers

	// Insert the record id of this reader in the array
	if (flag_many_records) {
		sh_mem->readers_recs[proc_index][0] = recid_min;
		sh_mem->readers_recs[proc_index][1] = recid_max;
	}
	else {
		sh_mem->readers_recs[proc_index][0] = recid;
	}

	// Enter CS (Read)
	sem_wait(&(sh_mem->sem_readers_recs[proc_index])); // Hold the semaphore for this record
	sem_post(&(sh_mem->mutex));
	// Exit CS (Insert rec id in the array)

	// Enter CS (New reader)
	sem_wait(&(sh_mem->sem_new_reader));
	sh_mem->readers_pid[proc_index] = pid; // Add readers's pid to the array of readers' pids
	sem_post(&(sh_mem->sem_new_reader));
	// Exit CS (New reader)

	// Enter CS (Search records)
	sem_wait(&(sh_mem->mutex));

	// There is one or more writers who write on this record
	for (int i = 0; i < last_writer; i++) {
		if (flag_many_records) {
			if ((sh_mem->writers_recs[i] >= recid_min) && (sh_mem->writers_recs[i] <= recid_max)) {
				int temp_rec = sh_mem->writers_recs[i];
				int temp_pid = sh_mem->writers_pid[i];
				sem_post(&(sh_mem->mutex)); // Leave mutex, don't hold back the other processes
				printf("Reader %d: waiting for writer %d to write on record %d\n", pid, temp_pid, temp_rec);
				sem_wait(&(sh_mem->sem_writers_recs[i])); // Reader is suspended until writers (who came before) finish
				printf("Reader %d: writer %d released record %d\n", pid, temp_pid, temp_rec);
				sem_post(&(sh_mem->sem_writers_recs[i])); // Release resource
				sem_wait(&(sh_mem->mutex)); // Take back mutex so that you can keep searching
			}
		}
		else if (sh_mem->writers_recs[i] == recid) {
			int temp_pid = sh_mem->writers_pid[i];
			sem_post(&(sh_mem->mutex)); // Leave mutex, don't hold back the other processes
			printf("Reader %d: waiting for writer %d to write on record %d\n", pid, temp_pid, recid);
			sem_wait(&(sh_mem->sem_writers_recs[i])); // Reader is suspended until writers (who came before) finish
			printf("Reader %d: writer %d released record %d\n", pid, temp_pid, recid);
			sem_post(&(sh_mem->sem_writers_recs[i])); // Release resource
			sem_wait(&(sh_mem->mutex)); // Take back mutex so that you can keep searching
		}
	}
	sem_post(&(sh_mem->mutex));
	// Exit CS (Search records)

	// Read many records
	if (flag_many_records) {
		int sum = 0;
		sleep(rand() % max_time); // Sleep for some seconds while holding the record
		for (int i = 0; i < num_recs; i++) {
			read(rp, &rec, sizeof(Record));
			sum += rec.balance;
			printf("Reader %d: Record %d %s %s %d\n", pid, rec.customer_id, rec.last_name, rec.first_name, rec.balance);
		}
		float average = (float)(sum / num_recs);
		printf("Reader %d: Average balance for records %d - %d: %f\n", pid, recid_min, recid_max, average);
	}
	// Read one record
	else {
		sleep((rand() % max_time) + 1); // Sleep for some seconds while holding the record
		read(rp, &rec, sizeof(Record));
		printf("Reader %d: Record %d %s %s %d\n", pid, rec.customer_id, rec.last_name, rec.first_name, rec.balance);
	}
	
	// Enter CS (Remove id of this rec from the array)
	sem_wait(&(sh_mem->mutex));
	sh_mem->readers_recs[proc_index][0] = 0;
	sh_mem->readers_recs[proc_index][1] = 0;
	sem_post(&(sh_mem->sem_readers_recs[proc_index]));
	// Exit CS (Read)

	sem_post(&(sh_mem->mutex));
	// Exit CS (Remove id of this rec from the array)

	// Close file
	CHECK_CALL(close(rp), -1);

	// Enter CS (increase records processed)
	sem_wait(&(sh_mem->sem_sum));
	sh_mem->total_recs_processed += num_recs; // Increase number of records that have been processed
	sem_post(&(sh_mem->sem_sum));
	// Exit CS (increase records processed)

	// Enter CS (Remove readers's pid from the array of readers' pids)
	sem_wait(&(sh_mem->sem_new_reader));
	sh_mem->readers_pid[proc_index] = 0;
	sem_post(&(sh_mem->sem_new_reader));
	// Exit CS (Remove readers's pid from the array of readers' pids)

	// Calculate time
	t2 = (double)times(&tb2);
	realtime = (double)((t2 - t1) / tics_per_sec);
	sh_mem->time_readers[proc_index] = realtime;

	// Detach shared memory segment
	CHECK_CALL(err = shmdt((void *) sh_mem), -1);

	exit(0);
}
