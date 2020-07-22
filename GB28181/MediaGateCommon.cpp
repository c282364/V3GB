#include "MediaGateCommon.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


//log回调函数
fnSipUA_OutputLogCB g_fnMediaGateLogCB = NULL;
void *g_pParamMediaGateLogCB = NULL;

void mediaGateLogInit(fnSipUA_OutputLogCB fn, void *pParam)
{
	g_fnMediaGateLogCB = fn;
	g_pParamMediaGateLogCB = pParam;
}

void mediaGateOutputLog(const int iLevel, const char *fn, 
    const char *szFile, const int iLine, const char *fmt, ...)
{
    if (NULL == g_fnMediaGateLogCB)
        return;
    
    char szLogInfo[4096] = {0};
    va_list args;
    va_start(args, fmt);
	vsnprintf(szLogInfo, 4096, fmt, args);
    va_end(args);

    g_fnMediaGateLogCB(iLevel, fn, szFile, iLine, szLogInfo, g_pParamMediaGateLogCB);
}


