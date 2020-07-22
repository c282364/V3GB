#ifndef __SIP_UA_H__
#define __SIP_UA_H__

/*数据传输协议,tcp/udp传输SIP消息*/
#define SIPUA_TANSPORT_UDP  0
#define SIPUA_TANSPORT_TCP  1

typedef enum tagESipUAMsg
{
    SIPUA_MSG_UNKNOWN = 0,
    /*注册类消息*/
    SIPUA_MSG_REGISTER_NEW, /*收到注册消息，对应EXOSIP_REGISTRATION_NEW，但已作废*/
    SIPUA_MSG_REGISTER_SUCCESS, /*注册成功*/
    SIPUA_MSG_REGISTER_FAILURE, /*注册失败*/
    SIPUA_MSG_REGISTER_REFRESH, /*刷新注册, 对应EXOSIP_REGISTRATION_REFRESHED消息，但已作废*/
    
    /*呼叫类消息*/
    SIPUA_MSG_CALL_INVITE_INCOMING, /*收到INVITE */
    SIPUA_MSG_CALL_ANSWER_200OK, /*200 ok消息*/
    SIPUA_MSG_CALL_ANSWER_RSP, /*其他非200响应*/
    SIPUA_MSG_CALL_ACK, /*ack*/
    SIPUA_MSG_CALL_BYE, /*bye*/

    /*MESSAGE消息*/
    SIPUA_MSG_MESSAGE, //收到MESSAGE消息
    SIPUA_MSG_MESSAGE_ANSWERED, //收到MESSAGE消息的200 ok响应
    SIPUA_MSG_MESSAGE_REQUESTFAILURE, //收到MESSAGE消息的错误响应

    SIPUA_MSG_INFO,//收到INFO消息
}ESipUAMsg;

/*字符串最大长度*/
#define SIPUA_STRING_LINE_LEN         128

/*sdp扩展x=内容长度*/
#define SDP_X_LINE_STRING_LEN       256

/*media信息最多的属性行*/
#define SDP_MEDIA_ATTR_MAX_COUNT    8

/*媒体类型: video/audio*/
typedef enum tagEMediaType
{
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_AUDIO,
}EMediaType;

/*媒体传输方向: sendonly, recvonly, sendrecv, inactive*/
typedef enum tagETransDir
{
    TRANS_DIR_UNKNOWN = 0,
    TRANS_DIR_SENDONLY,
    TRANS_DIR_RECVONLY,
    TRANS_DIR_SENDRECV,
    TRANS_DIR_INACTIVE,
}ETransDir;

/*payload type*/
typedef enum tagEPayloadType
{
    PAYLOAD_TYPE_UNKNOWN = 0,
    PAYLOAD_TYPE_PS = 96,
    PAYLOAD_TYPE_MPEG4 = 97,
    PAYLOAD_TYPE_H264 = 98,
    PAYLOAD_TYPE_SVAC = 99,
}EPayloadType;


/*媒体属性a行*/
typedef struct tagSMediaAttr
{
    EPayloadType ePayloadType; //96 98 97
    char szAttrLine[SIPUA_STRING_LINE_LEN]; //a=rtpmap:96 PS/90000
}SMediaAttr;

/*媒体信息，解析m行及其a行获得
m=video 6000 RTP/AVP 96 98 97
a=recvonly
a=rtpmap:96 PS/90000
a=rtpmap:98 H264/90000
a=rtpmap:97 MPEG4/90000
*/
typedef struct tagSMediaInfo
{
    unsigned char ucUseFlag; /*sdp中m行的存在标记*/
    char szAddr[SIPUA_STRING_LINE_LEN];
    int port;
    char szProto[SIPUA_STRING_LINE_LEN];
    ETransDir eDir;
    int iAttrCount; /*属性行数量*/
    SMediaAttr sAttr[SDP_MEDIA_ATTR_MAX_COUNT];
}SMediaInfo;


/*SDP信息*/
typedef struct tagSSdpInfo
{
    char szVersion[SIPUA_STRING_LINE_LEN]; //v=0
    char szOrigin[SIPUA_STRING_LINE_LEN]; //o=34020000002020000001 0 0 IN IP4 192.168.3.81
    char szSessionName[SIPUA_STRING_LINE_LEN]; //s=Playback
    char szUri[SIPUA_STRING_LINE_LEN]; //u=34020100001310000001:3
    char szConnection[SIPUA_STRING_LINE_LEN]; //c=IN IP4 192.168.3.81
    long long i64TimeBegin; long long i64TimeEnd; //t=1311904968 1311906769
    char szY[SIPUA_STRING_LINE_LEN]; //y=1100000000
    char szF[SIPUA_STRING_LINE_LEN]; //f=
    /*x=扩展SDP，addr=192.168.2.107&port=8000&deivce_info=35&type=hc_ipc&user=admin&pwd=admin1234*/
    char szX[SDP_X_LINE_STRING_LEN]; 
    SMediaInfo sVideoMediaInfo; //m=video
    SMediaInfo sAudioMediaInfo; //m=audio
}SSdpInfo;

