#ifndef __GB28181_STREAM_PULLER_H__
#define __GB28181_STREAM_PULLER_H__

#include "GB28181StreamInstance.h"
#include "GB28181MediaGate.h"
#include <pthread.h>
#include <semaphore.h>

#define RTP_RECV_BUFF_MAX_LEN	(1024*1024)

class CGB28181StreamPuller : public CGB28181StreamInstance
{
public:
    CGB28181StreamPuller();
    ~CGB28181StreamPuller();

    virtual int releaseInstance();

    int initStreamPuller(const char *pszStreamId, std::string strLocalIP, int iLocalPort,
        fnMediaGateStreamCB fnStreamCB, void *pParam);

    int releaseStreamPuller();

    /*接收RTP数据线程*/
    static void* threadPullStream(void* args);
    void doPullStream();

    /*处理收到的RTP数据*/
	void handleRtpData(unsigned char *pRtpData, int iDataLen);

    /*拼接RTP数据包*/
    int fillFrameBuff(unsigned char *pData, int iLen);

    /*收到完整的PS帧后，去掉PS头，直接返回RAW数据*/
    int cutPsHeader();

	//创建fd
	int createFd(int port);

private:
	//rtp/rtcp fd
    int m_fdRtp;
	int m_fdRtcp;
    fnMediaGateStreamCB m_fnStreamCB;
    void *m_pParamCB;

    /*国标视频接收线程*/
    pthread_t m_tThreadId;
    bool m_bThreadWorking;
    sem_t m_semWaitExit;

    /*ps数据缓存*/
    unsigned char *m_pPsData;
	int m_iPsDataLen;	
	//FILE *m_fpPsData;
	unsigned short m_usLastRtpSeq;
	
    /*raw数据缓存*/
    unsigned char *m_pRawData;	
};





#endif


