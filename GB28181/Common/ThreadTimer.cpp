#include "ThreadTimer.h"
#include "CommDefine.h"
#include <pthread.h>
#include <semaphore.h>

typedef struct tagSThreadTimerParam
{
	unsigned long ulElapse;
	fnThreadTimerCB fnCB;
	void *pUserData;
	sem_t semWaitExit;
	pthread_t tThreadId;
	bool bThreadWorking;
	unsigned long ulLastTick;
}SThreadTimerParam;

void* threadTimerElapse(void* args)
{
	SThreadTimerParam *pTimerParam = (SThreadTimerParam *)args;
	//定时器先执行一次
	pTimerParam->ulLastTick = Comm_GetTickCount();
	if (pTimerParam->fnCB)
	{
		pTimerParam->fnCB((HTIMER)pTimerParam, pTimerParam->pUserData);
	}
	
	while (pTimerParam->bThreadWorking)
	{
		unsigned long ulNowTick = Comm_GetTickCount();
		if (pTimerParam->ulLastTick > ulNowTick) /*由于tick值超过U32最大值，会出现上次tick值大于当前tick*/
		{
			pTimerParam->ulLastTick = ulNowTick;
		}

		U32 ulTickElapse = ulNowTick - pTimerParam->ulLastTick;
		if (ulTickElapse >= pTimerParam->ulElapse)
		{
			pTimerParam->ulLastTick = ulNowTick;
			if (pTimerParam->fnCB)
			{
				pTimerParam->fnCB((HTIMER)pTimerParam, pTimerParam->pUserData);
			}
		}
		
		usleep(50*1000); //ms
	}

	sem_post(&pTimerParam->semWaitExit);
	
	return 0;
}



//开始定时器
HTIMER Comm_CreateThreadTimer(unsigned long ulMilliSec, fnThreadTimerCB fn, void *pUserData)
{
	SThreadTimerParam *pTimerParam = new SThreadTimerParam;
	pTimerParam->ulElapse = ulMilliSec;
	pTimerParam->ulLastTick = 0;
	pTimerParam->fnCB = fn;
	pTimerParam->pUserData = pUserData;
	sem_init(&pTimerParam->semWaitExit, 0, 0);
	pTimerParam->bThreadWorking = true;
	pthread_create(&pTimerParam->tThreadId, 0, threadTimerElapse, pTimerParam);

	HTIMER hRet = (HTIMER)pTimerParam;
	return hRet;
}

//结束定时器
int Comm_ReleaseThreadTimer(HTIMER hTimer)
{	
	if (NULL == hTimer)
		return -1;
	
	SThreadTimerParam *pTimerParam = (SThreadTimerParam *)hTimer;
	//停止线程
	pTimerParam->bThreadWorking = false;
	sem_wait(&pTimerParam->semWaitExit);
	sem_destroy(&pTimerParam->semWaitExit);   
	//释放内存
	delete pTimerParam;

	return 0;
}




