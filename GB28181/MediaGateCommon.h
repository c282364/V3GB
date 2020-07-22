#ifndef __MEDIA_GATE_COMMON_H__
#define __MEDIA_GATE_COMMON_H__

#include <stdlib.h>
#include <stdio.h>
#include "SipUA.h"

/*日志级别
LOG_LEVEL_DEBUG = 0,
LOG_LEVEL_INFO,
LOG_LEVEL_WARN,
LOG_LEVEL_ERR,
LOG_LEVEL_FATAL,
*/

#define GB_MEDIAGATE_LOG_LEVEL_DEBUG     0
#define GB_MEDIAGATE_LOG_LEVEL_INFO      1
#define GB_MEDIAGATE_LOG_LEVEL_WARN      2
#define GB_MEDIAGATE_LOG_LEVEL_ERR       3
#define GB_MEDIAGATE_LOG_LEVEL_FATAL     4


void mediaGateLogInit(fnSipUA_OutputLogCB fn, void *pParam);

void mediaGateOutputLog(const int iLevel, const char *fn, 
    const char *szFile, const int iLine, const char *fmt, ...);

#define DEBUG_LOG_MEDIAGATE(format, ...) \
        mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define INFO_LOG_MEDIAGATE(format, ...) \
        mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define WARN_LOG_MEDIAGATE(format, ...) \
        mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_WARN, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define ERR_LOG_MEDIAGATE(format, ...) \
        mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_ERR, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )
    
#define FATAL_LOG_MEDIAGATE(format, ...) \
        mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_FATAL, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__ )

#define ENTER_FUNC_MEDIAGATE() mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, "--- enter func ---")	

#define EXIT_FUNC_MEDIAGATE() mediaGateOutputLog(GB_MEDIAGATE_LOG_LEVEL_DEBUG, __func__, __FILE__, __LINE__, "--- exit func ---")	


/*rtp头定义*/
typedef struct tagSRtpHdr
{
    unsigned int version:2; /* protocol version */
    unsigned int p:1;/* padding flag */
    unsigned int x:1;/* header extension flag */
    unsigned int cc:4;/* CSRC count */
    unsigned int m:1;/* marker bit */
    unsigned int pt:7;/* payload type */

    unsigned short seq; /* sequence number */
    unsigned long ts;/* timestamp */
    unsigned long ssrc;/* synchronization source */
}SRtpHdr;


#endif


