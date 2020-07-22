#include "GB28181MediaGate.h"
#include "SipUA.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include "GB28181StreamPusher.h"
#include "GB28181StreamPuller.h"
#include "MediaGateCommon.h"
#include "ThreadTimer.h"
#include "CommDefine.h"

//本地国标配置
std::string g_strLocalSipId;
std::string g_strLocalMediaIP;

//sip server国标配置
std::string g_strSipServerId;
std::string g_strSipServerIP;
int g_iSipServerPort = 5060;

//注册消息返回的regid
int g_iRegisterId = 0;
//注册超时时间
int g_iExpires = 3600;
//注册到sip server成功
bool g_bRegisterSuccess = false;
//注册时间
long long g_i64RegisterTime = 0; 

//上次发送Keepalive的时间
long long g_i64LastKeepaliveTime = 0; 

fnMediaGateEvent g_fnMediaGateEventCB = NULL;
void *g_pMediaGateUserParam = NULL;

#define MEDIA_GATE_MAX_USE_PORT		65000

/*发送PS流的开始端口*/
unsigned short g_usMediaGateStreamStartPort = 30000;
/*当前使用的端口*/
unsigned short g_usCurrUseMediaGateStreamPort = g_usMediaGateStreamStartPort;

/*国标推流实例*/
std::map<int, CGB28181StreamInstance*> g_mapGB28181StreamInstance;

//代理配置参数
int g_iProxyEnable = 0;
std::string g_strProxyIP;
int g_iProxySignalPort = 0;
int g_iProxyStreamPort = 0;
/*当前使用的端口, 上云时使用的对外端口*/
unsigned short g_usCurrUseProxyStreamPort = 0;


/*从字符串中得到key=val的val值*/
int mediaGateGetKeyValue(char *pszInStr, const char *pszKey, char *pszVal, int iValMaxSize)
{
	if (NULL == pszInStr || NULL == pszKey || NULL == pszVal)
		return -1;
	
	char szKeyName[1024] = {0};
	int iNameLen = sprintf(szKeyName, "%s=", pszKey);
	char *pKeyPos = strstr(pszInStr, szKeyName);
	if (NULL == pKeyPos)
		return -1;

	char *pValPos = pKeyPos + iNameLen;
	int iValLen = 0;
	while (*pValPos)
	{
		if ('&' == *pValPos || '\r' == *pValPos || '\n' == *pValPos)
			break;

		if (iValLen >= iValMaxSize)
			return -1;

		pszVal[iValLen] = *pValPos;
		iValLen++;
		pValPos++;
	}
	
	return iValLen;
}



/*媒体网关注册到sip server
@pszServerId: sip server的国标编码
@pszIP, port: sip server的地址
@pszBody, iBodyLen: 注册携带的消息体
*/
int mediaGateRegister(const char *pszServerId, const char *pszIP, int port)
{
	ENTER_FUNC_MEDIAGATE();
	std::string strBody = "gb28181 register from media gate";
	int ret = SipUA_RegisterUnauthor(g_strLocalSipId.c_str(), g_iExpires, pszServerId, pszIP, port, 
		strBody.c_str(), strBody.length());
	if (ret <= 0) //REGISTER失败
	{
		ERR_LOG_MEDIAGATE("SipUA_RegisterUnauthor failed, localid[%s], expires[%d], server[%s@%s:%d]", 
			g_strLocalSipId.c_str(), g_iExpires, pszServerId, pszIP, port);
		EXIT_FUNC_MEDIAGATE();
		return -1;
	}
	//REGISTER成功，保存register id
	g_iRegisterId = ret;
	EXIT_FUNC_MEDIAGATE();
	return ret;
}


