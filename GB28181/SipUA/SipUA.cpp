#include "SipUA.h"
#include <eXosip2/eXosip.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <vector>
#include "UaComm.h"

//#define INTELLIF_HEAD_NAME	"Intellif"

fnSipUAEventCB g_fnEventCB = NULL;
void *g_pEventCBParam = NULL;
/*线程监听osip事件*/
bool g_bEventThreadWorking = false;
sem_t g_semWaitThreadExit;

#define UA_STRING "VTDU"

typedef struct tagSEndpointAddr
{
	std::string strIP;
	int port;
}SEndpointAddr;

SEndpointAddr g_localAddr;

std::string g_strSipEventType[EXOSIP_EVENT_COUNT+1] = 
{
	"EXOSIP_REGISTRATION_NEW",           /**< announce new registration.       */
    "EXOSIP_REGISTRATION_SUCCESS",       /**< user is successfully registred.  */
    "EXOSIP_REGISTRATION_FAILURE",       /**< user is not registred.           */
    "EXOSIP_REGISTRATION_REFRESHED",     /**< registration has been refreshed. */
    "EXOSIP_REGISTRATION_TERMINATED",    /**< UA is not registred any more.    */
    "EXOSIP_CALL_INVITE",            /**< announce a new call                   */
    "EXOSIP_CALL_REINVITE",          /**< announce a new INVITE within call     */
    "EXOSIP_CALL_NOANSWER",          /**< announce no answer within the timeout */
    "EXOSIP_CALL_PROCEEDING",        /**< announce processing by a remote app   */
    "EXOSIP_CALL_RINGING",           /**< announce ringback                     */
    "EXOSIP_CALL_ANSWERED",          /**< announce start of call                */
    "EXOSIP_CALL_REDIRECTED",        /**< announce a redirection                */
    "EXOSIP_CALL_REQUESTFAILURE",    /**< announce a request failure            */
    "EXOSIP_CALL_SERVERFAILURE",     /**< announce a server failure             */
    "EXOSIP_CALL_GLOBALFAILURE",     /**< announce a global failure             */
    "EXOSIP_CALL_ACK",               /**< ACK received for 200ok to INVITE      */
    "EXOSIP_CALL_CANCELLED",         /**< announce that call has been cancelled */
    "EXOSIP_CALL_TIMEOUT",           /**< announce that call has failed         */
    "EXOSIP_CALL_MESSAGE_NEW",              /**< announce new incoming request. */
    "EXOSIP_CALL_MESSAGE_PROCEEDING",       /**< announce a 1xx for request. */
    "EXOSIP_CALL_MESSAGE_ANSWERED",         /**< announce a 200ok  */
    "EXOSIP_CALL_MESSAGE_REDIRECTED",       /**< announce a failure. */
    "EXOSIP_CALL_MESSAGE_REQUESTFAILURE",   /**< announce a failure. */
    "EXOSIP_CALL_MESSAGE_SERVERFAILURE",    /**< announce a failure. */
    "EXOSIP_CALL_MESSAGE_GLOBALFAILURE",    /**< announce a failure. */
    "EXOSIP_CALL_CLOSED",            /**< a BYE was received for this call      */
    "EXOSIP_CALL_RELEASED",             /**< call context is cleared.            */
    "EXOSIP_MESSAGE_NEW",              /**< announce new incoming request. */
    "EXOSIP_MESSAGE_PROCEEDING",       /**< announce a 1xx for request. */
    "EXOSIP_MESSAGE_ANSWERED",         /**< announce a 200ok  */
    "EXOSIP_MESSAGE_REDIRECTED",       /**< announce a failure. */
    "EXOSIP_MESSAGE_REQUESTFAILURE",   /**< announce a failure. */
    "EXOSIP_MESSAGE_SERVERFAILURE",    /**< announce a failure. */
    "EXOSIP_MESSAGE_GLOBALFAILURE",    /**< announce a failure. */
    "EXOSIP_SUBSCRIPTION_UPDATE",         /**< announce incoming SUBSCRIBE.      */
    "EXOSIP_SUBSCRIPTION_CLOSED",         /**< announce end of subscription.     */
    "EXOSIP_SUBSCRIPTION_NOANSWER",          /**< announce no answer              */
    "EXOSIP_SUBSCRIPTION_PROCEEDING",        /**< announce a 1xx                  */
    "EXOSIP_SUBSCRIPTION_ANSWERED",          /**< announce a 200ok                */
    "EXOSIP_SUBSCRIPTION_REDIRECTED",        /**< announce a redirection          */
    "EXOSIP_SUBSCRIPTION_REQUESTFAILURE",    /**< announce a request failure      */
    "EXOSIP_SUBSCRIPTION_SERVERFAILURE",     /**< announce a server failure       */
    "EXOSIP_SUBSCRIPTION_GLOBALFAILURE",     /**< announce a global failure       */
    "EXOSIP_SUBSCRIPTION_NOTIFY",            /**< announce new NOTIFY request     */
    "EXOSIP_SUBSCRIPTION_RELEASED",          /**< call context is cleared.        */
    "EXOSIP_IN_SUBSCRIPTION_NEW",            /**< announce new incoming SUBSCRIBE.*/
    "EXOSIP_IN_SUBSCRIPTION_RELEASED",       /**< announce end of subscription.   */
    "EXOSIP_NOTIFICATION_NOANSWER",          /**< announce no answer              */
    "EXOSIP_NOTIFICATION_PROCEEDING",        /**< announce a 1xx                  */
    "EXOSIP_NOTIFICATION_ANSWERED",          /**< announce a 200ok                */
    "EXOSIP_NOTIFICATION_REDIRECTED",        /**< announce a redirection          */
    "EXOSIP_NOTIFICATION_REQUESTFAILURE",    /**< announce a request failure      */
    "EXOSIP_NOTIFICATION_SERVERFAILURE",     /**< announce a server failure       */
    "EXOSIP_NOTIFICATION_GLOBALFAILURE",     /**< announce a global failure       */
    "EXOSIP_EVENT_COUNT"                  /**< MAX number of events              */
};


/*--------------------------------
begin:   SDP内容解析函数
---------------------------------*/

/*strcat一个sdp行*/
#define STRCAT_SDP_LINE(str, szLineName, szValue) \
{\
	if(strlen(szValue)) \
	{\
		char szSdpLine[SIPUA_STRING_LINE_LEN] = {0};\
		sprintf(szSdpLine, "%s=%s\r\n", szLineName, szValue);\
		strcat(str, szSdpLine);\
	}\
}

/*切割字符串*/
static void splitString(const std::string& s, std::vector<std::string>& v, const std::string& c)
{
	std::string::size_type pos1, pos2;
	pos2 = s.find(c);
	pos1 = 0;
	while(std::string::npos != pos2)
	{
		v.push_back(s.substr(pos1, pos2-pos1));

		pos1 = pos2 + c.size();
		pos2 = s.find(c, pos1);
	}
	
	if(pos1 != s.length())
		v.push_back(s.substr(pos1));
}


