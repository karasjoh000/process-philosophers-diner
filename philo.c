/*
   John Karasev
   CS 360 Systems Programming
   WSUV Spring 2018
   -----------------------------------------------------
   Assignment #6:
   Simulate philosphers diner using semophors from the unix api.
*/


#define _SVID_SOURCE
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

// for eating times
#define STDDEV_EAT 3
#define MEAN_EAT 9

// for thinking times
#define STDDEV_THNK 7
#define MEAN_THNK 11



static int shmid;

// This is needed for the semctl operation.
union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO
				(Linux-specific) */
};



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

void lifeOfPi( int id ) {
	struct sembuf sops[2];
	//assign the correct chopsticks to each philospher.
	sops[0].sem_num = ( id + 4 ) % 5;
	sops[1].sem_num = id;
	sops[0].sem_flg = sops[1].sem_flg = 0;
	int totEatTime;
	//loop until philospher ate for 100 seconds.
	for ( totEatTime = 0; totEatTime < 100; ) {
		int eatTime;
		int thinkTime;
		//request two chopsticks;
		sops[0].sem_op = sops[1].sem_op = -1;

		// wait for the two chopsticks.
		if ( semop( shmid, sops, 2 ) < 0 ) {
			fprintf( stderr, "Error on semop philID->%d : %s\n", id, strerror( errno ) );
			exit( 1 );
		}

		// get randomGaussian number for eating.
		eatTime = abs( randomGaussian(  MEAN_EAT, STDDEV_EAT ) );
		printf( "philID: %d eattime: %d, toteat:%d\n", id, eatTime, totEatTime );
		sleep( eatTime );
		totEatTime += eatTime;

		// set both chopsticks to one representing release of
		// chopsticks.
		sops[0].sem_op = sops[1].sem_op = 1;

		//put chopsticks back on the table.
		if ( semop( shmid, sops, 2 ) < 0 ) {
			fprintf( stderr, "Error on semop philID->%d : %s\n", id, strerror( errno ) );
			exit( 1 );
		}

		// After philospher is satisfied, he thinks for a random time.
		thinkTime = abs( randomGaussian( MEAN_THNK, STDDEV_THNK ) );
		printf( "philID: %d thinktime: %d\n", id, thinkTime );
		sleep( thinkTime );

	}
	// after philosher has eat for 100 seconds, he gets killed.
	printf( "philospher %d is dead\n", id );

	exit( 0 );
}

int main() {

	union semun r;
	r.val = 1;

	//get 5 semaphores representing the chopsticks.
	shmid = semget( IPC_PRIVATE, 5, S_IRUSR | S_IWUSR );

	if( shmid < 0 ) {
		fprintf( stderr, "Error on semget: %s\n", strerror( errno ) );
		exit( 1 );
	}

	int philID = 0;
	// intially set all flags to 1 representing that
	// all chopsticks are available.
	for ( ; philID < 5; philID++ ) {
		if( semctl( shmid, philID, SETVAL, r ) < 0 ) {
			fprintf( stderr, "Error on semctl: %s\n", strerror( errno ) );
			exit( 1 );
		}
	}

	// create a process for each philospher seeding a different number for
	// each one.
	for( philID = 0 ; philID < 5; philID++ ) {
		srand( philID );
		if ( ! fork() ) lifeOfPi( philID );
	}

	// Wait until all philosphers are killed.
	for( philID = 0 ; philID < 5; philID++ )
		wait( NULL );

	printf( "\n\nAll philosphers were successfully assassinated\n\n" );

	//release memory.
	if( semctl( shmid, -1, IPC_RMID ) < 0 ) {
		fprintf( stderr, "Error on semctl: %s\n", strerror( errno ) );
		exit( 1 );
	}

	exit( 0 );

}
