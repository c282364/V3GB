#include "GB28181StreamPusher.h"
#include "MediaGateCommon.h"
#include <string.h>

CGB28181StreamPusher::CGB28181StreamPusher()
{
	m_pPsBuff = NULL;
	m_fdPushStream = -1;
}

CGB28181StreamPusher::~CGB28181StreamPusher()
{


}

/*初始化推流
        @cid: exosip的cid
        @pszStreamId: 中心服务器分配的streamid
        @strLocalIP, iLocalPort: 本端地址        
        @strRemoteIP, iRemotePort: 远端数据地址
*/
int CGB28181StreamPusher::initStreamPusher(int cid, char *pszStreamId, std::string strLocalIP, int iLocalPort, 
	    std::string strRemoteIP, int iRemotePort, unsigned long ssrc)
{
	m_fdPushStream = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_fdPushStream < 0)
	{
		WARN_LOG_MEDIAGATE("create socket push stream failed, err=%d\r\n", errno);
		return -1;
	}

	struct sockaddr_in localAddr;
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = inet_addr(strLocalIP.c_str());
	localAddr.sin_port = htons(iLocalPort);
	int ret = bind(m_fdPushStream, (struct sockaddr*)&localAddr, sizeof(struct sockaddr));
	if (ret < 0)
	{
		WARN_LOG_MEDIAGATE("pusher bind port[%d] failed, errno=%d\r\n", iLocalPort, errno);
		close(m_fdPushStream);
		m_fdPushStream = -1;
		return -1;
	}

	//设置fd为非阻塞
	int flag = fcntl(m_fdPushStream, F_GETFL, 0);
	fcntl(m_fdPushStream, F_SETFL, flag | O_NONBLOCK);

	m_pPsBuff = new unsigned char[1024*512];

	m_cid = cid;
	m_strStreamId = pszStreamId;
	m_iLocalPort = iLocalPort;
	m_strRemoteIP = strRemoteIP;
	m_iRemotePort = iRemotePort;
	
	m_ulSSRC = ssrc;
	m_usSeq = 0;
	m_ulTimeStamp = 0;

	m_sockaddrServer.sin_addr.s_addr = inet_addr(strRemoteIP.c_str());
	m_sockaddrServer.sin_family = AF_INET;
	m_sockaddrServer.sin_port = ntohs(iRemotePort);

	INFO_LOG_MEDIAGATE("stream[%s] pusher init success, use local port[%d].\r\n", m_strStreamId.c_str(), iLocalPort);

	return 0;
}

int CGB28181StreamPusher::releaseInstance()
{
	return releaseStreamPusher();
}


int CGB28181StreamPusher::releaseStreamPusher()
{
	INFO_LOG_MEDIAGATE("stream[%s] pusher release.\r\n", m_strStreamId.c_str());
	
	if (m_pPsBuff)
	{
		delete []m_pPsBuff;
		m_pPsBuff = NULL;
	}

	if (m_fdPushStream > 0)
	{
		close(m_fdPushStream);
		m_fdPushStream = -1;
	}
	
	return 0;
}