//定时发送Keepavlie到sip server
void keepaliveMsgTimer(HTIMER hTimer, void *pUserData)
{
	//device cloud未向sip server注册
	if (false == g_bRegisterSuccess)
	{		
		mediaGateRegister(g_strSipServerId.c_str(), g_strSipServerIP.c_str(), g_iSipServerPort);
		return ;
	}
	
	long long i64CurTime = Comm_GetMilliSecFrom1970();

	//注册超时
	long long i64RegGap = i64CurTime - g_i64RegisterTime;
	if (i64RegGap >= ((g_iExpires - 30) * 1000)) //超时前30秒，刷新注册
	{
		int ret = SipUA_RefreshRegister(g_iRegisterId, g_iExpires);
		if (0 != ret)
		{
			ERR_LOG_MEDIAGATE("refresh register failed");
		}
		else
		{
			INFO_LOG_MEDIAGATE("refresh register success.");
			g_i64RegisterTime = i64RegGap;
		}
		return ;
	}

	//定时发送Keepalive
	long long i64KeepaliveGap = i64CurTime - g_i64LastKeepaliveTime;
	if (i64KeepaliveGap >= 30*1000) //30秒一次Keepalive
	{
		g_i64LastKeepaliveTime = i64CurTime;
		char szKeepaliveXml[1024] = {0};
		int iXmlLen = sprintf(szKeepaliveXml, "<?xml version=\"1.0\"?>\r\n"
			"<Notify>\r\n"
			"<CmdType>Keepalive</CmdType>\r\n"
			"<SN>43</SN>\r\n"
			"<DeviceID>%s</DeviceID>\r\n"
			"<Status>OK</Status>\r\n"
			"</Notify>\r\n", g_strLocalSipId.c_str());
		int ret = SipUA_InitMessage(g_strLocalSipId.c_str(), g_strSipServerId.c_str(), g_strSipServerIP.c_str(), 
			g_iSipServerPort, szKeepaliveXml, iXmlLen);
		if (ret < 0)
		{
			ERR_LOG_MEDIAGATE("send keepalive failed, server[%s@%s:%d], body: %s", g_strSipServerId.c_str(), 
				g_strSipServerIP.c_str(), g_iSipServerPort, szKeepaliveXml);
		}	
	}
}



/*
v=0
o=34020000002020000001 0 0 IN IP4 192.168.3.81
s=Play
c=IN IP4 192.168.3.81
t=0 0
m=video 6000 RTP/AVP 96 98 97
a=recvonly
a=rtpmap:96 PS/90000
a=rtpmap:98 H264/90000
a=rtpmap:97 MPEG4/90000
y=0100000001
f=

*/
void setDefaultSdp(SSdpInfo *pSdpInfo, const char *pszSessionName, const char *pszSipId, 
	const char *pszStreamIP, int port, const char *pszSSRC)
{
	memset(pSdpInfo, 0, sizeof(SSdpInfo));
	sprintf(pSdpInfo->szVersion, "0");
	sprintf(pSdpInfo->szOrigin, "%s 0 0 IN IP4 %s", pszSipId, pszStreamIP);
	sprintf(pSdpInfo->szSessionName, "%s", pszSessionName);
	sprintf(pSdpInfo->szConnection, "IN IP4 %s", pszStreamIP);
	pSdpInfo->i64TimeBegin = 0; pSdpInfo->i64TimeEnd = 0;
	//m=video
	pSdpInfo->sVideoMediaInfo.ucUseFlag = 1;
	pSdpInfo->sVideoMediaInfo.port = port;
	sprintf(pSdpInfo->sVideoMediaInfo.szProto, "RTP/AVP");
	pSdpInfo->sVideoMediaInfo.iAttrCount = 3;
	pSdpInfo->sVideoMediaInfo.sAttr[0].ePayloadType = PAYLOAD_TYPE_PS;
	sprintf(pSdpInfo->sVideoMediaInfo.sAttr[0].szAttrLine, "rtpmap:96 PS/90000");
	pSdpInfo->sVideoMediaInfo.sAttr[1].ePayloadType = PAYLOAD_TYPE_MPEG4;
	sprintf(pSdpInfo->sVideoMediaInfo.sAttr[1].szAttrLine, "rtpmap:97 MPEG4/90000");
	pSdpInfo->sVideoMediaInfo.sAttr[2].ePayloadType = PAYLOAD_TYPE_H264;
	sprintf(pSdpInfo->sVideoMediaInfo.sAttr[2].szAttrLine, "rtpmap:98 H264/90000");
	//m=audio
	pSdpInfo->sAudioMediaInfo.ucUseFlag = 0;
	//ssrc
	sprintf(pSdpInfo->szY, "%s", pszSSRC);
}

/*从map中查找stream instance*/
CGB28181StreamInstance *getStreamInstance(int cid)
{
	std::map<int, CGB28181StreamInstance*>::iterator iter;
	iter = g_mapGB28181StreamInstance.find(cid);
	if (iter == g_mapGB28181StreamInstance.end())
	{
		return NULL;
	}
	CGB28181StreamInstance *pInst = iter->second;
	return pInst;
}

