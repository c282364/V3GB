#include "UaComm.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

//log回调函数
fnSipUA_OutputLogCB g_fnSipUALogCB = NULL;
void *g_pParamSipUALogCB = NULL;

void uaLogInit(fnSipUA_OutputLogCB fn, void *pParam)
{
	g_fnSipUALogCB = fn;
	g_pParamSipUALogCB = pParam;
}

void uaOutputLog(const int iLevel, const char *fn, 
    const char *szFile, const int iLine, const char *fmt, ...)
{
    if (NULL == g_fnSipUALogCB)
        return;
    
    char szLogInfo[4096] = {0};
    va_list args;
    va_start(args, fmt);
	vsnprintf(szLogInfo, 4096, fmt, args);
    va_end(args);

    g_fnSipUALogCB(iLevel, fn, szFile, iLine, szLogInfo, g_pParamSipUALogCB);
}

	