/*获取一行内容v=xxxx
@pszInBuff: 输入字符串
@pszLineName: 查找的行名
@pszOutContent, pOutContentLen: 返回内容及长度
返回值: 整行的长度
*/
static int getLineContent(char *pszInBuff, const char *pszLineName, 
	char *pszOutContent, int *pOutContentLen, int iMaxContentLen)
{
	if (NULL == pszInBuff || iMaxContentLen <= 0 || NULL == pszLineName || 
		NULL == pszOutContent || NULL == pOutContentLen)
	{
		WARN_LOG_SIPUA("getLineContent failed, param error.");
		return -1;
	}

	*pOutContentLen = 0;
	char *pPos = strstr(pszInBuff, pszLineName);
	if (NULL == pPos)
		return -1;

	int iLineNameLen = strlen(pszLineName);
	pPos += iLineNameLen;
	
	int iContentLen = 0;
	while (*pPos)
	{
		if ('\r' == *pPos || '\n' == *pPos || '\0' == *pPos)
			break;

		pszOutContent[iContentLen] = *pPos;
		iContentLen++;
		if (iContentLen >= iMaxContentLen)
		{	
			WARN_LOG_SIPUA("content len[%d] out of range[%d].", iContentLen, iMaxContentLen);
			return -1;
		}
		pPos++;
	}
	/*返回长度*/
	*pOutContentLen = iContentLen;
	int ret = iLineNameLen + iContentLen;
	return ret;
}

/*从SDP字符串中，单独找出m=video, m=audio的内容
Media description, if present
         m=  (media name and transport address)
         i=* (media title)
         c=* (connection information - optional if included at
              session-level)
         b=* (zero or more bandwidth information lines)
         k=* (encryption key)
         a=* (zero or more media attribute lines)

以上带"*"号的是可选的,其余的是必须的。一般顺序也按照上面的顺序来排列。
*/
static int parseMediaSdpBody(SMediaInfo *pMediaInfo, char *pszInBuff, int iMaxInLen, const char *pszMediaType)
{
	if (NULL == pMediaInfo || NULL == pszInBuff || NULL == pszMediaType || iMaxInLen <= 0)
	{
		WARN_LOG_SIPUA("getMediaSdpBody failed, param error.");
		return -1;
	}

	pMediaInfo->ucUseFlag = 0;

	char *pPos = strstr(pszInBuff, pszMediaType);
	if (NULL == pPos)
		return -1;
	
	int iPosLen = (int)(pPos - pszInBuff);
	if (iPosLen <= 0)
		return -1;

	char szLine_m[SIPUA_STRING_LINE_LEN] = {0};
	int iLineLen = 0;
	int iContentLen = 0;
	iLineLen = getLineContent(pPos, "m=", szLine_m, &iContentLen, SIPUA_STRING_LINE_LEN);
	if (iLineLen <= 0)
		return -1;
	
	//video 6000 RTP/AVP 96 98 97
	std::vector<std::string> vcMLine;
	splitString(szLine_m, vcMLine, " ");
	int iSize = vcMLine.size();
	if (iSize <= 3)
	{
		WARN_LOG_SIPUA("media %s line error, size<=3", pszMediaType);
		return -1;
	}

	pMediaInfo->port = atoi(vcMLine[1].c_str());
	strcpy(pMediaInfo->szProto, vcMLine[2].c_str());
	int iAttrCount = iSize - 3;
	pMediaInfo->iAttrCount = iAttrCount;
	int j = 3;
	for(int i = 0; i < iAttrCount; i++)
	{
		if (i < SDP_MEDIA_ATTR_MAX_COUNT)
		{
			pMediaInfo->sAttr[i].ePayloadType = (EPayloadType)atoi(vcMLine[j].c_str());
		}
		
		j++;
		if (j >= iSize)
			break;
	}
	
	/*开始解析a行*/
	pPos += iLineLen;
	iPosLen += iLineLen;
	int iAttrIndex = 0;
	while (*pPos)
	{
		char szAttrLine[SIPUA_STRING_LINE_LEN] = {0};
		iLineLen = getLineContent(pPos, "a=", szAttrLine, &iContentLen, SIPUA_STRING_LINE_LEN);
		if (iLineLen > 0)
		{
			pPos += iLineLen;
			iPosLen += iLineLen;
			if (strncmp(szAttrLine, "sendonly", iContentLen) == 0)
			{
				pMediaInfo->eDir = TRANS_DIR_SENDONLY;
			}
			else if (strncmp(szAttrLine, "recvonly", iContentLen) == 0)
			{
				pMediaInfo->eDir = TRANS_DIR_RECVONLY;
			}
			else if (strncmp(szAttrLine, "sendrecv", iContentLen) == 0)
			{
				pMediaInfo->eDir = TRANS_DIR_SENDRECV;
			}
			else if (strncmp(szAttrLine, "inactive", iContentLen) == 0)
			{
				pMediaInfo->eDir = TRANS_DIR_INACTIVE;
			}
			else 
			{
				//a=rtpmap:96 PS/90000
				if (iAttrIndex < SDP_MEDIA_ATTR_MAX_COUNT)
				{
					strcpy(pMediaInfo->sAttr[iAttrIndex].szAttrLine, szAttrLine);
				}
				iAttrIndex++;
			}
		}
		else
		{
			pPos++;
			iPosLen++;
		}

		if (iPosLen >= iMaxInLen)
			break;		
	}

	pMediaInfo->ucUseFlag = 1;
	
	return 0;
}

/*SDP字符串内容解析出结构体
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
static int parseSdpBody(char *pszBody, int iBodyLen, SSdpInfo *pSdpInfo)
{
	if (iBodyLen <= 0 || NULL == pszBody)
		return -1;

	int iContentLen = 0;
	getLineContent(pszBody, "v=", pSdpInfo->szVersion, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "o=", pSdpInfo->szOrigin, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "s=", pSdpInfo->szSessionName, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "u=", pSdpInfo->szUri, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "c=", pSdpInfo->szConnection, &iContentLen, SIPUA_STRING_LINE_LEN);
	char szTime[SIPUA_STRING_LINE_LEN] = {0};
	getLineContent(pszBody, "t=", szTime, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "y=", pSdpInfo->szY, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "f=", pSdpInfo->szF, &iContentLen, SIPUA_STRING_LINE_LEN);
	getLineContent(pszBody, "x=", pSdpInfo->szX, &iContentLen, SDP_X_LINE_STRING_LEN);
	parseMediaSdpBody(&pSdpInfo->sVideoMediaInfo, pszBody, iBodyLen, "m=video");
	parseMediaSdpBody(&pSdpInfo->sAudioMediaInfo, pszBody, iBodyLen, "m=audio");
	/*从c行c=IN IP4 192.168.3.81解析出地址，m=video/audio内的c行暂时忽略*/
	std::vector<std::string> vcLine_c;
	splitString(pSdpInfo->szConnection, vcLine_c, " ");
	if (vcLine_c.size() >= 3)
	{
		strcpy(pSdpInfo->sVideoMediaInfo.szAddr, vcLine_c[2].c_str());
		strcpy(pSdpInfo->sAudioMediaInfo.szAddr, vcLine_c[2].c_str());
	}

	std::vector<std::string> vcLine_t;
	splitString(szTime, vcLine_t, " ");
	if (vcLine_t.size() >= 2)
	{
		pSdpInfo->i64TimeBegin = atoll(vcLine_t[0].c_str());
		pSdpInfo->i64TimeEnd = atoll(vcLine_t[1].c_str());
	}
	
	return 0;
}