/**/
int eraseStreamInstance(int cid)
{
	ENTER_FUNC_MEDIAGATE();
	
	std::map<int, CGB28181StreamInstance*>::iterator iter;
	iter = g_mapGB28181StreamInstance.find(cid);
	if (iter == g_mapGB28181StreamInstance.end())
	{
		EXIT_FUNC_MEDIAGATE();
		WARN_LOG_MEDIAGATE("cannot find stream instance, cid=%d", cid);
		return -1;
	}

	CGB28181StreamInstance *pInst = iter->second;
	/*删除资源puller停止接收线程, pusher删除...*/
	pInst->releaseInstance();		
	delete pInst;
	/*从map中移除*/	
	g_mapGB28181StreamInstance.erase(iter);	

	INFO_LOG_MEDIAGATE("erase stream instance, cid=%d", cid);
	EXIT_FUNC_MEDIAGATE();
	return 0;
}

/*创建推流句柄，把本端视频推送到远端国标平台*/
CGB28181StreamPusher *createStreamPusher(int cid, char *pszStreamId, char *pszRemoteIP, int iRemotePort)
{
	ENTER_FUNC_MEDIAGATE();
	
	CGB28181StreamPusher *pPusher = new CGB28181StreamPusher;
	/*尝试三次打开本地数据端口*/
	int iTry = 0;
	const int iCount = 3;
	for (iTry = 0; iTry < iCount; iTry++)
	{
		/*端口快到65535，再从头开始*/
		if (g_usCurrUseMediaGateStreamPort >= MEDIA_GATE_MAX_USE_PORT)
		{
			g_usCurrUseMediaGateStreamPort = g_usMediaGateStreamStartPort;
		}

		/*随机生成SSRC*/
		srand((int)time(0));
		unsigned long ssrc = 100000 + (rand() % 200000);
		int ret = pPusher->initStreamPusher(cid, pszStreamId, g_strLocalMediaIP, g_usCurrUseMediaGateStreamPort, 
			pszRemoteIP, iRemotePort, ssrc);
		if (0 != ret)
		{			
			WARN_LOG_MEDIAGATE("init stream pusher failed, use port %d, will try again.\r\n", g_usCurrUseMediaGateStreamPort);
			pPusher->releaseStreamPusher();
			g_usCurrUseMediaGateStreamPort += 2;
			continue;
		}

		/*初始化成功，break*/
		g_usCurrUseMediaGateStreamPort += 2;
		break;
	}

	/*三次都失败*/
	if (iTry == iCount)
	{
		WARN_LOG_MEDIAGATE("init stream pusher failed, try %d count.\r\n", iTry);
		delete pPusher;
		EXIT_FUNC_MEDIAGATE();
		return NULL;
	}

	EXIT_FUNC_MEDIAGATE();
	return pPusher;
}




/*处理invite消息*/
void mediaGateHandleCallInviteIncoming(void *pMsgPtr, SSdpInfo *pSdpInfo)
{	
	ENTER_FUNC_MEDIAGATE();
	
	if (NULL == g_fnMediaGateEventCB)
	{
		WARN_LOG_MEDIAGATE("media gate event callback null.\r\n");
		EXIT_FUNC_MEDIAGATE();
		return ;
	}
	
	int ret = 0;	
	int cid = SipUA_GetCallCallId(pMsgPtr);
	char szStreamId[256] = {0};
	ret = mediaGateGetKeyValue(pSdpInfo->szX, "id", szStreamId, 256);
	if (ret <= 0)
	{
		WARN_LOG_MEDIAGATE("cannot find 'id', sdp info error, x=%s.\r\n", pSdpInfo->szX);
		EXIT_FUNC_MEDIAGATE();
		return ;
	}

	/*创建推流句柄*/
	CGB28181StreamPusher *pPusher = createStreamPusher(
		cid, szStreamId, pSdpInfo->sVideoMediaInfo.szAddr, pSdpInfo->sVideoMediaInfo.port);
	if (NULL == pPusher)
	{					
		ERR_LOG_MEDIAGATE("create stream pusher failed.\r\n");
		/*处理异常，创建推流句柄失败*/
	}
	else
	{
		HMG hPusher = (HMG)pPusher;
		ret = g_fnMediaGateEventCB(MEDIA_GATE_MSG_CALL_INIT, szStreamId, pSdpInfo->szX, hPusher, g_pMediaGateUserParam);
		if (MEDIA_GATE_RETURN_SUCCESS == ret)
		{
			/*处理INVITE信令成功*/
			g_mapGB28181StreamInstance.insert(std::pair<int, CGB28181StreamInstance*>(cid, pPusher));
			char szSSRC[256] = {0};
			sprintf(szSSRC, "%lu", pPusher->getSSRC());
			SSdpInfo sAnswerSdp;
			setDefaultSdp(&sAnswerSdp, "Play", g_strLocalSipId.c_str(), 
				g_strLocalMediaIP.c_str(), pPusher->getLocalPort(), szSSRC);
			int tid = SipUA_GetCallTransId(pMsgPtr);
			int did = SipUA_GetCallDialogsId(pMsgPtr);
			pPusher->setDID(did);
			SipUA_AnswerInvite200OK(tid, &sAnswerSdp);
		}
		else
		{
			/*处理INVITE信令失败*/
			pPusher->releaseStreamPusher();
			delete pPusher;
		}			
	}

	EXIT_FUNC_MEDIAGATE();
}

