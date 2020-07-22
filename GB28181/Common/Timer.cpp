#include "Timer.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>


long Comm_SetTimer(unsigned long ulMilliSec, fnTimerCallBack fnCB)
{
	timer_t timerid;
	struct sigevent evp;

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = fnCB;
	act.sa_flags = 0;

	// XXX int sigaddset(sigset_t *set, int signum);  //将signum指定的信号加入set信号集
	// XXX int sigemptyset(sigset_t *set);			//初始化信号集
	
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGUSR1, &act, NULL) == -1)
	{
		perror("fail to sigaction\r\n");
		return -1;
	}

	memset(&evp, 0, sizeof(struct sigevent));
	evp.sigev_signo = SIGUSR1;
	evp.sigev_notify = SIGEV_SIGNAL;
	if (timer_create(CLOCK_REALTIME, &evp, &timerid) == -1)
	{
		perror("fail to timer_create\r\n");
		return -1;
	}

	struct itimerspec it;
	it.it_interval.tv_sec = (ulMilliSec / 1000);
	it.it_interval.tv_nsec = (ulMilliSec % 1000) * 1000 * 1000;
	it.it_value.tv_sec = (ulMilliSec / 1000);
	it.it_value.tv_nsec = (ulMilliSec % 1000) * 1000 * 1000;
	if (timer_settime(timerid, 0, &it, 0) == -1)
	{
		perror("fail to timer_settime\r\n");
		return -1;
	}

	long iTimerId = (long)timerid;
	return iTimerId;
}


int Comm_KillTimer(long lTimerId)
{
	timer_t tTimerid = (timer_t)lTimerId;
	timer_delete(tTimerid);
	return 0;
}







