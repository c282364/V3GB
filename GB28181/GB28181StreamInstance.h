#ifndef __GB28181_STREAM_INSTANCE_H__
#define __GB28181_STREAM_INSTANCE_H__

#include "MediaGateCommon.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>

class CGB28181StreamInstance
{
public:
    CGB28181StreamInstance();
    virtual ~CGB28181StreamInstance();

    virtual int releaseInstance();

    void setCID(int cid);
    void setDID(int did);
    void setStreamId(const char *pszStreamId);
    void setAutoRelease(bool bVal);

    /*call id*/
    int getCID();
    int getDID();
    //stream id
    std::string getStreamId();
    bool getAutoRelease();
    
protected:

    /*结束GB28181视频存在两种情况，1.主动挂断，要发BYE，再释放资源
        2.收到BYE，直接释放资源*/
    bool m_bAutoRelease;
    
    /*标识一次呼叫*/
    int m_cid; /*cid由SIP协议栈标识*/
    int m_did;
    std::string m_strStreamId; /*stream id由中心服务器分配*/
};


#endif