/*gb28181规定，
subject头域：“媒体发送者ID：发送方媒体流序列号，媒体流接受者ID：接收方媒体流序列号”
*/
static int parseSubjectHeader(SSubjectInfo *pSubjectInfo, char *pszSubject)
{
	if (NULL == pSubjectInfo || NULL == pszSubject)
		return -1;

	//51342405001320000010:04,34020000002000000001:04
	std::vector<std::string> vcStreamInfo;
	splitString(pszSubject, vcStreamInfo, ",");
	if (vcStreamInfo.size() != 2)
		return -1;

	//
	std::vector<std::string> vcSender;
	splitString(vcStreamInfo[0], vcSender, ":");

	//	
	std::vector<std::string> vcRecver;
	splitString(vcStreamInfo[1], vcRecver, ":");

	if (2 == vcSender.size() && 2 == vcRecver.size())
	{
		strcpy(pSubjectInfo->szSenderId, vcSender[0].c_str());
		strcpy(pSubjectInfo->szSenderStreamSeq, vcSender[1].c_str());
		
		strcpy(pSubjectInfo->szRecverId, vcRecver[0].c_str());
		strcpy(pSubjectInfo->szRecverStreamSeq, vcRecver[1].c_str());
	}

	return 0;
}

/*SMediaInfo结构体转成m=video字符串*/
static int mediaInfo2String(SMediaInfo *pMediaInfo, const char *pszStreamType, char *pszOutMediaStr)
{
	if (NULL == pMediaInfo || NULL == pszStreamType || NULL == pszOutMediaStr)
		return -1;
	
	/*
	m=video 6000 RTP/AVP 96 98 97
	a=recvonly
	a=rtpmap:96 PS/90000
	a=rtpmap:98 H264/90000
	a=rtpmap:97 MPEG4/90000
	*/
	char szMLine[256] = {0};
	sprintf(szMLine, "m=%s %d %s ", pszStreamType, pMediaInfo->port, pMediaInfo->szProto);
	char szAttrList[256] = {0};
	char szAttrs[1024] = {0};
	for (int i = 0; i < pMediaInfo->iAttrCount; i++)
	{
		char szMediaType[128] = {0};
		sprintf(szMediaType, "%d", (int)pMediaInfo->sAttr[i].ePayloadType);
		strcat(szAttrList, szMediaType);
		/*96 98 97 最后一个类型不需要空格,用换行符*/
		if (i == (pMediaInfo->iAttrCount - 1))
		{
			strcat(szAttrList, "\r\n");
		}
		else
		{
			strcat(szAttrList, " ");
		}
		
		char szALine[SIPUA_STRING_LINE_LEN] = {0};
		sprintf(szALine, "a=%s\r\n", pMediaInfo->sAttr[i].szAttrLine);
		strcat(szAttrs, szALine);
	}
	strcat(szMLine, szAttrList);

	//sendonly/recvonly/sendrecv/inative
	char szDir[256] = {0};
	switch(pMediaInfo->eDir)
	{
	case TRANS_DIR_SENDONLY:
		sprintf(szDir, "a=sendonly\r\n");
	break;

	case TRANS_DIR_RECVONLY:
		sprintf(szDir, "a=recvonly\r\n");
	break;

	case TRANS_DIR_SENDRECV:
		sprintf(szDir, "a=sendrecv\r\n");
	break;

	case TRANS_DIR_INACTIVE:
		sprintf(szDir, "a=inactive\r\n");
	break;
		
	default:
	break;
	}

	int ret = sprintf(pszOutMediaStr, "%s%s%s", szMLine, szDir, szAttrs);
	
	return ret;
}

/*sdp结构体转换成body字符串*/
static int sdpInfo2BodyString(SSdpInfo *pSdpInfo, char *pszOutBody)
{
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
	if (NULL == pSdpInfo)
		return 0;

	std::string strMediaLine = "";
	//m=video
	if (1 == pSdpInfo->sVideoMediaInfo.ucUseFlag)
	{
		char szMediaStr[1024] = {0};
		mediaInfo2String(&pSdpInfo->sVideoMediaInfo, "video", szMediaStr);
		strMediaLine += szMediaStr;
	}
	
	//m=audio
	if (1 == pSdpInfo->sAudioMediaInfo.ucUseFlag)
	{
		char szMediaStr[1024] = {0};
		mediaInfo2String(&pSdpInfo->sVideoMediaInfo, "audio", szMediaStr);
		strMediaLine += szMediaStr;
	}

	int ret = 0;

	STRCAT_SDP_LINE(pszOutBody, "v", pSdpInfo->szVersion);
	STRCAT_SDP_LINE(pszOutBody, "o", pSdpInfo->szOrigin);
	STRCAT_SDP_LINE(pszOutBody, "s", pSdpInfo->szSessionName);
	STRCAT_SDP_LINE(pszOutBody, "u", pSdpInfo->szUri);
	STRCAT_SDP_LINE(pszOutBody, "c", pSdpInfo->szConnection);
	char szTimeLine[SIPUA_STRING_LINE_LEN] = {0};
	sprintf(szTimeLine, "%lld %lld", pSdpInfo->i64TimeBegin, pSdpInfo->i64TimeEnd);
	STRCAT_SDP_LINE(pszOutBody, "t", szTimeLine);
	strcat(pszOutBody, strMediaLine.c_str()); //media信息		
	STRCAT_SDP_LINE(pszOutBody, "y", pSdpInfo->szY);
	STRCAT_SDP_LINE(pszOutBody, "f", pSdpInfo->szF);
	STRCAT_SDP_LINE(pszOutBody, "x", pSdpInfo->szX);
	ret = strlen(pszOutBody);

#if 0
	if (strlen(pSdpInfo->szX) > 0)
	{		
		ret = sprintf(pszOutBody, 
			"v=%s\r\n"
			"o=%s\r\n"
			"s=%s\r\n"
			"c=%s\r\n"
			"t=%lld %lld\r\n"
			"%s"
			"y=%s\r\n"
			"f=%s\r\n"
			"x=%s\r\n",
			pSdpInfo->szVersion,
			pSdpInfo->szOrigin,
			pSdpInfo->szSessionName,
			pSdpInfo->szConnection,
			pSdpInfo->i64TimeBegin, pSdpInfo->i64TimeEnd,
			strMediaLine.c_str(),
			pSdpInfo->szY,
			pSdpInfo->szF,
			pSdpInfo->szX
			);
	}
	else
	{
		ret = sprintf(pszOutBody, 
			"v=%s\r\n"
			"o=%s\r\n"
			"s=%s\r\n"
			"c=%s\r\n"
			"t=%lld %lld\r\n"
			"%s"
			"y=%s\r\n"
			"f=%s\r\n",
			pSdpInfo->szVersion,
			pSdpInfo->szOrigin,
			pSdpInfo->szSessionName,
			pSdpInfo->szConnection,
			pSdpInfo->i64TimeBegin, pSdpInfo->i64TimeEnd,
			strMediaLine.c_str(),
			pSdpInfo->szY,
			pSdpInfo->szF
			);
	}
#endif
	
	return ret;
}