int CGB28181StreamPusher::sendOneBlock(unsigned char *pBlockData, int iBlockLen, unsigned long ulIndex, unsigned long ulTimeStamp, bool bEndHead)
{
	int len = 0;

	const int iMaxSendBufSize = 2048;
	if (iBlockLen > iMaxSendBufSize)
	{
		WARN_LOG_MEDIAGATE("block size[%d] > max size[%d]\r\n", iBlockLen, iMaxSendBufSize);
		return -1;
	}

	/* build the RTP header */
    /*
     *
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                           timestamp                           |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |           synchronization source (SSRC) identifier            |
     *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     *   |            contributing source (CSRC) identifiers             |
     *   :                             ....                              :
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     **/

	//BYTE pSendBuff[200] = {0x80, 0x60, 0xA8, 0x5F, 0x08, 0x1B, 0x64, 0xAC, 0x44, 0xB4, 0xAC, 0xB1, 0x00};

	/*RTP头定义如上，以下是简单处理RTP头*/
	
	unsigned char pSendBuff[iMaxSendBufSize] = {0};

	pSendBuff[0] = 0x80;

	if (bEndHead)
	{
		pSendBuff[1] = 0xE0;
	}
	else
		pSendBuff[1] = 0x60;

	//索引
	pSendBuff[2] = ((ulIndex & 0xFF00) >> 8);
	pSendBuff[3] = ulIndex & 0x00FF;

	//时间戳
	pSendBuff[4] = ((ulTimeStamp & 0xFF000000) >> 24);
	pSendBuff[5] = ((ulTimeStamp & 0x00FF0000) >> 16);
	pSendBuff[6] = ((ulTimeStamp & 0x0000FF00) >> 8);
	pSendBuff[7] = ((ulTimeStamp & 0x000000FF));

	//ssrc
	pSendBuff[8] = ((m_ulSSRC & 0xFF000000) >> 24);
	pSendBuff[9] = ((m_ulSSRC & 0x00FF0000) >> 16);
	pSendBuff[10] = ((m_ulSSRC & 0x0000FF00) >> 8);
	pSendBuff[11] = ((m_ulSSRC & 0x000000FF));

	memcpy(pSendBuff + RTP_HDR_LEN, pBlockData, iBlockLen);
	len = RTP_HDR_LEN + iBlockLen;

	
	/*int select(int nfds, fd_set *readfds, fd_set *writefds,
					  fd_set *exceptfds, struct timeval *timeout);*/

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 50 * 1000;
	fd_set writeFdSet;
	FD_ZERO(&writeFdSet);
	FD_SET(m_fdPushStream, &writeFdSet);

	int ret = select(m_fdPushStream+1, NULL, &writeFdSet, NULL, &tv);
	if (ret <= 0)
	{
		return -1;
	}
	else
	{
		if (FD_ISSET(m_fdPushStream, &writeFdSet))
		{
			int ret = sendto(m_fdPushStream, (char*)pSendBuff, len, 0, (sockaddr*)&m_sockaddrServer, sizeof(sockaddr));
						
			return ret;
		}
	}

	return -1;
}


int CGB28181StreamPusher::inputFrameData(unsigned char* pFrameData, int iFrameLen, unsigned long long i64TimeStamp)
{
	//static unsigned long ulTimeStamp = 0; //(unsigned long)i64TimeStamp;
	//ulTimeStamp += 3000;
	//static FILE *fpt = fopen("ps_dump.ps", "wb");

	//00 00 00 01 67/68/65/41
	NAL_type Type = getH264NALtype(pFrameData[4]);
	if (NAL_SPS == Type || NAL_PFRAME == Type)
	{
		m_ulTimeStamp += 3600;
	}	
	
	int iPsLen = h264PsMux(pFrameData, iFrameLen, Type, (unsigned long long)m_ulTimeStamp, m_pPsBuff);
	if (iPsLen > 0)
	{
		//fwrite(m_pPsBuff, 1, iPsLen, fpt);

	
		////每次发送1400字节数据
		int iFrameSizePerCap = 1400;
		int iTotalSent = 0;		
	
		int iLeftLen = iPsLen;
		int iSentLen = 0;
		int iBlockLen = iFrameSizePerCap;
		while (iLeftLen > 0)
		{		
			m_usSeq++;
			
			if (iLeftLen > iFrameSizePerCap)
			{
				iBlockLen = iFrameSizePerCap;
				iTotalSent += sendOneBlock(m_pPsBuff+iSentLen, iBlockLen, m_usSeq, m_ulTimeStamp, false);
			}
			else 
			{
				//最后结束块
				iBlockLen = iLeftLen;
				iTotalSent += sendOneBlock(m_pPsBuff+iSentLen, iBlockLen, m_usSeq, m_ulTimeStamp, true);	
			}
			
			iLeftLen -= iBlockLen;
			iSentLen += iBlockLen;
		}

		return iTotalSent;
	}
	
	return -1;
}

unsigned long CGB28181StreamPusher::getSSRC()
{
	return m_ulSSRC;
}

int CGB28181StreamPusher::getLocalPort()
{
	return m_iLocalPort;
}


