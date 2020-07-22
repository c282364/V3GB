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
//    /*����SIP���ò���*/
//    int m_iTransport = SIPUA_TANSPORT_UDP;
//    std::string m_strSipAddr;
//    int m_iSipPort = 0;
//    std::string m_strLocalSipId;//"34020100002000000001";
//    std::string m_strPwd; //"123456"; //
//    int m_iExpires = 0;
//    int m_iMonitorKeepalive = 120; //�����¼�ƽ̨��keepalive��ʱʱ��
//
//    /*Զ��SIP���������ò���*/
//    std::string m_strServerId;
//    std::string m_strServerIP; //"192.168.2.5"; //
//    int m_iServerPort = 0;
//
//    /*http api���ò���*/
//    int m_iHttpApiPort;
//    std::string m_strHttpApiRoot;
//
//    //���ķ���������
//    int m_iEnableCenterServer; //�Ƿ����������ķ������佻��
//    std::string m_strCenterServerIP; //���ķ�������thrift��ַ
//    int m_iCenterServerPort;
//    int m_iSipServerId;
//    int m_iRegisterCenterTime; //�����ķ�������ʱע���ʱ��
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
    /*����MESSAGE��Ϣ*/
    void sipServerHandleMessage(void *pMsgPtr);

    /*����ע����Ϣ���µ�ע��*/
    void sipServerHandleRegisterNew(void *pMsgPtr);

    /*����ע����Ϣ��ˢ��ע��*/
    void sipServerHandleRegisterRefresh(void *pMsgPtr);

    /*����invite��Ϣ*/
    void sipServerHandleCallInviteIncoming(void *pMsgPtr);

    /*����INVITE 200��Ӧ*/
    void sipServerHandleCallAnswer200OK(void *pMsgPtr);

    /*����INVITE ������200��Ӧ*/
    void sipServerHandleCallAnswerResponse(void *pMsgPtr);

    /*����ACK��Ӧ*/
    void sipSendCallAck(void *pMsgPtr);

    /*����ACK��Ӧ*/
    void sipServerHandleCallAck(void *pMsgPtr);

    /*����BYE��Ϣ*/
    void sipServerHandleCallBye(void *pMsgPtr);

    //����v3Ԥ������
    void sipServerHandleV3TransReady(void *pMsgPtr);
    //����v3Ԥ��ȷ������
    void sipServerHandleV3TransAck(void *pMsgPtr);
    //����v3Ԥ��ֹͣ����
    void sipServerHandleV3TransStop(void *pMsgPtr);
    //����v3�طſ�ʼ����
    void sipServerHandleV3FileStart(void *pMsgPtr);
    //����v3�ط�ֹͣ����
    void sipServerHandleV3FileStop(void *pMsgPtr);

    /* ��ȡxml������µ�element text */
    std::string getXmlElementText(const char *pszXml, int iXmlLen, const char *pszValue);

    std::string getMsgCmdTypeV3(const char *pszBody, int iBodyLen);

    /* ��MESSAGE��Ϣ���ҵ�<CmdType></CmdType> */
    std::string getMsgCmdType(const char *pszBody, int iBodyLen);

    //����Keepalive��Ϣ
    int handleKeepaliveMsg(const char *pszBody, int iBodyLen);

    int initInviteMsg(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, int tid, int cid, int did);

    /*����ʵʱ��Ƶ��INVITE����*/
    void handleInviteRealPlay(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr);

    /*������ʷ��Ƶ��INVITE����*/
    void handleInvitePlayback(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr);

    //����Catalog��Ϣ
    int handleCatalogMsg(const char *pszBody, int iBodyLen);

    //����¼�������Ϣ
    int handleRecordInfoMsg(const char *pszBody, int iBodyLen);

    //�ͻ��˴���ע��ʧ����Ϣ
    void sipClientHandleRegisterFailed(void *pMsgPtr);

    void sipClientHandleRegisterSuccess(void *pMsgPtr);


private:
    /*���й���*/
    CCallManager g_callMgr;

    //MESSAGE��Ϣ������SN
    unsigned long g_ulMsgReqSN;
    std::vector<stPortUse> vecRecvPortUse;
    std::vector<stPortUse> vecSendPortUse;
    unsigned int m_nInitialUser;
};