/*subject结构体转字符串
subject头域：“媒体发送者ID：发送方媒体流序列号，媒体流接受者ID：接收方媒体流序列号”
*/
int subjectInfo2String(SSubjectInfo *pSubjectInfo, char *pszOutSubject)
{
	int ret = 0;
	if (NULL == pSubjectInfo)
		return 0;

	ret = sprintf(pszOutSubject, "%s:%s,%s:%s", 
		pSubjectInfo->szSenderId, pSubjectInfo->szSenderStreamSeq,
		pSubjectInfo->szRecverId, pSubjectInfo->szRecverStreamSeq);

	return ret;
}

/*从SIP消息内容获取SDP信息*/
int getSdpInfo(osip_message_t *pMsg, SSdpInfo *pOutSdpInfo)
{
	if (NULL == pMsg || NULL == pOutSdpInfo)
		return -1;	

	memset(pOutSdpInfo, 0, sizeof(SSdpInfo));
	char *pszBody = NULL;
	int iBodyLen = 0;
	osip_body_t *body = NULL;
	osip_message_get_body(pMsg, 0, &body);
	if (body)
	{
		pszBody = body->body;
		iBodyLen = (int)body->length;
		return parseSdpBody(pszBody, iBodyLen, pOutSdpInfo);			
	}

	return -1;
}

/*从SIP消息内容获取SDP信息*/
int getSubject(osip_message_t *pMsg, SSubjectInfo *pOutSubject)
{
	if (NULL == pMsg || NULL == pOutSubject)
		return -1;	

	memset(pOutSubject, 0, sizeof(SSubjectInfo));
	osip_header_t *subject_header;
	subject_header = NULL;
	osip_message_get_subject(pMsg, 0, &subject_header);
	if (subject_header)
	{			
		return parseSubjectHeader(pOutSubject, subject_header->hvalue);
	}

	return -1;
}

/*--------------------------------
end:   SDP内容解析函数
---------------------------------*/



 
static void *threadEventLoop(void *pParam);


/*--------------------------------
begin:   SIP消息解析函数
---------------------------------*/

/*回调呼叫类消息*/
static void callbackCallMsg(eXosip_event_t *evt, ESipUAMsg msg)
{
	if (g_fnEventCB)
	{	
		g_fnEventCB(msg, (void*)evt, g_pEventCBParam);		
	}
}

/*--------------------------------
end:   SIP消息解析函数
---------------------------------*/


static void *threadEventLoop(void *pParam)
{		
	while (g_bEventThreadWorking)
	{
		eXosip_event_t *evt = NULL;
		if (!(evt = eXosip_event_wait(0, 50))) {
			osip_usleep(10000);
			continue;
		}

		if (evt->type >= 0 && evt->type <= EXOSIP_EVENT_COUNT)
		{
			DEBUG_LOG_SIPUA("recv sip msg=%s, tid=%d, did=%d, rid=%d, cid=%d, sid=%d, nid=%d", g_strSipEventType[evt->type].c_str(),
				evt->tid, evt->did, evt->rid, evt->cid, evt->sid, evt->nid);
		}
		else
		{
			WARN_LOG_SIPUA("recv sip msg=%d, out of range.", evt->type);
		}

		eXosip_lock();
        //test 自动处理401
		//eXosip_default_action(evt);
		eXosip_automatic_refresh();
		eXosip_unlock();

		switch (evt->type)
		{
			/*注册类消息*/
			case EXOSIP_REGISTRATION_NEW:
			break;

			case EXOSIP_REGISTRATION_SUCCESS:
                printf("EXOSIP_REGISTRATION_SUCCESS\n");
				callbackCallMsg(evt, SIPUA_MSG_REGISTER_SUCCESS);
			break;

			case EXOSIP_REGISTRATION_FAILURE:
                printf("EXOSIP_REGISTRATION_FAILURE\n");
				callbackCallMsg(evt, SIPUA_MSG_REGISTER_FAILURE);
			break;

			case EXOSIP_REGISTRATION_REFRESHED:
				callbackCallMsg(evt, SIPUA_MSG_REGISTER_REFRESH);
			break;

			/*呼叫类消息*/
			case EXOSIP_CALL_INVITE: //invite
				callbackCallMsg(evt, SIPUA_MSG_CALL_INVITE_INCOMING);
			break;

			case EXOSIP_CALL_ANSWERED: //invite响应
			case EXOSIP_CALL_REDIRECTED:
			case EXOSIP_CALL_REQUESTFAILURE:
			case EXOSIP_CALL_SERVERFAILURE:
			case EXOSIP_CALL_GLOBALFAILURE:
				if (200 == evt->response->status_code)
				{
					callbackCallMsg(evt, SIPUA_MSG_CALL_ANSWER_200OK);
				}
				else
				{
					WARN_LOG_SIPUA("recv invite response, code=%d", evt->response->status_code);
					callbackCallMsg(evt, SIPUA_MSG_CALL_ANSWER_RSP);
				}
			break;

			case EXOSIP_CALL_ACK: //ack
				callbackCallMsg(evt, SIPUA_MSG_CALL_ACK);
			break;

			case EXOSIP_CALL_CLOSED: //呼叫关闭
				callbackCallMsg(evt, SIPUA_MSG_CALL_BYE);
			break;
			
			/*MESSAGE类消息*/
			case EXOSIP_MESSAGE_NEW:
				if (MSG_IS_REGISTER(evt->request))  /*注册消息*/
				{
					callbackCallMsg(evt, SIPUA_MSG_REGISTER_NEW);
				}
				else if(MSG_IS_MESSAGE(evt->request))
				{
					callbackCallMsg(evt, SIPUA_MSG_MESSAGE);
				}
                else if (MSG_IS_INFO(evt->request))
                {
                    callbackCallMsg(evt, SIPUA_MSG_INFO);
                }
			break;

			case EXOSIP_MESSAGE_ANSWERED: //MESSAGE 200 ok
				callbackCallMsg(evt, SIPUA_MSG_MESSAGE_ANSWERED);
			break;

			case EXOSIP_MESSAGE_REQUESTFAILURE: //MESSAGE 400响应
				callbackCallMsg(evt, SIPUA_MSG_MESSAGE_REQUESTFAILURE);
			break;
			

			/*订阅类消息*/
			case EXOSIP_SUBSCRIPTION_UPDATE:
			break;

			default:
			break;

		}		

		eXosip_event_free(evt);
	}

	sem_post(&g_semWaitThreadExit);

	return NULL;
}


