#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "status_display.h"
#include "PARKING.h"

#define SHARE_NAME "PARKING"
#define REVENUE_FILE "MONEY_FILE.txt"
#define GOTO_LINE1 "\033[u" /* use saved cursor location */


void get_entry(shm_t* shm){
	char* plate_num;
	char bg_status;
	char sign_display;

	/* save cursor location to use later */
	printf("\033[s");

	for(int i = 0; i < ENTRANCES; i++){
		/* lpr status */
		plate_num = shm->data->entrances[i].LPR.plate;

		/* gate status */
		bg_status = shm->data->entrances[i].gate.status;

		/* info screen */
		sign_display = shm->data->entrances[i].screen.display;

		/* print entrance */
		printf("\n***** ENTRANCE %d *****\n", i+1);
		printf("Entrance %d LPR status: %s\n", i+1, plate_num);
		printf("Entry %d Boomgate status: %c\n", i+1, bg_status);
		printf("Digital Sign %d Status: %c\n", i+1, sign_display);

	}
}

void get_exit(shm_t* shm){
	char* plate_num;
	char bg_status;

	/* go to entrance starting line */
	printf("%s", GOTO_LINE1);

	for(int i = 0; i < EXITS; i++){
		/* lpr status */
		plate_num = shm->data->exits[i].LPR.plate;

		/* gate status */
		bg_status = shm->data->exits[i].gate.status;

		/* print exit */
		printf("\n\t\t\t\t  ");
		printf("*****   EXIT %d  *****\n", i+1);
		printf("\t\t\t\t  ");
		printf("Exit %d LPR status: %s\n", i+1, plate_num);
		printf("\t\t\t\t  ");
		printf("Exit %d Boomgate status: %c\n\n", i+1, bg_status);
	}

}

void get_level(shm_t* shm){
	char* plate_num;
	int16_t temperature;
	int occup = 4; /* **** need to get real value *** */

	/* go to entrance starting line */
	printf("%s", GOTO_LINE1);

	for(int i = 0; i < LEVELS; i++){
		/* lpr status */
		plate_num = shm->data->levels[i].LPR.plate;

		/* temp number */
		temperature = shm->data->levels[i].temp;

		/* print level */
		printf("\n\t\t\t\t\t\t\t\t  ");
		printf("*****  LEVEL %d *****\n", i+1);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d LPR status: %s\n", i+1, plate_num);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Temperature Sensor %d Status: %d\n", i+1, temperature);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d Occupancy: %d of %d\n", i+1, occup, LEVEL_CAPACITY);
	}

}

double convertToNum(char a[]){
	double val;
	val = atof(a);
	return val;
}

void print_revenue(){
	/* read file */
	FILE *bill_file;
	bill_file = fopen(REVENUE_FILE, "r");

	/* Declare useful constants */
	char buff[BUFFER];
	char revenue_str[BUFFER];
	double bill_total = 0;

	/* Clear buff array */
	memset(buff, 0, BUFFER);

	/* read line by line until EOF */
	while (fgets(buff, sizeof(buff), bill_file)) {
		/* scan line and store relevant part as char */
		sscanf(buff, "%*s $%s", revenue_str);
		/* convert scan result to double and add to bill total */
		bill_total += convertToNum(revenue_str);

		/* Clear bill_string array */
		memset(revenue_str, 0, BUFFER);

	}
	/* print bill total rounded to 2 dp */
	printf("\nTotal Revenue: $%.2f\n",bill_total);
	/* close file */
	fclose(bill_file);
}

bool check_for_fail(char* check){
	/* check if string contains "fail" */
	if (strstr(check, "fail") != NULL){
		return true;
	}
	return false;
}

void *display(void *ptr){


		shm_t shm;

		int stdout_bk; /* is fd for stdout backup */
		int pipefd[2];
		pipe(pipefd); /* create pipe */
		char buf[1024];
		stdout_bk = dup(fileno(stdout));
		/* What used to be stdout will now go to the pipe. */
		dup2(pipefd[1], fileno(stdout));

		if ( get_shared_object( &shm, SHARE_NAME ) != true ) {
			printf("Shared memory connection failed.\n" );

		}

		int i = 0;
		while(i < 100){
			fflush(stdout);

	    close(pipefd[1]);

	    dup2(stdout_bk, fileno(stdout)); /* restore stdout */

	    read(pipefd[0], buf, 100);
			printf("%s\n", buf);

			/* if no fail print display otherwise break loop */
			if(check_for_fail(buf) != true){
				system("clear");

				get_entry(&shm);
				get_exit(&shm);
				get_level(&shm);
				print_revenue();

				usleep(50);
				i++;
			}
			else{
					break;

			}
		}
	return ptr;
}