/*处理bye消息*/
void mediaGateHandleCallBye(void *pMsgPtr)
{
	ENTER_FUNC_MEDIAGATE();
	
	if (NULL == g_fnMediaGateEventCB)
	{
		WARN_LOG_MEDIAGATE("media gate event callback null.\r\n");
		EXIT_FUNC_MEDIAGATE();
		return ;
	}
		
	int cid = SipUA_GetCallCallId(pMsgPtr);
	CGB28181StreamInstance *pInst = getStreamInstance(cid);
	if (NULL == pInst)
	{
		WARN_LOG_MEDIAGATE("cannot stream instance, cid=%d.\r\n", cid);
		EXIT_FUNC_MEDIAGATE();
		return ;
	}	
	
	std::string strStreamId = pInst->getStreamId();
	pInst->setAutoRelease(true); /*收到远端BYE，自动释放资源*/

	/*从map中移除*/
	eraseStreamInstance(cid);
	
	/*通知外部模块处理BYE消息*/
	g_fnMediaGateEventCB(MEDIA_GATE_MSG_CALL_RELEASE, strStreamId.c_str(), NULL, NULL, g_pMediaGateUserParam);
	
	EXIT_FUNC_MEDIAGATE();
	
}

/*处理注册成功消息*/
void mediaGateHandleRegisterSuccess(void *pMsgPtr)
{
    printf("RegisterSuccess\n");
	ENTER_FUNC_MEDIAGATE();
	g_bRegisterSuccess = true;
	g_i64RegisterTime = Comm_GetMilliSecFrom1970();
	EXIT_FUNC_MEDIAGATE();
}

/*处理注册失败消息*/
void mediaGateHandleRegisterFailure(void *pMsgPtr)
{
    printf("RegisterFailure\n");
	ENTER_FUNC_MEDIAGATE();
	g_bRegisterSuccess = false;
	EXIT_FUNC_MEDIAGATE();
}

/*处理MESSAGE 400消息*/
void mediaGateHandleMessageFailure(void *pMsgPtr)
{
	ENTER_FUNC_MEDIAGATE();
	/*device cloud media与sip server间只有一种Keepalive的MESSAGE消息，所以如果收到MESSAGE 400响应，
	则认为sip server返回device cloud media未注册*/
	static int iMsgFailureCount = 0;
	iMsgFailureCount++;
	if (iMsgFailureCount >= 3) //超过3次
	{
		iMsgFailureCount = 0;
		g_bRegisterSuccess = false;
		WARN_LOG_MEDIAGATE("Keepalive message recv 400 failure, device cloud media will re-register.");
	}

	EXIT_FUNC_MEDIAGATE();
}



