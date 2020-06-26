/*
   John Karasev
   CS 360 Systems Programming
   WSUV Spring 2018
   -----------------------------------------------------
   Assignment #6:
   Simulate philosphers diner using semophors from the unix api.
*/


#define _SVID_SOURCE
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

// for eating times
#define STDDEV_EAT 3
#define MEAN_EAT 9

// for thinking times
#define STDDEV_THNK 7
#define MEAN_THNK 11



static int semid;
static int shmid;

// This is needed for the semctl operation.
union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO
				(Linux-specific) */
};

// keeps track of philosphers info
typedef struct metaPhil {
	int thinktime;
	int eattime;
	int cycles;
	bool isAlive;
	pid_t id;
} pmeta;


//release mem, if error print strerror and exit with 1.
void release_sem() {
	if( semctl( semid, -1, IPC_RMID ) < 0 ) {
		fprintf( stderr, "Error on semctl: %s\n", strerror( errno ) );
		exit( 1 );
	}
}

//if ctrl-c hit, release semaphores then exit
void handler( int code ) {
	release_sem();
	exit(0);
}

//prints results from the philosphers diner at the end
void printMeta( pmeta *meta ) {
	printf("\n\n");
	for ( int i = 0; i < 5; i++ )
		printf("philospher %d thinktime=%d eattime=%d cycles=%d pid=%d\n",
			i,  meta[i].thinktime, meta[i].eattime, meta[i].cycles, meta[i].id);
}

// This functions just returns ptr to memory that is
// associated with shmid global var.
pmeta *attach() {
	void* ptr;
	if ( ( ptr = shmat( shmid, NULL, 0 )) < (void*)0) {
		fprintf( stderr, "Error on shmat: %s\n", strerror( errno ) );
		exit(0);
	}
	return ( pmeta * )ptr;
}

// creates and returns ptr to shared memory of size size.
void *getshmptr( int size ) {
	void *ptr;
	if ( ( shmid = shmget( IPC_PRIVATE, size, ( SHM_R | SHM_W ) ) ) < 0 ) {
		fprintf( stderr, "Error on shmget: %s\n", strerror( errno ) );
		exit(1);
	}

	ptr = attach();

	if ( shmctl(shmid, IPC_RMID, ( struct shmid_ds * ) NULL ) < 0 ) {
		fprintf( stderr, "Error on shmctl: %s\n", strerror( errno ) );
		exit(0);
	}
	return ptr;
}

int randomGaussian( int mean, int stddev ) {
	double mu = 0.5 + ( double ) mean;
	double sigma = fabs( ( double ) stddev );
	double f1 = sqrt( -2.0 * log( ( double ) rand() / ( double ) RAND_MAX ) );
	double f2 = 2.0 * 3.14159265359 * ( double ) rand() / ( double ) RAND_MAX;
	if ( rand() & ( 1 << 5 ) )
		return ( int ) floor( mu + sigma * cos( f2 ) * f1 );
	else
		return ( int ) floor( mu + sigma * sin( f2 ) * f1 );
}

// subroutine where each child lives in.
void lifeOfPi( int id, pmeta *meta ) {

	signal(SIGINT, SIG_DFL);  // set to defualt to avoid releasing memory after it is
				  // released by parent process.

	struct sembuf sops[2];
	//assign the correct chopsticks to each philospher.
	sops[0].sem_num = ( id + 4 ) % 5;
	sops[1].sem_num = id;
	sops[0].sem_flg = sops[1].sem_flg = 0;

	//loop until philospher ate for 100 seconds.
	for ( meta->eattime = meta->thinktime = meta->cycles =  0; meta->eattime < 100; meta->cycles++ ) {
		int eatTime;
		int thinkTime;
		//request two chopsticks;
		sops[0].sem_op = sops[1].sem_op = -1;

		// wait for the two chopsticks.
		if ( semop( semid, sops, 2 ) < 0 ) {
			fprintf( stderr, "Error on semop philID->%d : %s\n", id, strerror( errno ) );
			exit( 1 );
		}

		// get randomGaussian number for eating.
		eatTime = abs( randomGaussian(  MEAN_EAT, STDDEV_EAT ) );
		printf( "philID: %d eattime: %d, (total eat:%d)\n", id, eatTime, meta->eattime );
		sleep( eatTime );
		meta->eattime += eatTime;

		// set both chopsticks to one representing release of
		// chopsticks.
		sops[0].sem_op = sops[1].sem_op = 1;

		//put chopsticks back on the table.
		if ( semop( semid, sops, 2 ) < 0 ) {
			fprintf( stderr, "Error on semop philID->%d : %s\n", id, strerror( errno ) );
			exit( 1 );
		}

		// After philospher is satisfied, he thinks for a random time.
		thinkTime = abs( randomGaussian( MEAN_THNK, STDDEV_THNK ) );
		printf( "philID: %d thinktime: %d (total think: %d)\n", id, thinkTime, meta->thinktime );
		meta->thinktime += thinkTime;
		sleep( thinkTime );

	}
	// after philosher has eat for 100 seconds, he gets killed.
	printf( "philospher %d is dead (process %d)\n", id, meta->id );

	exit( 0 );
}

int main() {

	//if program exits unexpectly, release semaphores then exit.
	signal( SIGINT, handler );

	// needed for a semctl operation
	union semun r;
	r.val = 1;

	//get 5 semaphores representing the chopsticks.
	semid = semget( IPC_PRIVATE, 5, S_IRUSR | S_IWUSR );
	if( semid < 0 ) {
		fprintf( stderr, "Error on semget: %s\n", strerror( errno ) );
		exit( 1 );
	}

	int philID = 0;
	// intially set all flags to 1 representing that
	// all chopsticks are available.
	for ( ; philID < 5; philID++ ) {
		if( semctl( semid, philID, SETVAL, r ) < 0 ) {
			fprintf( stderr, "Error on semctl: %s\n", strerror( errno ) );
			exit( 1 );
		}
	}

	// borrow shared memory to keep track of philospher metadata.
	pmeta *sm = ( pmeta * ) getshmptr( sizeof(pmeta) * 5 );

	// create a process for each philospher seeding a different number for
	// each one.
	for( philID = 0 ; philID < 5; philID++ ) {
		pid_t pid;
		srand( philID + 1 );
		if ( !( pid = fork() ) ) lifeOfPi( philID, &(sm[philID]) );
		sm[philID].id = pid; // set pid for each child in shared mem.

	}

	// Wait until all philosphers are killed.
	for( philID = 0 ; philID < 5; philID++ )
		wait( NULL );

	// print the results from philosphers diner
	printMeta(sm);

	printf( "\n\nAll philosphers were successfully assassinated\n\n" );

	//release memory.
	release_sem();

	exit( 0 );

}
