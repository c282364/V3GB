#include "GB28181StreamPuller.h"
#include <string.h>
#include "CommDefine.h"
#include "RtcpPacket.h"
#include <vector>

CGB28181StreamPuller::CGB28181StreamPuller()
{
	m_fdRtp = -1;
	m_fdRtcp = -1;
	m_bThreadWorking = false;

	m_pPsData = NULL;
	m_iPsDataLen = 0;
	m_pRawData = NULL;
	//m_fpPsData = NULL;
	m_usLastRtpSeq = 0;
}

CGB28181StreamPuller::~CGB28181StreamPuller()
{

}

int CGB28181StreamPuller::releaseInstance()
{
	return releaseStreamPuller();
}

//创建fd
int CGB28181StreamPuller::createFd(int port)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
	{
        printf("create socket failed, err=%d\n", errno);
		return -1;
	}

	struct sockaddr_in localAddr;
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr(strLocalIP.c_str());
	localAddr.sin_port = htons(port);
	int ret = bind(fd, (struct sockaddr*)&localAddr, sizeof(struct sockaddr));
	if (ret < 0)
	{
        printf("bind port[%d] failed, errno=%d\n", port, errno);
		close(fd);
		fd = -1;
		return -1;
	}

	//设置fd为非阻塞
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);

	return fd;
}


int CGB28181StreamPuller::initStreamPuller(const char *pszStreamId, std::string strLocalIP, int iLocalPort,
        fnMediaGateStreamCB fnStreamCB, void *pParam)
{	
	m_strStreamId = pszStreamId;
	m_fnStreamCB = fnStreamCB;
	m_pParamCB = pParam;

	m_fdRtp = createFd(iLocalPort);
	if (m_fdRtp <= 0)
	{
		printf("create rtp fd failed, port=%d\n", iLocalPort);
		return -1;
	}

	int iRtcpPort = iLocalPort+1;
	m_fdRtcp = createFd(iRtcpPort);
	if (m_fdRtcp <= 0)
	{
        printf("create rtcp fd failed, port=%d\n", iRtcpPort);
		close(m_fdRtp);
		m_fdRtp = -1;		
		return -1;
	}

	m_pPsData = new unsigned char[RTP_RECV_BUFF_MAX_LEN];
	m_iPsDataLen = 0;
	m_pRawData = new unsigned char[RTP_RECV_BUFF_MAX_LEN];
	
	/*char szFilePs[1024] = {0};
	sprintf(szFilePs, "%s.ps", pszStreamId);
	m_fpPsData = fopen(szFilePs, "wb");*/

	/*开始国标视频接收线程*/
	sem_init(&m_semWaitExit, 0, 0);
	m_bThreadWorking = true;
	pthread_create(&m_tThreadId, 0, threadPullStream, this);

    printf("stream[%s] puller init success, use local port[%d].\n", m_strStreamId.c_str(), iLocalPort);
	return 0;
}

int CGB28181StreamPuller::releaseStreamPuller()
{
	if (m_bThreadWorking)
	{
		m_bThreadWorking = false;
		sem_wait(&m_semWaitExit);
		sem_destroy(&m_semWaitExit);    
	}
	
	if (m_fdRtp > 0)
	{
		close(m_fdRtp);
		m_fdRtp = -1;
	}

	if (m_fdRtcp > 0)
	{
		close(m_fdRtcp);
		m_fdRtcp = -1;
	}

	if (m_pPsData)
	{
		delete []m_pPsData;
		m_pPsData = NULL;
	}
	m_iPsDataLen = 0;

	/*if (m_fpPsData)
	{
		fclose(m_fpPsData);
		m_fpPsData = NULL;
	}*/

	if (m_pRawData)
	{
		delete []m_pRawData;
		m_pRawData = NULL;
	}

    printf("stream[%s] puller release.\n", m_strStreamId.c_str());

	return 0;
}

void* CGB28181StreamPuller::threadPullStream(void* args)
{
	CGB28181StreamPuller *pInst = (CGB28181StreamPuller*)args;
	pInst->doPullStream();

	return NULL;
}