void GB28181EventCB(ESipUAMsg msg, void *pMsgPtr, void *pParam)
{
	ENTER_FUNC_MEDIAGATE();
	
	DEBUG_LOG_MEDIAGATE("gb28181 media gate recv msg=%d", msg);
	/*if (pSdpInfo)
		printf("video: %s:%d\r\n", pSdpInfo->sVideoMediaInfo.szAddr, pSdpInfo->sVideoMediaInfo.port);

	if (pSubjectInfo)
		printf("subject: %s:%d,%s:%d\r\n", pSubjectInfo->szSenderId, pSubjectInfo->iSenderStreamSeq, pSubjectInfo->szRecverId, pSubjectInfo->iRecverStreamSeq);
	printf("---------body---------\r\n%s\r\n", pszBody);*/

	switch (msg)
	{
	/************************注册类消息*****************************/
	case SIPUA_MSG_REGISTER_SUCCESS: /*注册成功*/
		mediaGateHandleRegisterSuccess(pMsgPtr);
		break;
		
    case SIPUA_MSG_REGISTER_FAILURE: /*注册失败*/
		mediaGateHandleRegisterFailure(pMsgPtr);
		break;
	

	/************************呼叫类消息*****************************/
	case SIPUA_MSG_CALL_INVITE_INCOMING://invite
		{
			SSdpInfo sdpinfo;
			SipUA_GetRequestSdpInfo(pMsgPtr, &sdpinfo);
			mediaGateHandleCallInviteIncoming(pMsgPtr, &sdpinfo);		
		}
	break;

	case SIPUA_MSG_CALL_ANSWER_200OK: //200 ok
		{
			char szBody[4096] = {0};
			SipUA_GetResponseBodyContent(pMsgPtr, szBody, 4096);
			DEBUG_LOG_MEDIAGATE("recv 200 OK body\r\n%s", szBody);

			int cid = SipUA_GetCallCallId(pMsgPtr);
			int did = SipUA_GetCallDialogsId(pMsgPtr);
			CGB28181StreamInstance *pInst = getStreamInstance(cid);
			if (pInst)
			{
				pInst->setDID(did);
			}
			
			SipUA_SendAck(did);
		}
	break;

	case SIPUA_MSG_CALL_ANSWER_RSP: //非200响应
		{
			DEBUG_LOG_MEDIAGATE("handle SIPUA_MSG_CALL_ANSWER_RSP");
			int cid = SipUA_GetCallCallId(pMsgPtr);
			CGB28181StreamInstance *pInst = getStreamInstance(cid);
			if (pInst)
			{
				/*通知外部模块处理释放资源*/
				g_fnMediaGateEventCB(MEDIA_GATE_MSG_CALL_RELEASE, pInst->getStreamId().c_str(), NULL, NULL, g_pMediaGateUserParam);
			}
		}
	break;

	case SIPUA_MSG_CALL_BYE: //bye
		mediaGateHandleCallBye(pMsgPtr);
	break;

	/************************MESSAGE类消息*****************************/
	case SIPUA_MSG_MESSAGE_ANSWERED: //MESSAGE 200 ok
	break;

	case SIPUA_MSG_MESSAGE_REQUESTFAILURE: //MESSAGE 400响应
		mediaGateHandleMessageFailure(pMsgPtr);
	break;
	
	default:
	break;
	}

	EXIT_FUNC_MEDIAGATE();
}


