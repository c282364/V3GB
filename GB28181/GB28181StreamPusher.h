#ifndef __GB28181_STREAM_PUSHER_H__
#define __GB28181_STREAM_PUSHER_H__

#include "./psmux/psmux.h"
#include "GB28181StreamInstance.h"

class CGB28181StreamPusher : public CGB28181StreamInstance
{
public:
    CGB28181StreamPusher();
    ~CGB28181StreamPusher();

    virtual int releaseInstance();

    /*初始化推流
        @cid: exosip的cid
        @pszStreamId: 中心服务器分配的streamid
        @strLocalIP, iLocalPort: 本端地址        
        @strRemoteIP, iRemotePort: 远端数据地址
        */
    int initStreamPusher(int cid, char *pszStreamId, std::string strLocalIP, int iLocalPort, 
        std::string strRemoteIP, int iRemotePort, unsigned long ssrc);

    /*删除函数*/
    int releaseStreamPusher();

    /*接入层的视频传入，再转发PS流到远端国标平台*/
    int inputFrameData(unsigned char* pFrameData, int iFrameLen, unsigned long long i64TimeStamp);

    /*RTP的SSRC*/
    unsigned long getSSRC();

    /*本地发流的端口*/
    int getLocalPort();

    /*一个完整的视频帧，分块RTP发送*/
    int sendOneBlock(unsigned char *pBlockData, int iBlockLen, unsigned long ulIndex, unsigned long ulTimeStamp, bool bEndHead);

private:
    unsigned char *m_pPsBuff;
    int m_fdPushStream;
    int m_iLocalPort;
    std::string m_strRemoteIP;
    int m_iRemotePort;
    struct   sockaddr_in   m_sockaddrServer;

    /*rtp流的相关信息*/
    unsigned long m_ulSSRC;  //ssrc
    unsigned short m_usSeq;  //seq
    unsigned long m_ulTimeStamp; //time stamp

};



#endif
