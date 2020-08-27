/*
 * Small benchmark of Linux' pipe (part of an OS-Lab)
 * see man 2 pipe
 * 
 * Author: Nils Lübben, HS Esslingen
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/uio.h>
#include <math.h>
#include <wait.h>

#include "bench_utils.h"

#define READ  0
#define WRITE 1
#define PRETTY

int main(int argc, char * argv[]) {

    const int sizes[] = {64, 256, 1024, 4096, 16384, 65536,
						262144, 1048576, 4194304, 16777216};
    const int sizes_num = sizeof(sizes)/sizeof(sizes[0]);
#define MAX_SIZE sizes[sizes_num-1]
    int pdes[2];
	int ret;
    char * buffer;
    pid_t pid;
    pid_t pid_child;
    
    ret = pipe (pdes);
    if (ret < 0)
        ERROR("pipe", errno);

    pid = getpid();
    ret = pid_child = fork();
    if (ret < 0)
        ERROR("fork", errno);

    if (ret == 0) {
        /* CHILD Process */
		//int i, j;
		int nread;
        pid = getpid();
        DEBUG(printf("PID:%d (CHILD) starts\n",
                     (int) pid));

        // close the writing descriptor
        ret = close (pdes[WRITE]);

        buffer = malloc (MAX_SIZE);
        if (NULL == buffer)
            ERROR("malloc", ENOMEM);
        memset (buffer, 0, MAX_SIZE);
		
		/* bad read loop, is somehow out of sync with read byte-size
		 * for(i = 0; i < sizes_num; i++)
		 *	 for(j = 0; j < MEASUREMENTS; j++)
		 *		ret = read (pdes[READ], buffer, sizes[i]);
		 */

		//reads from parent process with max block size
		nread = 1;
		while (nread != 0){
			nread = read (pdes[READ], buffer, MAX_SIZE);
		}

		//close the reading descriptor
		ret = close (pdes[READ]);

        DEBUG(printf("PID:%d (CHILD) exits\n",
                     (int) pid));
        exit (EXIT_SUCCESS);
    }

	/* PARENT Pocess */
    int * ticks;
    int i;

    DEBUG(printf("PID:%d (PARENT) starts child_pid:%d\n",
                 (int) pid, (int)pid_child));
    // close the reading descriptor
    ret = close (pdes[READ]);

    DEBUG(printf("PID:%d (PARENT) remote_buffer:%p\n",
                 (int) pid, remote_buffer));

    buffer = malloc (MAX_SIZE);
    if (NULL == buffer)
        ERROR("malloc", ENOMEM);
	memset (buffer, 'a', MAX_SIZE);
        
    ticks = malloc (MEASUREMENTS * sizeof (int));
    if (NULL == ticks)
        ERROR("malloc", ENOMEM);
    memset (ticks, 0, MEASUREMENTS * sizeof(int));

    for (i = 0; i < sizes_num; i++) {
        int current_size = sizes[i];
        int nwrite;
        int j;
        int min_ticks;
        int max_ticks;
        long long ticks_all;
        struct timeval tv_start;
        struct timeval tv_stop;
        double time_delta_sec;

		assert (current_size <= MAX_SIZE);
        
		gettimeofday (&tv_start, NULL);
        for (j = 0; j < MEASUREMENTS; j++) {
            unsigned long long start;
            unsigned long long stop;
            start = getrdtsc();
			nwrite = write (pdes[WRITE], buffer, current_size);
            stop = getrdtsc();
            assert (nwrite == current_size);
            ticks[j] = stop - start;
        }
        gettimeofday (&tv_stop, NULL);

        min_ticks = INT_MAX;
        max_ticks = INT_MIN;
        ticks_all = 0;
        for (j = 0; j < MEASUREMENTS; j++) {
            if (min_ticks > ticks[j]) min_ticks = ticks[j];
            if (max_ticks < ticks[j]) max_ticks = ticks[j];
            ticks_all += ticks[j];
        }
        ticks_all -= min_ticks;
        ticks_all -= max_ticks;
		
		//calculate and convert time [s]
        time_delta_sec = ((tv_stop.tv_sec - tv_start.tv_sec) + ((tv_stop.tv_usec - tv_start.tv_usec) / (1000.0 * 1000.0)));

#ifndef PRETTY
        printf ("PID:%d time:%fs min:%d max:%d Avg ticks without min/max:%f Ticks (for %d measurements) for %d Bytes (%.2f MB/s)\n",
        		pid, time_delta_sec, min_ticks, max_ticks,
        		(double) ticks_all/(MEASUREMENTS - 2.0), MEASUREMENTS ,nwrite,
        		((double) current_size*MEASUREMENTS)/(1024.0*1024.0*time_delta_sec));	
#else	
		//alligned print
        printf ("PID: %d time: %9.4fs\tmin: %-10d max: %-10d\nTicks Avg ticks without min/max: %12.2f Ticks (for %d measurements) for %*d Bytes (%.2f MB/s)\n",
				pid, time_delta_sec, min_ticks, max_ticks,
        		(double) ticks_all/(MEASUREMENTS - 2.0), MEASUREMENTS,
				(int) (log(MAX_SIZE)/log(10)) + 1 ,nwrite,
        		((double) current_size*MEASUREMENTS)/(1024.0*1024.0*time_delta_sec));
		printf("===================================================================================================================\n");
#endif
    }
	
	//close the writing descriptor
	close (pdes[WRITE]);
	int status = 0;

	//join with child
	waitpid(pid_child, &status, 0);
    return EXIT_SUCCESS;
}