/*subject信息*/
typedef struct tagSSubjectInfo
{
    char szSenderId[SIPUA_STRING_LINE_LEN]; /*媒体流发送者设备编码*/
    char szSenderStreamSeq[SIPUA_STRING_LINE_LEN]; /*发送端媒体流序列号*/
    char szRecverId[SIPUA_STRING_LINE_LEN]; /*媒体流接收者设备编码*/
    char szRecverStreamSeq[SIPUA_STRING_LINE_LEN];/*接收端媒体流序列号*/
}SSubjectInfo;

/*注册消息内容*/
typedef struct tagSRegisterInfo
{    
    /*contact信息*/
    char szRegUser[SIPUA_STRING_LINE_LEN];
    char szContactIP[SIPUA_STRING_LINE_LEN];
    int iContactPort;
	
    char szCallId[SIPUA_STRING_LINE_LEN]; //注册的call-id头
    unsigned char ucAuthorFlag; /*带鉴权信息标记: 0未带鉴权信息，1带鉴权信息*/
    char szAuthorUser[SIPUA_STRING_LINE_LEN]; /*鉴权信息，用户名*/
    char szRealm[SIPUA_STRING_LINE_LEN]; //realm
    char szNonce[SIPUA_STRING_LINE_LEN]; //nonce
    char szUri[SIPUA_STRING_LINE_LEN]; //uri
    char szMethod[SIPUA_STRING_LINE_LEN]; //method
    char szResponse[SIPUA_STRING_LINE_LEN]; //response
    int expires; //expires
}SRegisterInfo;

/*消息的from头域*/
typedef struct tagSFromHeader
{
    char szUser[SIPUA_STRING_LINE_LEN];
    char szIP[SIPUA_STRING_LINE_LEN];
    int port;
}SFromHeader;


/*SipUA内部的日志，可以通过此回调函数输出，由外部的日志管理模块处理*/
typedef void (*fnSipUA_OutputLogCB)(int iLogLevel, const char *fn, 
    const char *szFile, const int iLine, const char* pszLogInfo, void *pParamCB);

 
/*SIPUA事件回调函数
@msg: 消息类型参考ESipUAMsg
@pSdpInfo: 由SDP字符串解析出的结构体，INVITE消息会解析，
其他非呼叫类的消息为NULL
@pMsgPtr: 指向消息内容地址的指针
@pszBody/iBodyLen: 消息体字符串/长度
@pParam: 回调函数用户参数
*/
typedef void (*fnSipUAEventCB)(ESipUAMsg msg, void *pMsgPtr, void *pParam);


/*初始化SIPUA
@iListenPort: sipua真正使用的本地地址
@pszSrcAddr, iSrcPort: SIP信令的源地址，如果要上云，使用云端地址
返回值: 成功返回0
*/
int SipUA_Init(int transport, int iListenPort, const char *pszSrcAddr, int iSrcPort, 
	fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB);

/*释放SIPUA
返回值: 成功返回0
*/
int SipUA_Release();

/*开始SIPUA事件
返回值: 成功返回0
*/
int SipUA_StartEventLoop(fnSipUAEventCB fnCB, void *pParam);


/*结束SIPUA
返回值: 成功返回0
*/
int SipUA_StopEventLoop();


/*使用鉴权方式注册到SIP服务器
@pszSipId: 注册的SIP编号
@pszUser, pszPwd: 注册用户名，密码
@expires: 超时时间
@pszServerId: 服务器SIP编号
@pszServerIP, iServerPort: 服务器地址
@pszBody, iBodyLen: 携带消息体，如果REGISTER不需要body，则设置body NULL, bodylen 0.
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterAuthor(
	const char *pszSipId, const char *pszUser, const char *pszPwd, int expires,
	const char *pszServerId, const char *pszServerIP, int iServerPort,
	const char *pszBody, int iBodyLen);

/*使用非鉴权方式注册到SIP服务器
参数参考SipUA_RegisterAuthor
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterUnauthorV3(
    const char *pszSipId, const char * pszSipRegion, const char * pszSipIp, int iSipPort, int expires,
    const char *pszServerId, const char * pszServerRegion, const char *pszServerIP, int iServerPort,
    const char *pszBody, int iBodyLen);


/*使用非鉴权方式注册到SIP服务器
参数参考SipUA_RegisterAuthor
返回值: 成功返回大于0的regid
*/
int SipUA_RegisterUnauthor(
	const char *pszSipId, int expires,
	const char *pszServerId, const char *pszServerIP, int iServerPort,
	const char *pszBody, int iBodyLen);