/*初始化国标媒体网关
@transport: UDP/TCP模式传输SIP信令
@pszLocalSipId: 媒体网关的SIP编码
@pszSignalAddr, iSignalPort: 信令面地址
@pszMediaAddr, iMediaStartPort: 媒体面的数据地址
@pszSipServerId, pszSipServerIP, iSipServerPort: sip server的id, sip信令地址
@fnCB, pParam: 事件回调函数
@iProxyEnable, 是否使用代理
@pszProxyIP, iProxySignalPort, iProxyStreamPort: 代理的IP，信令端口与媒体端口
*/
int GB28181MediaGate_Init(int transport, const char *pszLocalSipId,
    const char * pszSignalAddr, int iSignalPort, 
    const char * pszMediaAddr, int iMediaStartPort, 
    const char *pszSipServerId, const char * pszSipServerIP, int iSipServerPort,
    fnMediaGateEvent fnCB, void *pParam,
    fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB,
    int iProxyEnable, const char *pszProxyIP, int iProxySignalPort, int iProxyStreamPort)
{	
	mediaGateLogInit(fnLogCB, pParamLogCB);

	ENTER_FUNC_MEDIAGATE();

	const char *pszSipUaAddr = NULL;
	int iSipuaPort = 0;
	if (1 == iProxyEnable)
	{		
		pszSipUaAddr = pszProxyIP;
		iSipuaPort = iProxySignalPort;
		g_strLocalMediaIP = pszProxyIP;
	}
	else
	{
		pszSipUaAddr = pszSignalAddr;
		iSipuaPort = iSignalPort;
		g_strLocalMediaIP = pszMediaAddr;
	}
	
	int ret = SipUA_Init(transport, iSignalPort, pszSipUaAddr, iSipuaPort, fnLogCB, pParamLogCB);
	if (0 != ret)
	{
		ERR_LOG_MEDIAGATE("init sipua failed.");
		EXIT_FUNC_MEDIAGATE();
		return -1;
	}
	
	ret = SipUA_StartEventLoop(GB28181EventCB, NULL);
	if (0 != ret)
	{
		ERR_LOG_MEDIAGATE("sipua start event loop failed.");
		EXIT_FUNC_MEDIAGATE();
		return -1;
	}
	
	g_fnMediaGateEventCB = fnCB;
	g_pMediaGateUserParam = pParam;
	g_usMediaGateStreamStartPort = iMediaStartPort;
	g_usCurrUseMediaGateStreamPort = g_usMediaGateStreamStartPort;
	g_strLocalSipId = pszLocalSipId;

	g_strSipServerId = pszSipServerId;
	g_strSipServerIP = pszSipServerIP;
	g_iSipServerPort = iSipServerPort;

	g_iProxyEnable = iProxyEnable;
	g_strProxyIP = pszProxyIP;
	g_iProxySignalPort = iProxySignalPort;
	g_iProxyStreamPort = iProxyStreamPort;
	g_usCurrUseProxyStreamPort = g_iProxyStreamPort;

	Comm_CreateThreadTimer(10*1000, keepaliveMsgTimer, NULL);

#if 0
	//注册到sip server带私有消息体
		std::string strBody = "gb28181 register from media gate";
		GB28181MediaGate_Register(g_configer.getSipServerId().c_str(), g_configer.getSipServerIP().c_str(), g_configer.getSipServerPort(), 
			strBody.c_str(), strBody.length());
#endif

	INFO_LOG_MEDIAGATE("gb28181 mediagate inited, %s@%s:%d", g_strLocalSipId.c_str(), pszSignalAddr, iSignalPort);
	
	EXIT_FUNC_MEDIAGATE();
	return 0;
}

int GB28181MediaGate_Release()
{
	ENTER_FUNC_MEDIAGATE();
	SipUA_Release();
	EXIT_FUNC_MEDIAGATE();
	return 0;
}



/*向远端国标平台推送视频流*/
int GB28181MediaGate_PushFrame(HMG hPusher, unsigned char *pFrameData, int iFrameLen, unsigned long long i64TimeStamp)
{
	ENTER_FUNC_MEDIAGATE();
	if (NULL == hPusher)
		return -1;

	CGB28181StreamPusher *pPusher = (CGB28181StreamPusher *)hPusher;
	int ret = pPusher->inputFrameData(pFrameData, iFrameLen, i64TimeStamp);
	EXIT_FUNC_MEDIAGATE();
	return ret;
}

