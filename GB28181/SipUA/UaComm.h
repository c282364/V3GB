
#ifndef __UA_COMM_H__
#define __UA_COMM_H__


#include "SipUA.h"


/*日志级别
LOG_LEVEL_DEBUG = 0,
LOG_LEVEL_INFO,
LOG_LEVEL_WARN,
LOG_LEVEL_ERR,
LOG_LEVEL_FATAL,
*/

#define SIPUA_LOG_LEVEL_DEBUG     0
#define SIPUA_LOG_LEVEL_INFO      1
#define SIPUA_LOG_LEVEL_WARN      2
#define SIPUA_LOG_LEVEL_ERR       3
#define SIPUA_LOG_LEVEL_FATAL     4


void uaLogInit(fnSipUA_OutputLogCB fn, void *pParam);

void uaOutputLog(const int iLevel, const char *fn, 
    const char *szFile, const int iLine, const char *fmt, ...);

#define DEBUG_LOG_SIPUA(format, ...) \
        uaOutputLog(SIPUA_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define INFO_LOG_SIPUA(format, ...) \
        uaOutputLog(SIPUA_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define WARN_LOG_SIPUA(format, ...) \
        uaOutputLog(SIPUA_LOG_LEVEL_WARN, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define ERR_LOG_SIPUA(format, ...) \
        uaOutputLog(SIPUA_LOG_LEVEL_ERR, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define FATAL_LOG_SIPUA(format, ...) \
        uaOutputLog(SIPUA_LOG_LEVEL_FATAL, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )


#define ENTER_FUNC_SIPUA() uaOutputLog(SIPUA_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, "--- enter func ---")	

#define EXIT_FUNC_SIPUA() uaOutputLog(SIPUA_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, "--- exit func ---")	


#endif


