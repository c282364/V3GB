#include "CommDefine.h"
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

/*获取系统开机以来的毫秒数*/
U32 Comm_GetTickCount()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/*获取1970年至当前时间的毫秒数*/
long long Comm_GetMilliSecFrom1970()
{
	long long i64MilliSec = 0;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	i64MilliSec = (long long)(tv.tv_sec * 1000);
	i64MilliSec += tv.tv_usec / 1000;
	return i64MilliSec;
}


long Comm_FileSize(FILE *f)
{
	long curPos = 0;
	long length = 0;
	curPos = ftell(f);
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, curPos, SEEK_SET);
	return length;
}

/*获取当前的系统时间*/
void Comm_GetNowTime(SSystemTime *pTime)
{
	struct timeval tv;
    struct timezone tz;
    struct tm *p;
    gettimeofday(&tv, &tz);
    p = localtime(&tv.tv_sec);
    int iMilicSec = (int)(tv.tv_usec/1000);

    pTime->usYear = 1900+p->tm_year;
	pTime->usMonth = 1+p->tm_mon;
	pTime->usDay = p->tm_mday;
    pTime->usHour = p->tm_hour;
	pTime->usMinute = p->tm_min;
	pTime->usSecond = p->tm_sec;
	pTime->usMilliSec = iMilicSec;
}


/*从字符串中得到key=val的val值*/
int Comm_GetKeyValue(char *pszInStr, const char *pszKey, char *pszVal, int iValMaxSize)
{
	if (NULL == pszInStr || NULL == pszKey || NULL == pszVal)
	{
		return -1;
	}
	
	char szKeyName[1024] = {0};
	int iNameLen = sprintf(szKeyName, "%s=", pszKey);
	char *pKeyPos = strstr(pszInStr, szKeyName);
	if (NULL == pKeyPos)
	{
		return -1;
	}

	char *pValPos = pKeyPos + iNameLen;
	int iValLen = 0;
	while (*pValPos)
	{
		if ('&' == *pValPos || '\r' == *pValPos || '\n' == *pValPos)
			break;

		if (iValLen >= iValMaxSize)
		{
			return -1;
		}

		pszVal[iValLen] = *pValPos;
		iValLen++;
		pValPos++;
	}
	
	return iValLen;
}


/*字符串时间2018-11-13 12:05:00转换成unix时间，单位秒*/
long long Comm_StrTime2UnixTime(const char *pszStrTime)
{
	if (NULL == pszStrTime)
	{
		return 0;
	}

	int iYear = 0;
	int iMonth = 0;
	int iDay = 0;
	int iHour = 0;
	int iMinute = 0;
	int iSecond = 0;
	sscanf(pszStrTime, "%d-%d-%d %d:%d:%d", &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond);	

	struct tm tm_time;
	long long unix_time;

	tm_time.tm_year = iYear;
	tm_time.tm_mon = iMonth;
	tm_time.tm_mday = iDay;
	tm_time.tm_hour = iHour;
	tm_time.tm_min = iMinute;
	tm_time.tm_sec = iSecond;

	tm_time.tm_year -= 1900;
	tm_time.tm_mon -= 1;

	unix_time = mktime(&tm_time);
	return unix_time;
}

/*unix时间转字符串2019-11-27T12:05:00*/
void Comm_UnixTime2StrTime(long long lUnixTime, char *pszStrTime)
{
	if (NULL == pszStrTime)
	{
		return ;
	}

    time_t t = lUnixTime;
	struct tm tp;
	localtime_r(&t, &tp);

    sprintf(pszStrTime, "%04d-%02d-%02dT%02d:%02d:%02d",
        tp.tm_year + 1900,
        tp.tm_mon + 1,
        tp.tm_mday,
        tp.tm_hour, tp.tm_min, tp.tm_sec);

}