void CGB28181StreamPuller::doPullStream()
{
	const int iRecvBuffLen = 4096;
	char szRecvBuff[iRecvBuffLen] = { 0 };
	long long i64LastTime = Comm_GetMilliSecFrom1970();
	unsigned long ulNowTick = Comm_GetTickCount();
	unsigned long ulSenderId = htonl(ulNowTick);
	while (m_bThreadWorking)
	{
		//50ms
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 50 * 1000;
		fd_set readFdSet;
		FD_ZERO(&readFdSet);
		FD_SET(m_fdRtp, &readFdSet);
		int ret = select(m_fdRtp+1, &readFdSet, NULL, NULL, &tv);
		if (ret < 0)
		{
            printf("select failed, errno=%d, [%s]\n", errno, m_strStreamId.c_str());
			break;
		}
		else if (0 == ret)
		{
			continue;
		}
		else
		{
			if (FD_ISSET(m_fdRtp, &readFdSet))
			{
				struct sockaddr_in addrFrom;
				int iAddrLen = sizeof(addrFrom);
				ret = recvfrom(m_fdRtp, szRecvBuff, iRecvBuffLen, 0, (struct sockaddr*)&addrFrom, (socklen_t*)&iAddrLen);
				if (ret > 0)
				{
                    //printf("recvfrom 192.168.4.2\n");
					unsigned char* pRtpData = (unsigned char*)szRecvBuff;
					handleRtpData(pRtpData, ret);

					long long i64CurTime = Comm_GetMilliSecFrom1970();
					long long i64TimeGap = i64CurTime - i64LastTime;
					if (i64TimeGap >= 5000) //5秒回复一次RTCP receiver report消息
					{					
						i64LastTime = i64CurTime;
						
						unsigned char btRtcpData[1024] = {0};
						unsigned short seq = 0;
						unsigned long ulTimeStamp = 0;
						unsigned long ssrc = 0;

						memcpy(&seq, pRtpData+2, 2);
						memcpy(&ulTimeStamp, pRtpData+4, 4);
						memcpy(&ssrc, pRtpData+8, 4);						
						
						int iRtcpDataLen = makeRtcpPacketBuff(ulSenderId, ssrc, ulTimeStamp, seq, btRtcpData);
						if (iRtcpDataLen > 0)
						{
							//rtcp地址为发送RTP端口自动+1
							unsigned short usRtpFromPort = ntohs(addrFrom.sin_port);
							struct sockaddr_in addrToRtcp;
							memcpy(&addrToRtcp, &addrFrom, iAddrLen);
							addrToRtcp.sin_port = htons(usRtpFromPort+1);
							sendto(m_fdRtcp, (char*)btRtcpData, iRtcpDataLen, 0, (sockaddr*)&addrToRtcp, sizeof(sockaddr));
						}
					}
				}
			}
		}

	}

	sem_post(&m_semWaitExit);
}

void CGB28181StreamPuller::handleRtpData(unsigned char *pRtpData, int iDataLen)
{
	#if 0
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


	
	const int iRtpHdrLen = 12;
	unsigned char *pPayload = pRtpData + iRtpHdrLen;
	int iPayloadLen = iDataLen - iRtpHdrLen;
	if (iPayloadLen <= 0)
	{
        printf("payload len[%d] <= 0.\n", iPayloadLen);
		return ;
	}

	/*
	rtp padding位：1bit
	若置1，则此RTP包包含一个或者多个附加在末端的填充数据，填充数据不算负载一部分。
	填充的最后一个字节指明可以忽略多少填充字节。
	*/
	/*RTP头第一字节第6位是padding*/
	unsigned char ucPadding = ((pRtpData[0] & 0x20) >> 5);
	if (1 == ucPadding)
	{
		unsigned char ucPaddingCount = pRtpData[iDataLen-1];
		iPayloadLen -= ucPaddingCount;
	}

	//fwrite(pPayload, 1, iPayloadLen, m_fpPsData);
	
	unsigned short seq = (pRtpData[2] << 8) | pRtpData[3];
	
	if ((m_usLastRtpSeq+1) != seq) //rtp seq不连续，发现丢包
	{
		if ((0xFFFF == m_usLastRtpSeq && 0x0000 == seq) || (0x0000 == m_usLastRtpSeq && 0x0000 == seq))
		{
			//最大seq重新回到0, 或者初始seq=0，不提示丢包
		}
		else
		{
            printf("stream[%s] rtp seq error, last=%d, curr=%d\n", m_strStreamId.c_str(), m_usLastRtpSeq, seq);
		}
		
	}	
	m_usLastRtpSeq = seq;
	
	/*有些视频的RTP头不存在marker，不能根据marker来决定帧的结束，只能以payload 00 00 01 ba来确认一帧的起始*/
	if (0x00 == pPayload[0] && 0x00 == pPayload[1] && 0x01 == pPayload[2] && 0xba == pPayload[3])
	{
		if (m_iPsDataLen > 0)
		{
			cutPsHeader();
			m_iPsDataLen = 0;
		}

		fillFrameBuff(pPayload, iPayloadLen);
	}
	else
	{
		fillFrameBuff(pPayload, iPayloadLen);
	}

#if 0	
	/*marker位是1，表示一个完整帧的接收，RTP头第二字节第8位是*/
	unsigned char ucMarker = ((pRtpData[1] & 0x80) >> 7);
	if (1 == ucMarker)
	{	
		cutPsHeader();
		m_iPsDataLen = 0;
	}
#endif

}

