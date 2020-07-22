#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "SipUA.h"
#include <string>
#include "CallManager.h"
#include "ConfigSipServer.h"
#include "CodeConverter.h"
#include <vector>
//class CConfigSipServersssss
//{
//public:
//    std::string m_strCfgFile;
//
//    /*本地SIP配置参数*/
//    int m_iTransport = SIPUA_TANSPORT_UDP;
//    std::string m_strSipAddr;
//    int m_iSipPort = 0;
//    std::string m_strLocalSipId;//"34020100002000000001";
//    std::string m_strPwd; //"123456"; //
//    int m_iExpires = 0;
//    int m_iMonitorKeepalive = 120; //监视下级平台的keepalive超时时间
//
//    /*远端SIP服务器配置参数*/
//    std::string m_strServerId;
//    std::string m_strServerIP; //"192.168.2.5"; //
//    int m_iServerPort = 0;
//
//    /*http api配置参数*/
//    int m_iHttpApiPort;
//    std::string m_strHttpApiRoot;
//
//    //中心服务器配置
//    int m_iEnableCenterServer; //是否启用与中心服务器间交互
//    std::string m_strCenterServerIP; //中心服务器的thrift地址
//    int m_iCenterServerPort;
//    int m_iSipServerId;
//    int m_iRegisterCenterTime; //向中心服务器定时注册的时间
//};

typedef struct PortUse
{
    int nport;
    bool bused;
}stPortUse;

typedef struct MediaInfo
{
    std::string  strCuUserID;
    std::string  strCuIp;
    int nCuport;
    int nCuNat;
    int nCuTransType;
    int nCuUserType;
    std::string strCuGBVideoTransMode;
    int nCuMSCheck;
    std::string strPUID;
    int nPuChannelID;
    int nPuProtocol;
    int nPuStreamType;
    int nPuTransType;
    int nPuFCode;
    std::string strVtduRecvIp;
    std::string strVtduSendIP;
    int nOpenVideoStreamType;
    std::string strOpenVideoGBVideoTransMode;
    int nVtduRecvPort;
    int nVtduSendPort;
}stMediaInfo;

class SipServer
{
public:
    SipServer();
    ~SipServer();


    int StartSipServer(int transport, int iListenPort, const char *pszSrcAddr, int iSrcPort,
        fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB, int nWorkMode);

    int StopSipServer();

    void sipServerHandleInfo(void *pMsgPtr);
    /*处理MESSAGE消息*/
    void sipServerHandleMessage(void *pMsgPtr);

    /*处理注册消息，新的注册*/
    void sipServerHandleRegisterNew(void *pMsgPtr);

    /*处理注册消息，刷新注册*/
    void sipServerHandleRegisterRefresh(void *pMsgPtr);

    /*处理invite消息*/
    void sipServerHandleCallInviteIncoming(void *pMsgPtr);

    /*处理INVITE 200响应*/
    void sipServerHandleCallAnswer200OK(void *pMsgPtr);

    /*处理INVITE 其他非200响应*/
    void sipServerHandleCallAnswerResponse(void *pMsgPtr);

    /*发送ACK响应*/
    void sipSendCallAck(void *pMsgPtr);

    /*处理ACK响应*/
    void sipServerHandleCallAck(void *pMsgPtr);

    /*处理BYE消息*/
    void sipServerHandleCallBye(void *pMsgPtr);

    //处理v3预览请求
    void sipServerHandleV3TransReady(void *pMsgPtr);
    //处理v3预览确认请求
    void sipServerHandleV3TransAck(void *pMsgPtr);
    //处理v3预览停止请求
    void sipServerHandleV3TransStop(void *pMsgPtr);
    //处理v3回放开始请求
    void sipServerHandleV3FileStart(void *pMsgPtr);
    //处理v3回放停止请求
    void sipServerHandleV3FileStop(void *pMsgPtr);

    /* 获取xml根结节下的element text */
    std::string getXmlElementText(const char *pszXml, int iXmlLen, const char *pszValue);

    std::string getMsgCmdTypeV3(const char *pszBody, int iBodyLen);

    /* 从MESSAGE消息体找到<CmdType></CmdType> */
    std::string getMsgCmdType(const char *pszBody, int iBodyLen);

    //处理Keepalive消息
    int handleKeepaliveMsg(const char *pszBody, int iBodyLen);

    int initInviteMsg(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, int tid, int cid, int did);

    /*处理实时视频的INVITE请求*/
    void handleInviteRealPlay(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr);

    /*处理历史视频的INVITE请求*/
    void handleInvitePlayback(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr);

    //处理Catalog消息
    int handleCatalogMsg(const char *pszBody, int iBodyLen);

    //处理录像检索消息
    int handleRecordInfoMsg(const char *pszBody, int iBodyLen);

    //客户端处理注册失败消息
    void sipClientHandleRegisterFailed(void *pMsgPtr);

    void sipClientHandleRegisterSuccess(void *pMsgPtr);


private:
    /*呼叫管理*/
    CCallManager g_callMgr;

    //MESSAGE消息的请求SN
    unsigned long g_ulMsgReqSN;
    std::vector<stPortUse> vecRecvPortUse;
    std::vector<stPortUse> vecSendPortUse;
    unsigned int m_nInitialUser;
};