/*初始化SIPUA
@iListenPort: sipua真正使用的本地地址
@pszSrcAddr, iSrcPort: SIP信令的源地址，如果要上云，使用云端地址
返回值: 成功返回0
*/
int SipUA_Init(int transport, int iListenPort, const char *pszSrcAddr, int iSrcPort, 
	fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB)
{
	uaLogInit(fnLogCB, pParamLogCB);

	ENTER_FUNC_SIPUA();
	
	int ret = eXosip_init();
	if (0 != ret)
	{
		ERR_LOG_SIPUA("eXosip_init ret=%d", ret);
		EXIT_FUNC_SIPUA();
		return -1;
	}	

	eXosip_set_user_agent(UA_STRING);

	int proto = IPPROTO_UDP;
	if (SIPUA_TANSPORT_TCP == transport)
	{
		proto = IPPROTO_TCP;
	}
	
	ret = eXosip_listen_addr(proto, NULL, iListenPort, AF_INET, 0);
	if (0 != ret)
	{
		ERR_LOG_SIPUA("eXosip_listen_addr ret=%d, proto=%d, port=%d", ret, proto, iListenPort);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	if (pszSrcAddr)
		g_localAddr.strIP = pszSrcAddr;
	
	g_localAddr.port = iSrcPort;

	DEBUG_LOG_SIPUA("sipua inited, listen[%d], local addr[%s:%d]", iListenPort, g_localAddr.strIP.c_str(), g_localAddr.port);

	EXIT_FUNC_SIPUA();
	return 0;
}

/*释放SIPUA
返回值: 成功返回0
*/
int SipUA_Release()
{
	ENTER_FUNC_SIPUA();
	eXosip_quit();
	EXIT_FUNC_SIPUA();
	return 0;
}

/*开始SIPUA事件
返回值: 成功返回0
*/
int SipUA_StartEventLoop(fnSipUAEventCB fnCB, void *pParam)
{
	ENTER_FUNC_SIPUA();
	g_fnEventCB = fnCB;
	g_pEventCBParam = pParam;

	sem_init(&g_semWaitThreadExit, 0, 0);
	g_bEventThreadWorking = true;
	pthread_t tThreadId;
	pthread_create(&tThreadId, 0, threadEventLoop, NULL);	
	EXIT_FUNC_SIPUA();
	return 0;
}

/*结束SIPUA
返回值: 成功返回0
*/
int SipUA_StopEventLoop()
{
	ENTER_FUNC_SIPUA();
	if (true == g_bEventThreadWorking)
	{
		g_bEventThreadWorking = false;
		sem_wait(&g_semWaitThreadExit);
		sem_destroy(&g_semWaitThreadExit);
	}
	EXIT_FUNC_SIPUA();
	return 0;
}



/*使用鉴权方式注册到SIP服务器
@pszSipId: 注册的SIP编号
@pszUser, pszPwd: 注册用户名，密码
@expires: 超时时间
@pszServerId: 服务器SIP编号
@pszServerIP, iServerPort: 服务器地址
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterAuthor(
	const char *pszSipId, const char *pszUser, const char *pszPwd, int expires,
	const char *pszServerId, const char *pszServerIP, int iServerPort,
	const char *pszBody, int iBodyLen)
{
	/*const char *user = "34020100002000000001";
	const char *pwd = "12345678";
	const char *fromuser = "sip:34020100002000000001@192.168.2.5:5070";
	const char *proxy = "sip:34020000002000000001@192.168.31.122:5060";*/

	ENTER_FUNC_SIPUA();
	
	int ret = 0;
	char fromuser[256] = {0};
	sprintf(fromuser, "sip:%s@%s:%d", pszSipId, g_localAddr.strIP.c_str(), g_localAddr.port);
	char proxy[256] = {0};
	sprintf(proxy, "sip:%s@%s:%d", pszServerId, pszServerIP, iServerPort);
	const char *contact = NULL;
	osip_message_t *reg = NULL;

	//初始register消息
	eXosip_clear_authentication_info();
	eXosip_add_authentication_info(pszUser, pszUser, pszPwd, "MD5", NULL);
	int regid =	eXosip_register_build_initial_register(fromuser, proxy, contact, expires, &reg);
	if (regid < 1) 
	{
		WARN_LOG_SIPUA("init register failed, from[%s], proxy[%s]", fromuser, proxy);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	//设置消息体
	if (pszBody && iBodyLen > 0)
	{
		ret = osip_message_set_body(reg, pszBody, iBodyLen);
		osip_message_set_content_type(reg, "application/json");
		if (0 != ret)
		{
			WARN_LOG_SIPUA("set body failed, bodylen[%d] [%s].", iBodyLen, pszBody);
			EXIT_FUNC_SIPUA();
			return -1;
		}
	}

	//发送register消息
    reg->v3_head = NULL;
	ret = eXosip_register_send_register(regid, reg);
	if (ret != 0) 
	{
		WARN_LOG_SIPUA("send register failed, from[%s], proxy[%s]", fromuser, proxy);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	EXIT_FUNC_SIPUA();
	return regid;
}

/*使用非鉴权方式注册到SIP服务器
参数参考SipUA_RegisterAuthor
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterUnauthorV3(
    const char *pszSipId, const char * pszSipRegion, const char * pszSipIp, int iSipPort, int expires,
    const char *pszServerId, const char * pszServerRegion, const char *pszServerIP, int iServerPort,
    const char *pszBody, int iBodyLen)
{
    ENTER_FUNC_SIPUA();

    int ret = 0;
    char fromuser[256] = { 0 };
    sprintf(fromuser, "sip:%s@%s:%d", pszSipId, pszSipIp, iSipPort);
    char proxy[256] = { 0 };
    sprintf(proxy, "sip:%s@%s:%d", pszServerId, pszServerIP, iServerPort);
    char contact[256] = { 0 };//sip:122205106000000001@122205:5063
    sprintf(contact, "sip:%s@%s:%d", pszSipId, pszSipRegion, iSipPort);
    //const char *contact = NULL; 
    osip_message_t *reg = NULL;

    int regid = eXosip_register_build_initial_register(fromuser, proxy, contact, expires, &reg);
    if (regid < 1)
    {
        WARN_LOG_SIPUA("init register failed, from[%s], proxy[%s], contact[%s], ret[%d]", fromuser, proxy, contact, regid);
        EXIT_FUNC_SIPUA();
        return -1;
    }
 
    DEBUG_LOG_SIPUA("init register success, from[%s], proxy[%s]", fromuser, proxy);

    //设置消息体
    if (pszBody && iBodyLen > 0)
    {
        ret = osip_message_set_body(reg, pszBody, iBodyLen);
        osip_message_set_content_type(reg, "txt/xml");
        if (0 != ret)
        {
            WARN_LOG_SIPUA("set body failed, bodylen[%d] [%s].", iBodyLen, pszBody);
            EXIT_FUNC_SIPUA();
            return -1;
        }
    }

    //test
    char *szV3_head = new char[30];
    memset(szV3_head,0,30);
    sprintf(szV3_head, "%s", ";MSG_TYPE=MSG_VTDU_LOGIN_REQ");
    reg->v3_head = szV3_head;
    ret = eXosip_register_send_register(regid, reg);
    if (ret != 0)
    {
        WARN_LOG_SIPUA("send register failed, from[%s], proxy[%s]", fromuser, proxy);
        EXIT_FUNC_SIPUA();
        return -1;
    }

    EXIT_FUNC_SIPUA();
    return regid;
}

/*使用非鉴权方式注册到SIP服务器
参数参考SipUA_RegisterAuthor
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterUnauthor(
	const char *pszSipId, int expires,
	const char *pszServerId, const char *pszServerIP, int iServerPort,
	const char *pszBody, int iBodyLen)
{
	ENTER_FUNC_SIPUA();
	
	int ret = 0;
	char fromuser[256] = {0};
	sprintf(fromuser, "sip:%s@%s:%d", pszSipId, g_localAddr.strIP.c_str(), g_localAddr.port);
	char proxy[256] = {0};
	sprintf(proxy, "sip:%s@%s:%d", pszServerId, pszServerIP, iServerPort);
	const char *contact = NULL;
	osip_message_t *reg = NULL;	
	
	int regid = eXosip_register_build_initial_register(fromuser, proxy, contact, expires, &reg);
	if (regid < 1) 
	{
		WARN_LOG_SIPUA("init register failed, from[%s], proxy[%s]", fromuser, proxy);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	DEBUG_LOG_SIPUA("init register success, from[%s], proxy[%s]", fromuser, proxy);

	//设置消息体
	if (pszBody && iBodyLen > 0)
	{
		ret = osip_message_set_body(reg, pszBody, iBodyLen);
		osip_message_set_content_type(reg, "application/json");
		if (0 != ret)
		{
			WARN_LOG_SIPUA("set body failed, bodylen[%d] [%s].", iBodyLen, pszBody);
			EXIT_FUNC_SIPUA();
			return -1;
		}
	}
	
    reg->v3_head = NULL;
	ret = eXosip_register_send_register(regid, reg);
	if (ret != 0) 
	{
		WARN_LOG_SIPUA("send register failed, from[%s], proxy[%s]", fromuser, proxy);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	EXIT_FUNC_SIPUA();
	return regid;
}


/*刷新注册超时时间
@regid: 由SipUA_RegisterAuthor/SipUA_RegisterUnauthor返回
@expires: 超时时间
返回值: 成功返回0
*/
int SipUA_RefreshRegister(int regid, int expires)
{
	ENTER_FUNC_SIPUA();
	osip_message_t *reg = NULL;
	int ret = eXosip_register_build_register(regid, expires, &reg);
	if (0 != ret)
	{		
		WARN_LOG_SIPUA("build register failed, ret=%d", ret);
		EXIT_FUNC_SIPUA();
		return -1;
	}
	
    reg->v3_head = NULL;
	ret = eXosip_register_send_register(regid, reg);
	if (0 != ret)
	{		
		WARN_LOG_SIPUA("send register failed, ret=%d", ret);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	EXIT_FUNC_SIPUA();
	return 0;
}

/*回复REGISTER 401未认证的响应
@pMsgPtr: 指向消息内容地址的指针
*/
int SipUA_RegisterReply401Unauthorized(void *pMsgPtr, const char *pszRealm, const char *pszNonce)
{
	ENTER_FUNC_SIPUA();
	
	eXosip_event_t *sipEvent = (eXosip_event_t *)pMsgPtr;	
	osip_message_t * pSRegister = NULL;
    osip_www_authenticate_t * header = NULL;
    osip_www_authenticate_init(&header);
    osip_www_authenticate_set_auth_type(header, osip_strdup("Digest"));
    osip_www_authenticate_set_realm(header, osip_enquote(pszRealm));
    osip_www_authenticate_set_nonce(header, osip_enquote(pszNonce));

    char *pDest = NULL;
    osip_www_authenticate_to_str(header, &pDest);
    int iReturnCode = eXosip_message_build_answer(sipEvent->tid, 401, &pSRegister);
    if (iReturnCode == 0 && pSRegister != NULL)
    {
        osip_message_set_www_authenticate(pSRegister, pDest);
        //osip_message_set_content_type(pSRegister, "Application/MANSCDP+xml");
        pSRegister->v3_head = NULL;
        eXosip_message_send_answer(sipEvent->tid, 401, pSRegister);
    }

    osip_www_authenticate_free(header);
    osip_free(pDest);
	
	EXIT_FUNC_SIPUA();
	return 0;
}

/*回复REGISTER 200*/
int SipUA_RegisterReply200OK(void *pMsgPtr, const char *pszSrcId)
{
	ENTER_FUNC_SIPUA();
	
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	osip_header_t* header = NULL;
	int result = osip_message_get_expires(evt->request, 0, &header);
	if (result < 0)
	{
		WARN_LOG_SIPUA("get expire fail when recv register");
		EXIT_FUNC_SIPUA();
		return -1;
	}
	
	osip_message_t * pSRegister = NULL;
	int iReturnCode = eXosip_message_build_answer(evt->tid, 200, &pSRegister);
	if (iReturnCode == 0 && pSRegister != NULL)
	{
		char szSrc[1024] = {0};
		sprintf(szSrc, "sip:%s@%s:%d", pszSrcId, g_localAddr.strIP.c_str(), g_localAddr.port);
		osip_message_set_header(pSRegister, "Contact", szSrc);
		osip_message_set_header(pSRegister, "Expires", header->hvalue);
        pSRegister->v3_head = NULL;
		eXosip_message_send_answer(evt->tid, 200, pSRegister);
	}

	EXIT_FUNC_SIPUA();
	return 0;
}




/*发起一个新的INVITE，带body字符串
@pszSrcId: 源端ID
@pszDestId: 目的端ID
@pszDestIP, iDestPort: 目的端地址
@pszSubject: 消息头subject
@pszBody, iBodyLen: 消息体
@pszIntellifHead: Intellif头域(内部扩展使用，非国标)
返回值: 成功返回大于0的call_id
*/
int SipUA_InitInviteWidthBody(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
		const char *pszSubject, const char *pszBody, int iBodyLen)
{
	ENTER_FUNC_SIPUA();
	
	osip_message_t *invite = NULL;
	char szSrc[1024] = {0};
	char szDest[1024] = {0};
	sprintf(szSrc, "sip:%s@%s:%d", pszSrcId, g_localAddr.strIP.c_str(), g_localAddr.port);
	sprintf(szDest, "sip:%s@%s:%d", pszDestId, pszDestIP, iDestPort);

	int ret = 0;
	ret = eXosip_call_build_initial_invite(&invite, szDest, szSrc, NULL, pszSubject);
	if (0 != ret)
	{
		WARN_LOG_SIPUA("init invite failed, ret=%d, src[%s] dest[%s] subject[%s].", ret, szSrc, szDest, pszSubject);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	ret = osip_message_set_body(invite, pszBody, iBodyLen);
	if (0 != ret)
	{
		WARN_LOG_SIPUA("set body failed, ret=%d, bodylen[%d] [%s].", ret, iBodyLen, pszBody);
		EXIT_FUNC_SIPUA();
		return -1;
	}	
	osip_message_set_content_type(invite, "application/sdp");

    invite->v3_head = NULL;
	ret = eXosip_call_send_initial_invite(invite); 
	if (ret <= 0)
	{
		WARN_LOG_SIPUA("send invite failed, ret=%d, src[%s] dest[%s] subject[%s].", ret, szSrc, szDest, pszSubject);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	DEBUG_LOG_SIPUA("send invite success, subject: %s, %s-->%s\r\n%s", pszSubject, szSrc, szDest, pszBody);

	//INFO_LOG_SIPUA("send invite ret=%d\r\n", ret);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*发起一个新的INVITE，带sdp结构体
@pszSrcId: 源端ID
@pszDestId: 目的端ID
@pszDestIP, iDestPort: 目的端地址
@pszSubject: 消息头subject
@pszBody, iBodyLen: 消息体
@pszIntellifHead: Intellif头域(内部扩展使用，非国标)
返回值: 成功返回大于0的call_id
*/
int SipUA_InitInviteWidthSdpInfo(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
		SSubjectInfo *pSubjectInfo, SSdpInfo *pSdpInfo)
{
	ENTER_FUNC_SIPUA();
	char szBody[1024] = {0};
	int iBodyLen = sdpInfo2BodyString(pSdpInfo, szBody);
	char szSubject[1024] = {0};
	subjectInfo2String(pSubjectInfo, szSubject);
	int ret = SipUA_InitInviteWidthBody(pszSrcId, pszDestId, pszDestIP, iDestPort, 
		szSubject, szBody, iBodyLen);
	EXIT_FUNC_SIPUA();
	return ret;
}


/*回复INVITE请求200 OK
返回值: 成功返回0
*/
int SipUA_AnswerInvite200OK(int tid, SSdpInfo *pSdpInfo)
{		
	ENTER_FUNC_SIPUA();
	
	osip_message_t *answer = NULL;
	int ret = eXosip_call_build_answer(tid, 200, &answer);
	if (ret != 0)
	{
		WARN_LOG_SIPUA("build answer failed, ret[%d], tid[%d]", ret, tid);
		eXosip_call_send_answer (tid, 400, NULL);
		ret = -1;
	}
	else
	{
		char szBody[1024] = {0};
		int iBodyLen = sdpInfo2BodyString(pSdpInfo, szBody);
		osip_message_set_body(answer, szBody, iBodyLen);
		osip_message_set_content_type(answer, "application/sdp");
        answer->v3_head = NULL;
		ret = eXosip_call_send_answer (tid, 200, answer);		
	}

	EXIT_FUNC_SIPUA();
	return ret;
}

/*回复INVITE请求失败
返回值: 成功返回0
*/
int SipUA_AnswerInviteFailure(int tid, int status)
{
	ENTER_FUNC_SIPUA();
	int ret = eXosip_call_send_answer(tid, status, NULL);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*发送ACK
返回值: 成功返回0
*/
int SipUA_SendAck(int did)
{
	ENTER_FUNC_SIPUA();
	
	osip_message_t *ack = NULL;
	int ret = eXosip_call_build_ack(did, &ack);
	if (ret != 0)
	{
		WARN_LOG_SIPUA("build ack failed, ret=%d, did=%d", ret, did);
		EXIT_FUNC_SIPUA();
		return -1;
	}
    ack->v3_head = NULL;
	ret = eXosip_call_send_ack(did, ack);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*挂断呼叫*/
int SipUA_TerminateCall(int cid, int did)
{
	ENTER_FUNC_SIPUA();
	int ret = eXosip_call_terminate(cid, did);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*发起一条option消息
根据国标文档，body数据为XML格式
*/
int SipUA_InitOPTIONS(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
    const char *pszBody, int iBodyLen)
{
    ENTER_FUNC_SIPUA();

    char szSrc[1024] = { 0 };
    char szDest[1024] = { 0 };
    sprintf(szSrc, "sip:%s@%s:%d", pszSrcId, g_localAddr.strIP.c_str(), g_localAddr.port);
    sprintf(szDest, "sip:%s@%s:%d", pszDestId, pszDestIP, iDestPort);

    osip_message_t *message = NULL;
    int ret = eXosip_message_build_request(&message, "OPTIONS", szDest, szSrc, NULL);
    if (0 != ret)
    {
        WARN_LOG_SIPUA("build message request failed, src[%s], dest[%s]", szSrc, szDest);
        EXIT_FUNC_SIPUA();
        return -1;
    }

    osip_message_set_body(message, pszBody, iBodyLen);
    osip_message_set_content_type(message, "txt/xml");
    message->v3_head = ";MSG_TYPE=MSG_VTDU_HEART";
    ret = eXosip_message_send_request(message);
    EXIT_FUNC_SIPUA();
    return ret;
}

/*发起一条MESSAGE消息
根据国标文档，body数据为XML格式
*/
int SipUA_InitMessage(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
	const char *pszBody, int iBodyLen)
{
	ENTER_FUNC_SIPUA();
	
	char szSrc[1024] = {0};
	char szDest[1024] = {0};
	sprintf(szSrc, "sip:%s@%s:%d", pszSrcId, g_localAddr.strIP.c_str(), g_localAddr.port);
	sprintf(szDest, "sip:%s@%s:%d", pszDestId, pszDestIP, iDestPort);

	osip_message_t *message = NULL;
	int ret = eXosip_message_build_request(&message, "MESSAGE", szDest, szSrc, NULL);
	if (0 != ret)
	{
		WARN_LOG_SIPUA("build message request failed, src[%s], dest[%s]", szSrc, szDest);
		EXIT_FUNC_SIPUA();
		return -1;
	}

	osip_message_set_body(message, pszBody, iBodyLen);
	osip_message_set_content_type(message, "Application/MANSCDP+xml");
    message->v3_head = NULL;
	ret = eXosip_message_send_request(message);
	EXIT_FUNC_SIPUA();
	return ret;
}


/*回复info*/
int SipUA_AnswerInfo(void *pMsgPtr, int status, const char *pszBody, int iBodyLen)
{
    ENTER_FUNC_SIPUA();
    eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
    osip_message_t *answer = NULL;
    eXosip_message_build_answer(evt->tid, status, &answer);

    if (iBodyLen > 0)
    {
        osip_message_set_body(answer, pszBody, iBodyLen);
        osip_message_set_content_type(answer, "xml/txt");
    }
    answer->v3_head = NULL;
    int ret = eXosip_message_send_answer(evt->tid, status, answer);
    EXIT_FUNC_SIPUA();
    return ret;
}

/*回复MESSAGE*/
int SipUA_AnswerMessage(void *pMsgPtr, int status, const char *pszBody, int iBodyLen)
{
	ENTER_FUNC_SIPUA();
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	osip_message_t *answer = NULL;
	eXosip_message_build_answer(evt->tid, status, &answer);

	if (iBodyLen > 0)
	{
		osip_message_set_body(answer, pszBody, iBodyLen);
		osip_message_set_content_type(answer, "Application/MANSCDP+xml");
	}
    answer->v3_head = NULL;
	int ret = eXosip_message_send_answer(evt->tid, status, answer);
	EXIT_FUNC_SIPUA();
	return ret;
}


/**< unique id for transactions (to be used for answers) */
int SipUA_GetCallTransId(void *pMsgPtr)
{
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	return evt->tid;
}

/**< unique id for SIP dialogs */
int SipUA_GetCallDialogsId(void *pMsgPtr)
{
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	return evt->did;
}

/**< unique id for SIP calls (but multiple dialogs!) */
int SipUA_GetCallCallId(void *pMsgPtr)
{
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	return evt->cid;
}


/*获取INVITE请求的SDP信息
@pMsgPtr: 指向消息内容地址的指针
@pOutSdpInfo: 返回SDP结构体
返回值: 成功返回0
*/
int SipUA_GetRequestSdpInfo(void *pMsgPtr, SSdpInfo *pOutSdpInfo)
{
	ENTER_FUNC_SIPUA();
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	int ret = getSdpInfo(evt->request, pOutSdpInfo);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*获取200 OK回复的SDP信息
@pMsgPtr: 指向消息内容地址的指针
@pOutSdpInfo: 返回SDP结构体
返回值: 成功返回0
*/
int SipUA_GetResponseSdpInfo(void *pMsgPtr, SSdpInfo *pOutSdpInfo)
{
	ENTER_FUNC_SIPUA();
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	int ret = getSdpInfo(evt->response, pOutSdpInfo);
	EXIT_FUNC_SIPUA();
	return ret;
}

int SipUA_GetRequestSubject(void *pMsgPtr, SSubjectInfo *pOutSubject)
{
	ENTER_FUNC_SIPUA();
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	int ret = getSubject(evt->request, pOutSubject);
	EXIT_FUNC_SIPUA();
	return ret;
}

int SipUA_GetResponseSubject(void *pMsgPtr, SSubjectInfo *pOutSubject)
{
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	int ret = getSubject(evt->response, pOutSubject);
	EXIT_FUNC_SIPUA();
	return ret;
}

/*获取from头*/
int SipUA_GetRequestFromHeader(void *pMsgPtr, SFromHeader *pOutFrom)
{
	ENTER_FUNC_SIPUA();
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	memset(pOutFrom, 0, sizeof(SFromHeader));
	if (NULL == evt->request)
	{
		EXIT_FUNC_SIPUA();
		return -1;
	}

	osip_from_t *pFromHeader = osip_message_get_from(evt->request);
	if (pFromHeader)
	{		
		if (pFromHeader->url->username)
			sprintf(pOutFrom->szUser, "%s", pFromHeader->url->username);

		if (pFromHeader->url->host)
			sprintf(pOutFrom->szIP, "%s", pFromHeader->url->host);

		if (pFromHeader->url->port)
			pOutFrom->port = atoi(pFromHeader->url->port);

		EXIT_FUNC_SIPUA();
		return 0;
	}

	EXIT_FUNC_SIPUA();
	return -1;
}

int SipUA_GetRegisterStatus(void *pMsgPtr, int *pnStatus)
{
    eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
    *pnStatus = evt->response->status_code;
    return 0;
}

/*获取注册信息*/
int SipUA_GetRegisterInfo(void *pMsgPtr, SRegisterInfo *pOutRegisterInfo)
{
	ENTER_FUNC_SIPUA();
	
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	memset(pOutRegisterInfo, 0, sizeof(SRegisterInfo));	

	if (NULL == evt->request)
	{
		EXIT_FUNC_SIPUA();
		return -1;
	}

	//get via
	osip_via_t *via = NULL;
	osip_message_get_via(evt->request, 0, &via);
	if (via != NULL)
	{
		if(via->host)
			sprintf(pOutRegisterInfo->szContactIP, "%s", via->host);

		if(via->port)
			pOutRegisterInfo->iContactPort = atoi(via->port);
	}

	//get contact
	osip_contact_t *contact = NULL;
	osip_message_get_contact(evt->request, 0, &contact);
	if (contact != NULL)
	{	
		if (contact->url->username)
			sprintf(pOutRegisterInfo->szRegUser, "%s", contact->url->username);

		//优化使用VIA域的地址作为连接地址，如果via域没地址，使用contact域地址
		if (strlen(pOutRegisterInfo->szContactIP) <= 0)
		{
			if (contact->url->host)
				sprintf(pOutRegisterInfo->szContactIP, "%s", contact->url->host);

			if (contact->url->port)
				pOutRegisterInfo->iContactPort = atoi(contact->url->port);
		}
	}

	//get call-id
	osip_call_id_t *callid = osip_message_get_call_id(evt->request);
	if (callid != NULL)
	{
		if (callid->number && callid->host)
			sprintf(pOutRegisterInfo->szCallId, "%s@%s", callid->number, callid->host);
		else if (callid->number && NULL == callid->host)
			sprintf(pOutRegisterInfo->szCallId, "%s", callid->number);
	}

	//get expires
	osip_header_t* header = NULL;
	int result = osip_message_get_expires(evt->request, 0, &header);
	if (result < 0)
	{
		WARN_LOG_SIPUA("get expire fail when recv register");
		pOutRegisterInfo->expires = -1;
	}
	else
	{
		pOutRegisterInfo->expires = atoi(header->hvalue);
	}

	//get author info
	osip_authorization_t * Sdest = NULL;
	osip_message_get_authorization(evt->request, 0, &Sdest);
	if (Sdest != NULL)
	{
		pOutRegisterInfo->ucAuthorFlag = 1;

		char *pMethod = evt->request->sip_method;
		char *pUsername = (char *)"";
        if (Sdest->username != NULL)
        {
            pUsername = osip_strdup_without_quote(Sdest->username);
        }
		char *pRealm = (char *)"";
        if (Sdest->realm != NULL)
        {
            pRealm = osip_strdup_without_quote(Sdest->realm);
        }
		char *pNonce = (char *)"";
        if (Sdest->nonce != NULL)
        {
            pNonce = osip_strdup_without_quote(Sdest->nonce);
        }
		char *pResponse = (char *)"";
        if (Sdest->response != NULL)
        {
            pResponse = osip_strdup_without_quote(Sdest->response);
        }

		char *pUri = (char *)"";
        if (Sdest->uri != NULL)
        {
            pUri = osip_strdup_without_quote(Sdest->uri);
        }
		
		strcpy(pOutRegisterInfo->szAuthorUser, pUsername);
		strcpy(pOutRegisterInfo->szRealm, pRealm);
		strcpy(pOutRegisterInfo->szNonce, pNonce);
		strcpy(pOutRegisterInfo->szUri, pUri);
		strcpy(pOutRegisterInfo->szMethod, pMethod);
		strcpy(pOutRegisterInfo->szResponse, pResponse);
		
	}
	else
	{
		pOutRegisterInfo->ucAuthorFlag = 0;
	}

	EXIT_FUNC_SIPUA();
	return 0;
}

/*获取消息体*/
int SipUA_GetRequestBodyContent(void *pMsgPtr, char *pszOutBody, int iMaxBodyLen)
{
	ENTER_FUNC_SIPUA();
	
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	if (NULL == evt->request)
	{
		WARN_LOG_SIPUA("get request body failed, request null.");
		EXIT_FUNC_SIPUA();
		return -1;
	}

	int ret = 0;
	osip_body_t *body = NULL;
	osip_message_get_body(evt->request, 0, &body);
	if (body)
	{
		ret = (int)body->length;
		if (ret > 0 && ret <= iMaxBodyLen)
		{
			memcpy(pszOutBody, body->body, ret);
		}
		else
		{
			WARN_LOG_SIPUA("get request body failed, size[%d] out of range[%d]", ret, iMaxBodyLen);
		}		
	}
	
	EXIT_FUNC_SIPUA();
	return ret;
}

/*获取SIP响应的消息体*/
int SipUA_GetResponseBodyContent(void *pMsgPtr, char *pszOutBody, int iMaxBodyLen)
{
	ENTER_FUNC_SIPUA();
		
	eXosip_event_t *evt = (eXosip_event_t *)pMsgPtr;
	if (NULL == evt->response)
	{
		WARN_LOG_SIPUA("get response body failed, response null.");
		EXIT_FUNC_SIPUA();
		return -1;
	}

	int ret = 0;
	osip_body_t *body = NULL;
	osip_message_get_body(evt->response, 0, &body);
	if (body)
	{
		ret = (int)body->length;
		if (ret > 0 && ret <= iMaxBodyLen)
		{
			memcpy(pszOutBody, body->body, ret);
		}
		else
		{
			WARN_LOG_SIPUA("get response body failed, size[%d] out of range[%d]", ret, iMaxBodyLen);
		}		
	}
	
	EXIT_FUNC_SIPUA();
	return ret;

}