//填充帧缓存
int CGB28181StreamPuller::fillFrameBuff(unsigned char *pData, int iLen)
{
	if ((m_iPsDataLen + iLen) > RTP_RECV_BUFF_MAX_LEN)
	{
        printf("stream[%s] frame len out of range. %d + %d > %d\n", m_strStreamId.c_str(), m_iPsDataLen, iLen, RTP_RECV_BUFF_MAX_LEN);
		m_iPsDataLen = 0;
		return 0;
	}
	memcpy(m_pPsData + m_iPsDataLen, pData, iLen);
	m_iPsDataLen += iLen;
	return m_iPsDataLen;
}


/*收到完整的PS帧后，去掉PS头，直接返回RAW数据*/
int CGB28181StreamPuller::cutPsHeader()
{
	if (NULL == m_fnStreamCB)
		return -1;
	
	unsigned char *pInData = m_pPsData;
	int iInLen = m_iPsDataLen;	
	//查找00 00 01 E0
	if (NULL == pInData || iInLen <= 4)
	{
        printf("iInLen[%d] error.\n", iInLen);
		return 0;
	}

//        0               1               2               3
//        0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//byte=0 |start 0x00 0x00 0x00 		     				 | h264 id=0xEO  |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     4 | 		pes pack len			 | 10|   | | | | |    flag       |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//	   8 |	pes head len | head data...raw data
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

	int iRawDataLen = 0;	
	int iSrcLen = iInLen;
	for (int i = 0; i < iInLen - 4; i++)
	{
		/*跳过PS头相关内容*/
		if ((0x00 == pInData[i] && 0x00 == pInData[i + 1] && 0x01 == pInData[i + 2] && 0xBA == pInData[i + 3]) ||
			(0x00 == pInData[i] && 0x00 == pInData[i + 1] && 0x01 == pInData[i + 2] && 0xBB == pInData[i + 3]) ||
			(0x00 == pInData[i] && 0x00 == pInData[i + 1] && 0x01 == pInData[i + 2] && 0xBC == pInData[i + 3]))
		{			
			i += 4;
			continue;
		}

		if ((0x00 == pInData[i] && 0x00 == pInData[i + 1] && 0x01 == pInData[i + 2] && 0xE0 == pInData[i + 3]))
		{
			unsigned short usPesLen = 0; //PES 长，为这个字节之后整个pes 长度--es 长度 + 剩余ps报头
			unsigned short usHeadFlag = 0; //PES包头识别标志
			unsigned char ucHeadLen = 0; //PES包头长

			usPesLen = (pInData[i + 4] << 8) | (pInData[i + 5]);
			usHeadFlag = (pInData[i + 6] << 8) | (pInData[i + 7]);
			ucHeadLen = pInData[i + 8];
			
			int iDataPos = i + 9 + ucHeadLen; //跳过PES头，找到ES数据位置
			int iDataLen = usPesLen - 2 - 1 - ucHeadLen; //ES数据长度: 计算SPS长度38 - 2(PES包头识别) - 1(PES包头长度) - (PES信息区)

			if ((iDataPos < 0 || iDataPos > iSrcLen) || (iDataLen < 0 || iDataLen > iSrcLen))
			{
                printf("stream[%s] copy buff error: DataPos[%d], DataLen[%d], src[%d].\n",
					m_strStreamId.c_str(), iDataPos, iDataLen, iSrcLen);
				break;
			}

			if (((iRawDataLen + iDataLen) > iSrcLen) || (iRawDataLen + iDataLen) > RTP_RECV_BUFF_MAX_LEN)
			{
                printf("stream[%s] out of range, %d+%d>%d.\n", m_strStreamId.c_str(), iRawDataLen, iDataLen, iSrcLen);
				break;
			}

			//printf("i:%d, raw:%d, pos:%d, peslen:%d, headlen:%d, datalen:%d, inlen:%d", 
			//	i, iRawDataLen, iDataPos, usPesLen, ucHeadLen, iDataLen, iInLen);
			
			memcpy(m_pRawData + iRawDataLen, pInData + iDataPos, iDataLen);
			iRawDataLen += iDataLen;

			i += iDataLen;
		}
	}	
	
	if (iRawDataLen > 0)
	{
		//printf("stream[%s] output frame, raw_len=%d.", m_strStreamId.c_str(), iRawDataLen);
		/*void *pPuller, int iStreamType, 
        unsigned char *pBuffer, int iBufSize, void *param*/
		m_fnStreamCB((void*)this, 0, m_pRawData, iRawDataLen, m_pParamCB);
	}

	return iRawDataLen;
}





