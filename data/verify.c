// Author: A. Delis ad@di.uoa.gr
#include <stdio.h>
#include <unistd.h>

#define SIZE_NAME 20
#define SIZE_POST 6

typedef struct {
	int custid;
	char LastName[SIZE_NAME];
	char FirstName[SIZE_NAME];
	char postcode[SIZE_POST];
} Record;

int main (int argc, char** argv) {

	FILE *fpb;
	Record rec;
	long lSize;
	int numOfrecords, i;

	if (argc != 2) {
		printf("Correct syntax is: %s BinaryFile\n", argv[0]);
		return(1);
	}
	fpb = fopen(argv[1], "rb");
	if (fpb == NULL) {
		printf("Cannot open binary file\n");
		return 1;
	}

	// check number of records
	fseek(fpb, 0, SEEK_END);
	lSize = ftell(fpb);
	rewind(fpb);
	numOfrecords = (int)lSize / sizeof(rec);

	printf("\nRecords found in file: %d\n\n", numOfrecords);

	for (i = 0; i < numOfrecords; i++) {
		fread(&rec, sizeof(rec), 1, fpb);
		printf("%d %s %s %s \n", rec.custid, rec.LastName, rec.FirstName, rec.postcode);
	}
	printf("\n");
	fclose(fpb);

	return 0;
}