/*刷新注册超时时间
@regid: 由SipUA_RegisterAuthor/SipUA_RegisterUnauthor返回
@expires: 超时时间
返回值: 成功返回0
*/
int SipUA_RefreshRegister(int regid, int expires);

/*回复REGISTER 401未认证的响应
@pszRealm, pszNonce: 鉴权信息
@pMsgPtr: 指向消息内容地址的指针
*/
int SipUA_RegisterReply401Unauthorized(void *pMsgPtr, const char *pszRealm, const char *pszNonce);

/*回复REGISTER 200*/
int SipUA_RegisterReply200OK(void *pMsgPtr, const char *pszSrcId);



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
		const char *pszSubject, const char *pszBody, int iBodyLen);

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
		SSubjectInfo *pSubjectInfo, SSdpInfo *pSdpInfo);

/*回复INVITE请求200 OK
返回值: 成功返回0
*/
int SipUA_AnswerInvite200OK(int tid, SSdpInfo *pSdpInfo);

/*回复INVITE请求失败
返回值: 成功返回0
*/
int SipUA_AnswerInviteFailure(int tid, int status);


/*发送ACK
返回值: 成功返回0
*/
int SipUA_SendAck(int did);

/*挂断呼叫*/
int SipUA_TerminateCall(int cid, int did);

/*发起一条option消息
根据国标文档，body数据为XML格式
*/
int SipUA_InitOPTIONS(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
    const char *pszBody, int iBodyLen);

/*发起一条MESSAGE消息
根据国标文档，body数据为XML格式
*/
int SipUA_InitMessage(const char *pszSrcId, const char *pszDestId, const char *pszDestIP, int iDestPort,
	const char *pszBody, int iBodyLen);

/*回复MESSAGE*/
int SipUA_AnswerMessage(void *pMsgPtr, int status, const char *pszBody, int iBodyLen);

/*回复info*/
int SipUA_AnswerInfo(void *pMsgPtr, int status, const char *pszBody, int iBodyLen);

/**呼叫类消息，得到其ID, < unique id for transactions (to be used for answers) */
int SipUA_GetCallTransId(void *pMsgPtr);

/**< unique id for SIP dialogs */
int SipUA_GetCallDialogsId(void *pMsgPtr);

/**< unique id for SIP calls (but multiple dialogs!) */
int SipUA_GetCallCallId(void *pMsgPtr);

/*获取INVITE请求的SDP信息
@pMsgPtr: 指向消息内容地址的指针
@pOutSdpInfo: 返回SDP结构体
返回值: 成功返回0
*/
int SipUA_GetRequestSdpInfo(void *pMsgPtr, SSdpInfo *pOutSdpInfo);

/*获取200 OK回复的SDP信息
@pMsgPtr: 指向消息内容地址的指针
@pOutSdpInfo: 返回SDP结构体
返回值: 成功返回0
*/
int SipUA_GetResponseSdpInfo(void *pMsgPtr, SSdpInfo *pOutSdpInfo);

/*获取subject信息*/
int SipUA_GetRequestSubject(void *pMsgPtr, SSubjectInfo *pOutSubject);

int SipUA_GetResponseSubject(void *pMsgPtr, SSubjectInfo *pOutSubject);

/*获取from头*/
int SipUA_GetRequestFromHeader(void *pMsgPtr, SFromHeader *pOutFrom);

int SipUA_GetRegisterStatus(void *pMsgPtr, int *pnStatus);
/*获取注册信息*/
int SipUA_GetRegisterInfo(void *pMsgPtr, SRegisterInfo *pOutRegisterInfo);

/*获取SIP请求的消息体*/
int SipUA_GetRequestBodyContent(void *pMsgPtr, char *pszOutBody, int iMaxBodyLen);

/*获取SIP响应的消息体*/
int SipUA_GetResponseBodyContent(void *pMsgPtr, char *pszOutBody, int iMaxBodyLen);



#endif