/*创建拉流句柄，从远端国标平台获取视频
@pszStreamId: 视频流ID
@pszDestId: 请求的设备ID
@pszDestIP, iDestPort: sip server的地址
@iPlaybackFlag: 历史视频标记，0实时视频，1历史视频
@i64StartTime， i64StopTime：开始，结束时间，如果iPlaybackFlag为实时视频，这两参数忽略
@fnStreamCB, pParam: 视频流的回调接口
*/
HMG GB28181MediaGate_CreateStreamPuller(const char *pszStreamId, 
	const char * pszDestId,const char * pszDestIP, int iDestPort,
	int iPlaybackFlag, long long i64StartTime, long long i64StopTime,
	fnMediaGateStreamCB fnStreamCB, void *pParam)
{
	//ENTER_FUNC_MEDIAGATE();
	
	CGB28181StreamPuller *pPuller = new CGB28181StreamPuller;

	int iLocalPort = 0;
	/*尝试三次打开本地数据端口*/
	int iTry = 0;
	const int iCount = 3;
	for (iTry = 0; iTry < iCount; iTry++)
	{
		/*端口快到65535，再从头开始*/
		if (g_usCurrUseMediaGateStreamPort >= MEDIA_GATE_MAX_USE_PORT)
		{
			g_usCurrUseMediaGateStreamPort = g_usMediaGateStreamStartPort;
			g_usCurrUseProxyStreamPort = g_iProxyStreamPort;
		}
	
		int ret = pPuller->initStreamPuller(pszStreamId, g_strLocalMediaIP, g_usCurrUseMediaGateStreamPort, 
			fnStreamCB, pParam);
		if (0 != ret)
		{
			WARN_LOG_MEDIAGATE("init stream puller failed, use port %d, will try again.\r\n", g_usCurrUseMediaGateStreamPort);
			pPuller->releaseStreamPuller();
			g_usCurrUseMediaGateStreamPort += 2;
			g_usCurrUseProxyStreamPort += 2;
			continue;
		}

		/*初始化成功，break*/
		if (1 == g_iProxyEnable)
		{
			iLocalPort = g_usCurrUseProxyStreamPort;
		}
		else
		{
			iLocalPort = g_usCurrUseMediaGateStreamPort;
		}
		g_usCurrUseMediaGateStreamPort += 2;
		g_usCurrUseProxyStreamPort += 2;
		break;
	}

	/*三次都失败*/
	if (iTry == iCount)
	{
		printf("init stream puller failed, try %d count.\r\n", iTry);
		delete pPuller;
		//EXIT_FUNC_MEDIAGATE();
		return NULL;
	}

	/*subject头域：“媒体发送者ID：发送方媒体流序列号，媒体流接受者ID：接收方媒体流序列号”*/
	char szSubject[1024] = {0};
	sprintf(szSubject, "%s:%s,%s:%s", pszDestId, "0", g_strLocalSipId.c_str(), "0");
		
	char szBody[1024] = {0};
	const char *pszLocalIP = g_strLocalMediaIP.c_str();
	int iBodyLen = 0;
	if (0 == iPlaybackFlag) //实时视频
	{
		iBodyLen = sprintf(szBody, 			
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=Play\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n"
			"m=video %d RTP/AVP 96 98 97\r\n"
			"a=recvonly\r\n"
			"a=rtpmap:96 PS/90000\r\n"
			"a=rtpmap:97 MPEG4/90000\r\n"
			"a=rtpmap:98 H264/90000\r\n",
			g_strLocalSipId.c_str(), pszLocalIP,
			pszLocalIP,
			iLocalPort
			);
	}
	else //历史视频
	{
		iBodyLen = sprintf(szBody, 			
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=Playback\r\n"
			"u=%s:0\r\n"
			"c=IN IP4 %s\r\n"
			"t=%lld %lld\r\n"
			"m=video %d RTP/AVP 96 98 97\r\n"
			"a=recvonly\r\n"
			"a=rtpmap:96 PS/90000\r\n"
			"a=rtpmap:97 MPEG4/90000\r\n"
			"a=rtpmap:98 H264/90000\r\n",
			g_strLocalSipId.c_str(), pszLocalIP, //o=
			pszDestId, //u=
			pszLocalIP, //c=
			i64StartTime, i64StopTime, //t=
			iLocalPort //m=
			);
	}

	int cid = SipUA_InitInviteWidthBody(g_strLocalSipId.c_str(), pszDestId, pszDestIP, iDestPort,
		szSubject, szBody, iBodyLen);
	if (cid <= 0)
	{
        printf("init invite failed, cid=%d.\n", cid);
		delete pPuller;
		//EXIT_FUNC_MEDIAGATE();
		return NULL;
	}
	
	pPuller->setCID(cid);
	g_mapGB28181StreamInstance.insert(std::pair<int, CGB28181StreamInstance*>(cid, pPuller));

	HMG hPuller = (HMG)pPuller;
	//EXIT_FUNC_MEDIAGATE();
	return hPuller;
}

/*删除拉流句柄*/
int GB28181MediaGate_DeleteStreamPuller(HMG hPuller)
{
	ENTER_FUNC_MEDIAGATE();
	if (NULL == hPuller)
		return -1;
	
	CGB28181StreamPuller *pPuller = (CGB28181StreamPuller *)hPuller;
	/*有media gate内部自动释放*/
	if (true == pPuller->getAutoRelease())
	{
		EXIT_FUNC_MEDIAGATE();
		return 0;
	}

	/*先发BYE*/
	int cid = pPuller->getCID();
	int did = pPuller->getDID();
	SipUA_TerminateCall(cid, did);
	eraseStreamInstance(cid);	
	
	EXIT_FUNC_MEDIAGATE();
	return 0;
}




