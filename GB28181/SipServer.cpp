#include "SipServer.h"
#include "SipUA.h"
#include "UaComm.h"
#include "tinyxml.h"
#include "json/json.h"
#include "GB28181MediaGate.h"
#include "GB28181StreamPuller.h"
#include "ConfigFile/inifile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
CConfigSipServer g_configSipServer;
using namespace inifile;
void SipServerEventCB(ESipUAMsg msg, void *pMsgPtr, void *pParam)
{
    SipServer* pSipServer = (SipServer*)pParam;
    printf("EventCB %d\n",msg);
    switch (msg)
    {
        /*����ע������Ϣ*/
    case SIPUA_MSG_REGISTER_NEW: /*�µ�ע��*/
        pSipServer->sipServerHandleRegisterNew(pMsgPtr);
        break;

    case SIPUA_MSG_REGISTER_REFRESH: /*ˢ��ע��*/
        pSipServer->sipServerHandleRegisterRefresh(pMsgPtr);
        break;

    case SIPUA_MSG_REGISTER_SUCCESS:
        pSipServer->sipClientHandleRegisterSuccess(pMsgPtr);// ע��ɹ�
        break;

    case SIPUA_MSG_REGISTER_FAILURE:
        pSipServer->sipClientHandleRegisterFailed(pMsgPtr);// ע��ʧ�� 401/��401
        break;

        /*�����������Ϣ*/
    case SIPUA_MSG_CALL_INVITE_INCOMING: /*�յ����з���INVITE*/
        pSipServer->sipServerHandleCallInviteIncoming(pMsgPtr);
        break;

    case SIPUA_MSG_CALL_ANSWER_200OK: /*�յ����лظ�200 OK*/
        pSipServer->sipServerHandleCallAnswer200OK(pMsgPtr);
        //pSipServer->sipSendCallAck(pMsgPtr);
        break;

    case SIPUA_MSG_CALL_ANSWER_RSP: /*�յ����лظ�������200��Ӧ*/
        pSipServer->sipServerHandleCallAnswerResponse(pMsgPtr);
        break;

    case SIPUA_MSG_CALL_ACK: /*�յ����е�ACK*/
        pSipServer->sipServerHandleCallAck(pMsgPtr);
        break;

    case SIPUA_MSG_CALL_BYE: /*�յ�BYE��Ϣ�����л��߱���*/
        pSipServer->sipServerHandleCallBye(pMsgPtr);
        break;

        /*����MESSAGE��Ϣ*/
    case SIPUA_MSG_MESSAGE:
        pSipServer->sipServerHandleMessage(pMsgPtr);
        break;
        /*����INFO��Ϣ*/
    case SIPUA_MSG_INFO:
        pSipServer->sipServerHandleInfo(pMsgPtr);
        break;
    default:
        break;
    }
}

/*ע���߳�*/
bool g_bRegisterSuccess = false;
static void *threadRegister(void *pParam)
{
    do 
    {
        int regid = SipUA_RegisterUnauthorV3(
            g_configSipServer.m_strLocalSipId.c_str(), g_configSipServer.m_strSipRegion.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_iSipPort, g_configSipServer.m_iExpires,
            g_configSipServer.m_strServerId.c_str(), g_configSipServer.m_strServerRegion.c_str(), g_configSipServer.m_strServerIP.c_str(),
            g_configSipServer.m_iServerPort, NULL, 0);
        sleep(60);
    } while (!g_bRegisterSuccess);
}

bool g_bHeartBeatThreadWorking = true;
//���������߳�
static void *threadHeartBeat(void *pParam)
{
    do
    {
        char szKeepaliveXml[1024] = { 0 };
        int nInStrmSize = 0;
        int iXmlLen = sprintf(szKeepaliveXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<Message Version=\"1.0.2\">\r\n"
            "<IE_HEADER/>\r\n"
            "<VTDU_LOAD>\r\n"
            "<inStrmSize>%d</inStrmSize>\r\n"
            "<oStrmSize>0</oStrmSize>\r\n"
            "<VODCount>0</VODCount>\r\n"
            "</VTDU_LOAD>\r\n"
            "</Message>\r\n", nInStrmSize);
        int ret = SipUA_InitOPTIONS(g_configSipServer.m_strLocalSipId.c_str(), g_configSipServer.m_strServerId.c_str(), g_configSipServer.m_strServerIP.c_str(),
            g_configSipServer.m_iServerPort, szKeepaliveXml, iXmlLen);
        if (ret < 0)
        {
            printf("send keepalive failed, server[%s@%s:%d], body: %s", g_configSipServer.m_strLocalSipId.c_str(),
                g_configSipServer.m_strServerIP.c_str(), g_configSipServer.m_iServerPort, szKeepaliveXml);
        }

        sleep(15);
    } while (g_bHeartBeatThreadWorking);
}

SipServer::SipServer()
{
    inifile::IniFile *m_pCfgFile;
    //�򿪱������õı����ļ�
    m_pCfgFile = new(std::nothrow) IniFile();
    if (NULL == m_pCfgFile)
    {
        printf("open ini file failed0\n");
    }

    //������̵߳���ע�����
    int nRet = m_pCfgFile->openini("gb.ini", IFACE_INI_PARAM_TYPE_NAME);
    if (nRet != 0)
    {
        printf("open ini file failed1\n");
    }

    int dwRet = -1;
    g_configSipServer.m_strLocalSipId = m_pCfgFile->getStringValue("local", "sipid", dwRet);
    g_configSipServer.m_strSipRegion = m_pCfgFile->getStringValue("local", "region", dwRet);
    g_configSipServer.m_strSipDomain = m_pCfgFile->getStringValue("local", "domain", dwRet);
    g_configSipServer.m_iTransport = m_pCfgFile->getIntValue("local", "transport", dwRet);
    g_configSipServer.m_strSipAddr = m_pCfgFile->getStringValue("local", "sipaddr", dwRet);
    g_configSipServer.m_iSipPort = m_pCfgFile->getIntValue("local", "sipport", dwRet);
    g_configSipServer.m_strPwd = m_pCfgFile->getStringValue("local", "pwd", dwRet);
    g_configSipServer.m_iExpires = m_pCfgFile->getIntValue("local", "expires", dwRet);
    g_configSipServer.m_iMonitorKeepalive = m_pCfgFile->getIntValue("local", "keepalive", dwRet); //�����¼�ƽ̨��keepalive��ʱʱ��
    g_configSipServer.m_iGbMode = m_pCfgFile->getIntValue("local", "gbMode", dwRet);

    g_configSipServer.m_strServerId = m_pCfgFile->getStringValue("remote", "sipid", dwRet);
    g_configSipServer.m_strServerRegion = m_pCfgFile->getStringValue("remote", "region", dwRet);
    g_configSipServer.m_strServerIP = m_pCfgFile->getStringValue("remote", "sipaddr", dwRet);
    g_configSipServer.m_iServerPort = m_pCfgFile->getIntValue("remote", "sipport", dwRet);

    g_ulMsgReqSN = 17430;
    m_nInitialUser = 1;
    //��ʼ�� ��Ա
    //g_configSipServer.m_strLocalSipId = "34020000001320000001";
    //g_configSipServer.m_strSipRegion = "3402000000";
    //g_configSipServer.m_strSipDomain = "340200";
    //g_configSipServer.m_iTransport = SIPUA_TANSPORT_UDP;
    //g_configSipServer.m_strSipAddr = "192.168.4.117";
    //g_configSipServer.m_iSipPort = 5061;
    //g_configSipServer.m_strPwd; "123456"; 
    //g_configSipServer.m_iExpires = 60;
    //g_configSipServer.m_iMonitorKeepalive = 60; //�����¼�ƽ̨��keepalive��ʱʱ��

    //g_configSipServer.m_strServerId = "34020000002000000001";
    //g_configSipServer.m_strServerRegion = "3402000000";
    //g_configSipServer.m_strServerIP = "192.168.4.110";
    //g_configSipServer.m_iServerPort = 5060;

    for (int i = 0; i < 15000; )
    {
        stPortUse stRecv;
        stRecv.bused = false;
        stRecv.nport = i + 15000;
        stPortUse stSend;
        stSend.bused = false;
        stSend.nport = i + 30000;
        vecRecvPortUse.push_back(stRecv);
        vecSendPortUse.push_back(stSend);
        i = i + 2;
    }
}

SipServer::~SipServer()
{

}
/*ý������������������ƽ̨��*/
void MediaGateStreamCB(void *pPuller, int iStreamType, unsigned char *pBuffer, int iBufSize, void *param)
{
    printf("MediaGateStreamCB call back\n");
    static FILE* file = fopen("test.h264", "wb+");
    fwrite(pBuffer,1, iBufSize, file);
}

/*���������������Զ�˹���ƽ̨��ȡ��Ƶ
@pszStreamId: ��Ƶ��ID
@pszDestId: ������豸ID
@pszDestIP, iDestPort: sip server�ĵ�ַ
@iPlaybackFlag: ��ʷ��Ƶ��ǣ�0ʵʱ��Ƶ��1��ʷ��Ƶ
@i64StartTime�� i64StopTime����ʼ������ʱ�䣬���iPlaybackFlagΪʵʱ��Ƶ��������������
@fnStreamCB, pParam: ��Ƶ���Ļص��ӿ�
*/
HMG GB28181MediaGate_CreateStreamPullerLocal(const char *pszStreamId,
    const char * pszDestId, const char * pszDestIP, int iDestPort,
    int iPlaybackFlag, long long i64StartTime, long long i64StopTime,
    fnMediaGateStreamCB fnStreamCB, void *pParam)
{
    //ENTER_FUNC_MEDIAGATE();
    printf("GB28181MediaGate_CreateStreamPullerLocal begin\n");

    CGB28181StreamPuller *pPuller = new CGB28181StreamPuller;

    int iLocalPort = 30000;
    /*�������δ򿪱������ݶ˿�*/
    int iTry = 0;
    const int iCount = 3;
    for (iTry = 0; iTry < iCount; iTry++)
    {
        int ret = pPuller->initStreamPuller(pszStreamId, "192.168.4.110", iLocalPort,
            fnStreamCB, pParam);
        if (0 != ret)
        {
            WARN_LOG_MEDIAGATE("init stream puller failed, use port 3000, will try again.\r\n");
            pPuller->releaseStreamPuller();
            continue;
        }

        break;
    }

    /*���ζ�ʧ��*/
    if (iTry == iCount)
    {
        printf("init stream puller failed, try %d count.\r\n", iTry);
        delete pPuller;
        //EXIT_FUNC_MEDIAGATE();
        return NULL;
    }

    /*subjectͷ�򣺡�ý�巢����ID�����ͷ�ý�������кţ�ý����������ID�����շ�ý�������кš�*/
    char szSubject[1024] = { 0 };
    sprintf(szSubject, "%s:%s,%s:%s", pszDestId, "0", "34020000002000000001", "0");

    char szBody[1024] = { 0 };
    const char *pszLocalIP = "192.168.4.110";
    int iBodyLen = 0;
    if (0 == iPlaybackFlag) //ʵʱ��Ƶ
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
            pszDestId,
            pszLocalIP,
            pszLocalIP,
            iLocalPort
        );
    }
    else //��ʷ��Ƶ
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
            pszDestId,
             pszLocalIP, //o=
            pszDestId, //u=
            pszLocalIP, //c=
            i64StartTime, i64StopTime, //t=
            iLocalPort //m=
        );
    }

    int cid = SipUA_InitInviteWidthBody("34020000002000000001", pszDestId, pszDestIP, iDestPort,
        szSubject, szBody, iBodyLen);
    if (cid <= 0)
    {
        printf("init invite failed, cid=%d.\n", cid);
        delete pPuller;
        //EXIT_FUNC_MEDIAGATE();
        return NULL;
    }

    pPuller->setCID(cid);

    HMG hPuller = (HMG)pPuller;
    //EXIT_FUNC_MEDIAGATE();
    return hPuller;
}
//����Catalog��Ϣ
int SipServer::handleCatalogMsg(const char *pszBody, int iBodyLen)
{
    /*�豸�б�����body��xml
    <Response>
        <CmdType>Catalog</CmdType>
        <SN>248</SN>
        <DeviceID>34020000002000000001</DeviceID>
        <SumNum>24</SumNum>
        <DeviceList Num="1">
            <Item>
                <DeviceID>34020000001310000004</DeviceID>
                <Name>..........4</Name>
                <Manufacturer>Fri</Manufacturer>
                <Model>Fri SIP Agent 1.0</Model>
                <Owner>........</Owner>
                <CivilCode>3402</CivilCode>
                <Address>........</Address>
                <Parental>0</Parental>
                <ParentID>34020000002000000001</ParentID>
                <RegisterWay>1</RegisterWay>
                <Secrecy>0</Secrecy>
                <Status>ON</Status>
            </Item>
        </DeviceList>
    </Response>
    */
    printf("handleCatalogMsg begin\n");

    TiXmlDocument doc;
    doc.Parse(pszBody);

    TiXmlElement *pXmlElementResponse = doc.FirstChildElement("Response");
    if (NULL == pXmlElementResponse)
    {
        printf("cannot find Response\r\n%s\n", pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return -1;
    }

    /*ƽ̨ID <DeviceID>*/
    TiXmlElement *pXmlElementDeviceId = pXmlElementResponse->FirstChildElement("DeviceID");
    if (NULL == pXmlElementDeviceId)
    {
        printf("cannot find Response.DeviceID\r\n%s\n", pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return -1;
    }
    std::string strSipId = pXmlElementDeviceId->GetText();

    /*�豸���� <SumNum>*/
    TiXmlElement *pXmlElementSumNum = pXmlElementResponse->FirstChildElement("SumNum");
    if (NULL == pXmlElementSumNum)
    {
        printf("cannot find Response.SumNum\r\n%s\n", pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return -1;
    }
    int iSumNum = atoi(pXmlElementSumNum->GetText());

    /*�豸�б� <DeviceList Num="1">*/
    int sDeviceListNum = -1;
    TiXmlElement *pXmlElementDeviceList = pXmlElementResponse->FirstChildElement("DeviceList");
    if (NULL == pXmlElementDeviceList)
    {
        printf("cannot find Response.DeviceList\r\n%s\n", pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return -1;
    }
    pXmlElementDeviceList->QueryIntAttribute("Num", &sDeviceListNum);
    printf("Plat SumNum: %d, DeviceListNum: %d\n", iSumNum, sDeviceListNum);

    g_callMgr.lockSipEndpoint();
    /*ȷ����catalog��Ϣ���Ѿ�ע����¼�ƽ̨�ϱ�*/
    CSipEndpointInfo *pSipEndpoint = g_callMgr.getSipEndpointInfo(strSipId);
    if (NULL == pSipEndpoint)
    {
        g_callMgr.unlockSipEndpoint();
        printf("cannot find SipEndpointInfo, sipid=%s\r\n%s\n", strSipId.c_str(), pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return -1;
    }

    //����<Item>...</Item>
    TiXmlNode* pChild = NULL;
    for (pChild = pXmlElementDeviceList->FirstChildElement(); pChild != 0; pChild = pChild->NextSiblingElement())
    {
        TiXmlElement* pItemChild = NULL;
        Json::Value jsItem;
        //����<Item>...</Item>����
        for (pItemChild = pChild->FirstChildElement(); pItemChild != 0; pItemChild = pItemChild->NextSiblingElement())
        {
            if (pItemChild->Value() && pItemChild->GetText())
            {
                std::string strVal = pItemChild->Value();
                std::string strText = pItemChild->GetText();
                //DEBUG_LOG("item value: %s, text: %s", strVal.c_str(), strText.c_str());
                std::string strItemText;
                //�ַ�����ת������ֹ��������
                if (CodeConverter_IsGBK(strText.c_str()))
                {
                    char szUtf8[4096] = { 0 };
                    if (CodeConverter_GB2312ToUtf8(strText.c_str(), strText.length(), szUtf8, 4096) <= 0)
                    {
                        //gb2312תutf8ʧ��
                        printf("convert gb2312 to utf8 faild, text=%s\n", strText.c_str());
                        strItemText = strText;
                    }
                    else
                    {
                        //gb2312תutf8�ɹ�
                        strItemText = szUtf8;
                    }
                }
                else
                {
                    strItemText = strText;
                }

                jsItem[strVal] = strItemText;
            }
        }

        pSipEndpoint->appendSipDevice(jsItem);
    }

    GB28181MediaGate_CreateStreamPullerLocal("0", strSipId.c_str(), "192.168.4.2", 5060, 0, 0,10086, MediaGateStreamCB, this);

    /**/
    int iCount = pSipEndpoint->getCatalogSize();
    if (iCount >= iSumNum)
    {
        std::string strDeviceList = pSipEndpoint->m_jsonCatalog.toStyledString();
        printf("recv all device list, count=%d.\r\n%s\n", iCount, strDeviceList.c_str());
    }

    g_callMgr.unlockSipEndpoint();

    doc.Clear();
    printf("handleCatalogMsg end\n");
    return 0;
}

void SipServer::sipServerHandleInfo(void *pMsgPtr)
{
    printf("sipServerHandleInfo begin\n");

    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleInfo end\n");
        return;
    }

    printf("recv MESSAGE, body: %s", szBody);
    std::string strCmdType = getMsgCmdTypeV3(szBody, ret);
    if ("MSG_READY_VIDEO_TRANS_REQ" == strCmdType)//����Ԥ��
    {
        sipServerHandleV3TransReady(pMsgPtr);
    }
    else if ("MSG_START_VIDEO_VTDU_ACK" == strCmdType)//ȷ��Ԥ��
    {
        sipServerHandleV3TransAck(pMsgPtr);
    }
    else if ("MSG_VTDU_STOP_VIDEO_REQ" == strCmdType)//ֹͣԤ��
    {
        sipServerHandleV3TransStop(pMsgPtr);
    }
    else if ("MSG_START_FILE_VOD_TASK_REQ" == strCmdType)//�����ط�
    {
        sipServerHandleV3FileStart(pMsgPtr);
    }
    else if ("MSG_STOP_FILE_VOD_TASK_REQ" == strCmdType)//ֹͣ�ط�
    {
        sipServerHandleV3FileStop(pMsgPtr);
    }
    else
    {
        printf("unsupport Request:%s\n", strCmdType.c_str());
    }

}

/*����MESSAGE��Ϣ*/
void SipServer::sipServerHandleMessage(void *pMsgPtr)
{
    printf("sipServerHandleMessage begin\n");

    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleMessage end\n");
        return;
    }

    printf("recv MESSAGE, body: %s", szBody);

    std::string strCmdType = getMsgCmdType(szBody, ret);
    if ("Keepalive" == strCmdType) //MESSAGE����
    {
        if (0 == handleKeepaliveMsg(szBody, ret))
        {
            SipUA_AnswerMessage(pMsgPtr, 200, NULL, 0);
        }
        else
        {
            printf("handleKeepaliveMsg failed, body: %s\n", szBody);
            SipUA_AnswerMessage(pMsgPtr, 400, NULL, 0);
        }
    }
    else if ("Catalog" == strCmdType) //MESSAGE�豸Ŀ¼
    {
        if (0 == handleCatalogMsg(szBody, ret))
        {
            SipUA_AnswerMessage(pMsgPtr, 200, NULL, 0);
        }
        else
        {
            printf("handleCatalogMsg failed, body: %s\n", szBody);
            SipUA_AnswerMessage(pMsgPtr, 400, NULL, 0);
        }
    }
    else if ("RecordInfo" == strCmdType) //¼���б�
    {
        if (0 == handleRecordInfoMsg(szBody, ret))
        {
            SipUA_AnswerMessage(pMsgPtr, 200, NULL, 0);
        }
        else
        {
            printf("handleRecordInfoMsg failed, body: %s\n", szBody);
            SipUA_AnswerMessage(pMsgPtr, 400, NULL, 0);
        }
    }
    else
    {
        printf("unhandle cmdtype %s, body: %s\n", strCmdType.c_str(), szBody);
    }

    printf("sipServerHandleMessage end\n");
}

//����¼�������Ϣ
int SipServer::handleRecordInfoMsg(const char *pszBody, int iBodyLen)
{
    printf("handleRecordInfoMsg begin\n");

    /*
    <?xml version="1.0"?>
    <Response>
    <CmdType>RecordInfo</CmdType>
    <SN>17430</SN>
    <DeviceID>34020000001310000010</DeviceID>
    <Name>xxx</Name>
    <SumNum>2</SumNum>
    <RecordList Num="1">
    <Item>
    <DeviceID>34020000001310000010</DeviceID>
    <Name>xxx</Name>
    <StartTime>2019-11-26T12:00:00</StartTime>
    <EndTime>2019-11-26T12:05:00</EndTime>
    <Secrecy>0</Secrecy>
    <Type>time</Type>
    </Item>
    </RecordList>
    </Response>
    */

    printf("recordlist: %s\n", pszBody);

    std::string strDeviceId = getXmlElementText(pszBody, iBodyLen, "DeviceID");
    if (strDeviceId.length() <= 0)
    {
        printf("get deviceid failed, body: %s\n", pszBody);
        return -1;
    }

    //�յ�¼���б��ҵ���Ӧ������INVITE���󣬰�INVITE��������
    SPlaybackCallerInfo playbackCaller;
    int ret = g_callMgr.getPlaybackCallerInfo(strDeviceId, playbackCaller);
    if (0 == ret)
    {
        initInviteMsg(playbackCaller.sdpInfo, playbackCaller.subject, playbackCaller.sHdrFrom,
            playbackCaller.tid, playbackCaller.cid, playbackCaller.did);
        g_callMgr.removePlaybackCallerInfo(strDeviceId);
    }

    printf("handleRecordInfoMsg end\n");
    return 0;
}

//�ͻ��˴���ע��ɹ���Ϣ
void SipServer::sipClientHandleRegisterSuccess(void *pMsgPtr)
{
    g_bRegisterSuccess = true;
    pthread_t tThreadId;
    pthread_create(&tThreadId, 0, threadHeartBeat, NULL);

}

//�ͻ��˴���ע��ʧ����Ϣ
void SipServer::sipClientHandleRegisterFailed(void *pMsgPtr)
{
    int nStatus = -1;
    SipUA_GetRegisterStatus(pMsgPtr, &nStatus);
    if (nStatus == 401)
    {
        char szBody[4096] = { 0 };
        int iBodyLen = sprintf(szBody,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<Message Version=\"1.0.2\">\r\n"
            "<IE_HEADER>\r\n"
            "<MessageType>MSG_VTDU_LOGIN_REQ</MessageType>\r\n"
            "<Reserved />\r\n"
            "</IE_HEADER>\r\n"
            "<IE_SERVER_REGISTER_INFO>\r\n"
            "<ServerType>VTDU</ServerType>\r\n"
            "<ServerID>%s</ServerID>\r\n"
            "<DomainID>%s</DomainID>\r\n"
            "<ServerIP>%s</ServerIP>\r\n"
            "<Version>3.1.1.13</Version>\r\n"
            "</IE_SERVER_REGISTER_INFO>\r\n"
            "<IE_VTDU_PARAM>\r\n"
            "<maxAudioInAmount>200</maxAudioInAmount>\r\n"
            "<maxVideoInAmount>200</maxVideoInAmount>\r\n"
            "<maxAudioOutAmount>200</maxAudioOutAmount>\r\n"
            "<maxVideoOutAmount>200</maxVideoOutAmount>\r\n"
            "<xDomRcvIP>%s</xDomRcvIP>\r\n"
            "</IE_VTDU_PARAM>\r\n"
            "<IE_IO_OPTION>\r\n"
            "<IP_NUM>1</IP_NUM>\r\n"
            "<IP_RECORD>\r\n"
            "<IP>%s</IP>\r\n"
            "<IN_BW>100000</IN_BW >\r\n"
            "<OUT_BW>100000</OUT_BW >\r\n"
            "</IP_RECORD>\r\n"
            "</IE_IO_OPTION>\r\n"
            "<IE_DOMAIN_LIST>\r\n"
            "<DomNUM>1</DomNUM>\r\n"
            "<Domain>\r\n"
            "<DomainID>%s</DomainID>\r\n"
            "</Domain>\r\n"
            "</IE_DOMAIN_LIST>\r\n"
            "</Message>\r\n", g_configSipServer.m_strLocalSipId.c_str(), g_configSipServer.m_strSipDomain.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_strSipDomain.c_str());

        int regid = SipUA_RegisterUnauthorV3(
            g_configSipServer.m_strLocalSipId.c_str(), g_configSipServer.m_strSipRegion.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_iSipPort, g_configSipServer.m_iExpires,
            g_configSipServer.m_strServerId.c_str(), g_configSipServer.m_strServerRegion.c_str(), g_configSipServer.m_strServerIP.c_str(),
            g_configSipServer.m_iServerPort, szBody, iBodyLen);
    }
    else
    {
        printf("reg failed status:%d\n", nStatus);
        pthread_t tThreadId;
        pthread_create(&tThreadId, 0, threadRegister, NULL);
        //ע��ʧ�� ������ʱע���߳� tbd
    }
}

/*����ע����Ϣ���µ�ע��*/
void SipServer::sipServerHandleRegisterNew(void *pMsgPtr)
{
    //��ȡ�ͻ���ע����Ϣ
    SRegisterInfo regInfo;
    SipUA_GetRegisterInfo(pMsgPtr, &regInfo);
    printf("contact[%s@%s:%d], callid[%s], user[%s], realm[%s], nonce[%s], uri[%s], method[%s], response[%s], expires[%d]\n",
        regInfo.szRegUser, regInfo.szContactIP, regInfo.iContactPort, regInfo.szCallId,
        regInfo.szAuthorUser, regInfo.szRealm, regInfo.szNonce, regInfo.szUri,
        regInfo.szMethod, regInfo.szResponse, regInfo.expires);

    char szRegBody[4096] = { 0 };
    int iBodyLen = SipUA_GetRequestBodyContent(pMsgPtr, szRegBody, 4096);
    if (iBodyLen > 0) /*ע��Я����Ϣ�壬��device cloud*/
    {
        printf("device cloud register with body: %s\n", szRegBody);
        SipUA_RegisterReply200OK(pMsgPtr, g_configSipServer.m_strLocalSipId.c_str());
        g_callMgr.lockSipEndpoint();
        g_callMgr.insertEndpoint(&regInfo, SIP_ENDPOINT_DEVICECLOUD);
        g_callMgr.unlockSipEndpoint();
        return;
    }

    if (0 == regInfo.ucAuthorFlag) /*ע��δ��Ȩ���ظ�401*/
    {
        const char *pszRealm = "c1d243976057fe91";
        const char *pszNonce = "839cae39ffe92bda";

        SipUA_RegisterReply401Unauthorized(pMsgPtr, pszRealm, pszNonce);
    }
    else /*ע�����Ȩ���ظ�200*/
    {
        printf("register ok, contact[%s@%s:%d], user[%s], realm[%s], nonce[%s], uri[%s], method[%s], response[%s], expires[%d]\n",
            regInfo.szRegUser, regInfo.szContactIP, regInfo.iContactPort,
            regInfo.szAuthorUser, regInfo.szRealm, regInfo.szNonce, regInfo.szUri,
            regInfo.szMethod, regInfo.szResponse, regInfo.expires);
        SipUA_RegisterReply200OK(pMsgPtr, g_configSipServer.m_strLocalSipId.c_str()); //�ظ�200 OK

        std::string strSipId = regInfo.szRegUser;
        int iUpdateCatalog = 0;
        if (0 == regInfo.expires) //ע������
        {
            //��������
            g_callMgr.removeCallContextBySipId(strSipId);
            //�ͷ�sip�ͻ�����Ϣ
            g_callMgr.removeEndpoint(strSipId);
            printf("sip endpoint[%s] logout.\n", strSipId.c_str());
        }
        else
        {
            g_callMgr.lockSipEndpoint();
            /*ע��ɹ��������¼�ƽ̨ע����Ϣ*/
            iUpdateCatalog = g_callMgr.insertEndpoint(&regInfo, SIP_ENDPOINT_GB_PLATFORM);
            g_callMgr.unlockSipEndpoint();
        }

        if (1 == iUpdateCatalog)
        {
            /*��ѯ�豸Ŀ¼*/
            char szCatalog[1024] = { 0 };
            int iBodyLen = sprintf(szCatalog,
                "<?xml version=\"1.0\"?>\r\n"
                "<Query>\r\n"
                "<CmdType>Catalog</CmdType>\r\n"
                "<SN>17430</SN>\r\n"
                "<DeviceID>%s</DeviceID>\r\n"
                "</Query>\r\n", regInfo.szRegUser);
            SipUA_InitMessage(g_configSipServer.m_strLocalSipId.c_str(), regInfo.szRegUser,
                regInfo.szContactIP, regInfo.iContactPort, szCatalog, iBodyLen);
        }
    }

}

/*����ע����Ϣ��ˢ��ע��*/
void SipServer::sipServerHandleRegisterRefresh(void *pMsgPtr)
{

}


/*����invite��Ϣ*/
void SipServer::sipServerHandleCallInviteIncoming(void *pMsgPtr)
{
    printf("sipServerHandleCallInviteIncoming begin\n");

    SFromHeader sHdrFrom;
    SipUA_GetRequestFromHeader(pMsgPtr, &sHdrFrom);
    SSdpInfo sdpInfo;
    SipUA_GetRequestSdpInfo(pMsgPtr, &sdpInfo);
    char szReqBody[4096] = { 0 };
    SipUA_GetRequestBodyContent(pMsgPtr, szReqBody, 4096);
    SSubjectInfo subject;
    SipUA_GetRequestSubject(pMsgPtr, &subject);

    printf("invite incomming, subject sender[%s:%s], recver[%s:%s].\r\n%s\n", subject.szSenderId, subject.szSenderStreamSeq,
        subject.szRecverId, subject.szRecverStreamSeq, szReqBody);

    std::string strS = sdpInfo.szSessionName; //s=Play / Playback 
    if ("Play" == strS) //ʵʱ��Ƶ����
    {
        handleInviteRealPlay(sdpInfo, subject, sHdrFrom, pMsgPtr);
    }
    else if ("Playback" == strS) //¼��ط�����
    {
        handleInvitePlayback(sdpInfo, subject, sHdrFrom, pMsgPtr);
    }
    else
    {
        printf("invite sdp error, s=%s, sdp info:\r\n%s\n", strS.c_str(), szReqBody);
        int tid = SipUA_GetCallTransId(pMsgPtr);
        SipUA_AnswerInviteFailure(tid, 400);
    }

    printf("sipServerHandleCallInviteIncoming end\n");

}

/*����INVITE 200��Ӧ*/
void SipServer::sipServerHandleCallAnswer200OK(void *pMsgPtr)
{
    printf("sipServerHandleCallAnswer200OK begin\n");
    /*INVITE 200 OK��Ϣֻ�б��вŻ�ظ�*/
    int iCalleeCid = SipUA_GetCallCallId(pMsgPtr);
    /*ͨ�����к����ȣ��ҵ�����������*/
    g_callMgr.lockCallContext();

    SCallContext *pCtx = g_callMgr.getCallContextByCallLeg(iCalleeCid, CALL_LEG_TYPE_CALLEE);
    if (NULL == pCtx)
    {
        printf("cannot find call context by callee leg[%d].\n", iCalleeCid);
        printf("sipServerHandleCallAnswer200OK end\n");
        g_callMgr.unlockCallContext();
        return;
    }

    int iCallerTid = pCtx->sCaller.tid;
    SSdpInfo sdpInfo;
    SipUA_GetResponseSdpInfo(pMsgPtr, &sdpInfo);
    /*���е�200 OKת��������*/
    int ret = SipUA_AnswerInvite200OK(iCallerTid, &sdpInfo);
    if (0 == ret)
    {
        /*���汻�е�tid, did*/
        pCtx->sCallee.tid = SipUA_GetCallTransId(pMsgPtr);
        pCtx->sCallee.did = SipUA_GetCallDialogsId(pMsgPtr);
        memcpy(&pCtx->sCallee.sVideoMediaInfo, &sdpInfo.sVideoMediaInfo, sizeof(SMediaInfo));
        memcpy(&pCtx->sCallee.sAudioMediaInfo, &sdpInfo.sAudioMediaInfo, sizeof(SMediaInfo));
    }
    else
    {
        /*�����лظ�200 OKʧ��*/
        printf("answer caller 200 ok failed, ret=%d.\n", ret);
    }

    g_callMgr.unlockCallContext();

    printf("sipServerHandleCallAnswer200OK end\n");
}

/*����INVITE ������200��Ӧ*/
void SipServer::sipServerHandleCallAnswerResponse(void *pMsgPtr)
{
    printf("sipServerHandleCallAnswerResponse begin\n");
    /*ֻ�б��вŻ�ظ�*/
    int iCalleeCid = SipUA_GetCallCallId(pMsgPtr);
    /*ͨ�����к����ȣ��ҵ�����������*/
    SCallContext *pCtx = g_callMgr.getCallContextByCallLeg(iCalleeCid, CALL_LEG_TYPE_CALLEE);
    if (NULL == pCtx)
    {
        printf("cannot find call context by callee leg[%d].\n", iCalleeCid);
        printf("sipServerHandleCallAnswerResponse end\n");
        return;
    }

    printf("callee answer failure, caller[tid:%d, did:%d, cid:%d], callee[tid:%d, did:%d, cid:%d]\n",
        pCtx->sCaller.tid, pCtx->sCaller.did, pCtx->sCaller.cid,
        pCtx->sCallee.tid, pCtx->sCallee.did, pCtx->sCallee.cid);
    //�����лظ�400������ʧ��
    int iCallerTid = pCtx->sCaller.tid;
    int ret = SipUA_AnswerInviteFailure(iCallerTid, 400);
    if (0 != ret)
    {
        printf("answer caller 400 failed, CallerTid=%d\n", iCallerTid);
    }

    g_callMgr.removeCallContext(pCtx);

    printf("sipServerHandleCallAnswerResponse end\n");
}

/*����ACK��Ӧ*/
void SipServer::sipSendCallAck(void *pMsgPtr)
{
    printf("sipSendCallAck begin\n");
    /*ACK��Ϣֻ�����вŻᷢ�����򱻽лظ�ACK*/
    int iDid = SipUA_GetCallDialogsId(pMsgPtr);
    SipUA_SendAck(iDid);
    printf("sipSendCallAck end\n");
}


/*����ACK��Ӧ*/
void SipServer::sipServerHandleCallAck(void *pMsgPtr)
{
    printf("sipServerHandleCallAck begin\n");
    /*ACK��Ϣֻ�����вŻᷢ�����򱻽лظ�ACK*/
    int iCallerCid = SipUA_GetCallCallId(pMsgPtr);
    SCallContext *pCtx = g_callMgr.getCallContextByCallLeg(iCallerCid, CALL_LEG_TYPE_CALLER);
    if (NULL == pCtx)
    {
        printf("cannot find call context by caller leg[%d].\n", iCallerCid);
        printf("sipServerHandleCallAck end\n");
        return;
    }

    SipUA_SendAck(pCtx->sCallee.did);
    printf("sipServerHandleCallAck end\n");
}

/*����BYE��Ϣ*/
void SipServer::sipServerHandleCallBye(void *pMsgPtr)
{
    printf("sipServerHandleCallBye begin\n");

    //SipUA��������
    int cid = SipUA_GetCallCallId(pMsgPtr);
    g_callMgr.lockCallContext();
    SCallContext *pCtx = g_callMgr.getCallContext(cid);
    if (NULL == pCtx)
    {
        printf("cannot find call context by cid[%d].\n", cid);
        g_callMgr.unlockCallContext();
        printf("sipServerHandleCallBye end\n");
        return;
    }
    SipUA_TerminateCall(pCtx->sCaller.cid, pCtx->sCaller.did);
    SipUA_TerminateCall(pCtx->sCallee.cid, pCtx->sCallee.did);
    g_callMgr.unlockCallContext();

    //�ͷź���������
    g_callMgr.removeCallContext(pCtx);

    printf("sipServerHandleCallBye end\n");
}

//����v3Ԥ������
void SipServer::sipServerHandleV3TransReady(void *pMsgPtr)
{
    int nStatus = 400;
    std::string strError = "";
    stMediaInfo stCurMediaInfo;
    do 
    {
        char szBody[4096] = { 0 };
        int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
        if (ret <= 0)
        {
            printf("SipUA_GetRequestBodyContent get body failed.\n");
            strError = "get msg body failed";
            break;
        }

        //������Ϣ�� / ��ȡ���ؽ��շ��Ͷ˿�
        TiXmlDocument doc;
        doc.Parse(szBody);

        //cu
        TiXmlElement *pXmlElementCuInfo = doc.FirstChildElement("IE_CU_ADDRINFO");
        if (NULL == pXmlElementCuInfo)
        {
            printf("cannot find IE_CU_ADDRINFO\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find IE_CU_ADDRINFO";
            break;
        }

        TiXmlElement *pXmlElementCuUserID = pXmlElementCuInfo->FirstChildElement("UserID");
        if (NULL == pXmlElementCuUserID)
        {
            printf("cannot find UserID\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU UserID";
            break;
        }
        stCurMediaInfo.strCuUserID = pXmlElementCuUserID->GetText();

        TiXmlElement *pXmlElementCuIP = pXmlElementCuInfo->FirstChildElement("IP");
        if (NULL == pXmlElementCuIP)
        {
            printf("cannot find CU IP\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU IP";
            break;
        }
        stCurMediaInfo.strCuIp = pXmlElementCuIP->GetText();

        TiXmlElement *pXmlElementCuPort = pXmlElementCuInfo->FirstChildElement("Port");
        if (NULL == pXmlElementCuPort)
        {
            printf("cannot find CU Port\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU Port";
            break;
        }
        std::string strCuPort = pXmlElementCuPort->GetText();
        stCurMediaInfo.nCuport = atoi(strCuPort.c_str());

        TiXmlElement *pXmlElementCuNat = pXmlElementCuInfo->FirstChildElement("Nat");
        if (NULL == pXmlElementCuNat)
        {
            printf("cannot find CU NAT\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU NAT";
            break;
        }
        std::string strCuNat = pXmlElementCuNat->GetText();
        stCurMediaInfo.nCuNat = atoi(strCuNat.c_str());

        TiXmlElement *pXmlElementCuTransType = pXmlElementCuInfo->FirstChildElement("TransType");
        if (NULL == pXmlElementCuTransType)
        {
            printf("cannot find CU TransType\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU TransType";
            break;
        }
        std::string strCuTransType = pXmlElementCuTransType->GetText();
        stCurMediaInfo.nCuTransType = atoi(strCuTransType.c_str());

        std::string strCuUserType = "";
        TiXmlElement *pXmlElementCuUserType = pXmlElementCuInfo->FirstChildElement("UserType");
        if (NULL == pXmlElementCuUserType)
        {
            printf("cannot find CU UserType\r\n%s\n", szBody);
        }
        else
        {
            strCuUserType = pXmlElementCuUserType->GetText();
            stCurMediaInfo.nCuUserType = atoi(strCuUserType.c_str());
        }

        TiXmlElement *pXmlElementCuGBVideoTransMode = pXmlElementCuInfo->FirstChildElement("GBVideoTransMode");
        if (NULL == pXmlElementCuGBVideoTransMode)
        {
            printf("cannot find CU GBVideoTransMode\r\n%s\n", szBody);
        }
        else
        {
            stCurMediaInfo.strCuGBVideoTransMode = pXmlElementCuGBVideoTransMode->GetText();
        }
        
        TiXmlElement *pXmlElementCuMSCheck = pXmlElementCuInfo->FirstChildElement("MSCheck");
        if (NULL == pXmlElementCuMSCheck)
        {
            printf("cannot find CU MSCheck\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find CU MSCheck";
            break;
        }
        std::string strCuMSCheck = pXmlElementCuMSCheck->GetText();
        stCurMediaInfo.nCuMSCheck = atoi(strCuMSCheck.c_str());
            

        //pu
        TiXmlElement *pXmlElementPuInfo = doc.FirstChildElement("IE_PU_ADDRINFO");
        if (NULL == pXmlElementPuInfo)
        {
            printf("cannot find IE_PU_ADDRINFO\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find IE_PU_ADDRINFO";
            break;
        }

        TiXmlElement *pXmlElementPuPUID = pXmlElementPuInfo->FirstChildElement("PUID");
        if (NULL == pXmlElementPuPUID)
        {
            printf("cannot find PUID\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PUID";
            break;
        }
        stCurMediaInfo.strPUID = pXmlElementPuPUID->GetText();

        TiXmlElement *pXmlElementPuChannelID = pXmlElementPuInfo->FirstChildElement("ChannelID");
        if (NULL == pXmlElementPuChannelID)
        {
            printf("cannot find PU ChannelID\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PU ChannelID";
            break;
        }
        std::string strPuChannelID = pXmlElementPuChannelID->GetText();
        stCurMediaInfo.nPuChannelID = atoi(strPuChannelID.c_str());

        TiXmlElement *pXmlElementPuProtocol = pXmlElementPuInfo->FirstChildElement("Protocol");
        if (NULL == pXmlElementPuProtocol)
        {
            printf("cannot find PU Protocol\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PU Protocol";
            break;
        }
        std::string strPuProtocol = pXmlElementPuProtocol->GetText();
        stCurMediaInfo.nPuProtocol = atoi(strPuProtocol.c_str());

        TiXmlElement *pXmlElementPuStreamType = pXmlElementPuInfo->FirstChildElement("StreamType");
        if (NULL == pXmlElementPuStreamType)
        {
            printf("cannot find PU StreamType\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PU StreamType";
            break;
        }
        std::string strPuStreamType = pXmlElementPuStreamType->GetText();
        stCurMediaInfo.nPuStreamType = atoi(strPuStreamType.c_str());

        TiXmlElement *pXmlElementPuTransType = pXmlElementPuInfo->FirstChildElement("TransType");
        if (NULL == pXmlElementPuTransType)
        {
            printf("cannot find PU TransType\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PU TransType";
            break;
        }
        std::string strPuTransType = pXmlElementPuTransType->GetText();
        stCurMediaInfo.nPuTransType = atoi(strPuTransType.c_str());

        TiXmlElement *pXmlElementPuFCode = pXmlElementPuInfo->FirstChildElement("FCode");
        if (NULL == pXmlElementPuFCode)
        {
            printf("cannot find FCode\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find PU FCode";
            break;
        }
        std::string strPuFCode = pXmlElementPuFCode->GetText();
        stCurMediaInfo.nPuFCode = atoi(strPuFCode.c_str());

        //IE_VTDU_ROUTE
        TiXmlElement *pXmlElementVtduRoute = doc.FirstChildElement("IE_VTDU_ROUTE");
        if (NULL == pXmlElementVtduRoute)
        {
            printf("cannot find IE_VTDU_ROUTE\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find IE_VTDU_ROUTE";
            break;
        }

        TiXmlElement *pXmlElementVtduRecvIP = pXmlElementVtduRoute->FirstChildElement("RecvIP");
        if (NULL == pXmlElementVtduRecvIP)
        {
            printf("cannot find RecvIP\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find RecvIP";
            break;
        }
        stCurMediaInfo.strVtduRecvIp = pXmlElementVtduRecvIP->GetText();

        TiXmlElement *pXmlElementVtduSendIP = pXmlElementVtduRoute->FirstChildElement("SendIP");
        if (NULL == pXmlElementVtduSendIP)
        {
            printf("cannot find SendIP\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find SendIP";
            break;
        }
        stCurMediaInfo.strVtduSendIP = pXmlElementVtduSendIP->GetText();

        //IE_OPEN_VIDEO
        TiXmlElement *pXmlElementOpenVideo = doc.FirstChildElement("IE_OPEN_VIDEO");
        if (NULL == pXmlElementOpenVideo)
        {
            printf("cannot find IE_OPEN_VIDEO\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find IE_OPEN_VIDEO";
            break;
        }

        TiXmlElement *pXmlElementOpenVideoStreamType = pXmlElementOpenVideo->FirstChildElement("StreamType");
        if (NULL == pXmlElementOpenVideoStreamType)
        {
            printf("cannot find OpenVideoStreamType\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find OpenVideoStreamType";
            break;
        }
        std::string strOpenVideoStreamType = pXmlElementOpenVideoStreamType->GetText();
        stCurMediaInfo.nOpenVideoStreamType = atoi(strOpenVideoStreamType.c_str());

        TiXmlElement *pXmlElementOpenVideoGBVideoTransMode = pXmlElementOpenVideo->FirstChildElement("GBVideoTransMode");
        if (NULL == pXmlElementOpenVideoGBVideoTransMode)
        {
            printf("cannot find GBVideoTransMode\r\n%s\n", szBody);
            doc.Clear();
            strError = "cannot find GBVideoTransMode";
            break;
        }
        stCurMediaInfo.strOpenVideoGBVideoTransMode = pXmlElementOpenVideoGBVideoTransMode->GetText();

        bool bFindRecvPort = false;
        for (std::vector<stPortUse>::iterator itorRecv = vecRecvPortUse.begin(); itorRecv != vecRecvPortUse.end(); ++itorRecv)
        {
            if (itorRecv->bused = false)
            {
                itorRecv->bused = true;
                stCurMediaInfo.nVtduRecvPort = itorRecv->nport;
                bFindRecvPort = true;
                break;
            }
        }
        if (!bFindRecvPort)
        {
            nStatus = 400;
            printf("no recv port can use\n");
            strError = "no recv port can use";
            break;
        }
        bool bFindSendPort = false;
        for (std::vector<stPortUse>::iterator itorSend = vecSendPortUse.begin(); itorSend != vecSendPortUse.end(); ++itorSend)
        {
            if (itorSend->bused = false)
            {
                itorSend->bused = true;
                stCurMediaInfo.nVtduSendPort = itorSend->nport;
                bFindSendPort = true;
                break;
            }
        }
        if (!bFindSendPort)
        {
            nStatus = 400;
            vecRecvPortUse[stCurMediaInfo.nVtduRecvPort].bused = false;
            printf("no send port can use\n");
            strError = "no send port can use";
            break;
        }
        //����˿ڣ�newһ�����󣬶����������շ��̡߳��ȴ�ack�������߳�
        //ý����Ϣ����
        nStatus = 200;
    } while (0);
    
    char pszBody[4096] = { 0 };
    int iBodyLen = 0;
    if (nStatus == 200)
    {
        
        iBodyLen = sprintf(pszBody, "error info: %s", strError.c_str());
    }
    else
    {
        iBodyLen = sprintf(pszBody, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<MessageVersion=\"1.0.2\">\r\n"
            "<IE_HEADER>\r\n"
            "<MessageType>MSG_READY_VIDEO_TRANS_RESP</MessageType>\r\n"
            "</IE_HEADER>\r\n"
            "<IE_CU_ADDRINFO>\r\n"
            "< UserID>%s</UserID>\r\n"
            "<IP>>%s</IP>\r\n"
            "<Port>%d</Port>\r\n"
            "<Nat>%d</Nat>\r\n"
            "<TransType>%d</TransType>\r\n"
            "<MSCheck>%d</MSCheck>\r\n"
            "<VtduToCuPort>%d</VtduToCuPort>\r\n"
            "<InitialUser>%d</InitialUser>\r\n"
            "</IE_CU_ADDRINFO>\r\n"
            "<IE_PU_ADDRINFO>\r\n"
            "<PUID>%s</PUID>\r\n"
            "<ChannelID>%d</ChannelID>\r\n"
            "<Protocol>%d</Protocol>\r\n"
            "<StreamType>%d</StreamType>\r\n"
            "<TransType>%d</TransType>\r\n"
            "<FCode>%d</FCode>\r\n"
            "<PuToVtduPort>%d</PuToVtduPort>\r\n"
            "</IE_PU_ADDRINFO>\r\n"
            "<IE_VTDU_ROUTE>\r\n"
            "<RecvIP>%s</RecvIP>\r\n"
            "<SendIP>%s</SendIP>\r\n"
            "</IE_VTDU_ROUTE>\r\n"
            "<IE_OPEN_VIDEO>\r\n"
            "<StreamType>%d</StreamType> \r\n"
            "<GBVideoTransMode>%s</GBVideoTransMode>\r\n"
            "</IE_OPEN_VIDEO>\r\n"
            "<IE_RESULT>\r\n"
            "<ErrorCode>0</ErrorCode>\r\n"
            "<ErrorMessage/>\r\n"
            "</IE_RESULT>\r\n"
            "<VTDU_LOAD_STATE>\r\n"
            "<State/>\r\n"
            "</VTDU_LOAD_STATE>\r\n"
            "</Message>\r\n", stCurMediaInfo.strCuUserID.c_str(), stCurMediaInfo.strCuIp.c_str(), stCurMediaInfo.nCuport, stCurMediaInfo.nCuNat, stCurMediaInfo.nCuTransType, stCurMediaInfo.nCuMSCheck,
            stCurMediaInfo.nVtduSendPort, m_nInitialUser, stCurMediaInfo.strPUID.c_str(), stCurMediaInfo.nPuChannelID, stCurMediaInfo.nPuProtocol, stCurMediaInfo.nPuStreamType, stCurMediaInfo.nPuTransType,
            stCurMediaInfo.nPuFCode, stCurMediaInfo.nVtduRecvPort, stCurMediaInfo.strVtduRecvIp.c_str(), stCurMediaInfo.strVtduSendIP.c_str(), stCurMediaInfo.nOpenVideoStreamType,
            stCurMediaInfo.strOpenVideoGBVideoTransMode.c_str());
        m_nInitialUser++;
    }

    //�ظ�״̬
    SipUA_AnswerInfo(pMsgPtr, nStatus, pszBody, iBodyLen);
}

//����v3Ԥ��ȷ������
void SipServer::sipServerHandleV3TransAck(void *pMsgPtr)
{
    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleInfo end\n");
        return;
    }
    //tbd ������Ϣ��
    //ý����Ϣ��ѯ
    //�½��շ����󣬴洢���󣬿�ʼ�շ�

}

//����v3Ԥ��ֹͣ����
void SipServer::sipServerHandleV3TransStop(void *pMsgPtr)
{
    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleInfo end\n");
        return;
    }
    //tbd ������Ϣ��
    //ý����Ϣ��ѯ/ɾ��
    //ֹͣ�շ��߳�

    //�ظ�״̬
}

//����v3�طſ�ʼ����
void SipServer::sipServerHandleV3FileStart(void *pMsgPtr)
{
    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleInfo end\n");
        return;
    }
    //tbd ������Ϣ��
    //ý����Ϣ�洢
    //��ʼ�շ��߳�

    //�ظ�״̬
}

//����v3�ط�ֹͣ����
void SipServer::sipServerHandleV3FileStop(void *pMsgPtr)
{
    char szBody[4096] = { 0 };
    int ret = SipUA_GetRequestBodyContent(pMsgPtr, szBody, 4096);
    if (ret <= 0)
    {
        printf("SipUA_GetRequestBodyContent get body failed.\n");
        printf("sipServerHandleInfo end\n");
        return;
    }
    //tbd ������Ϣ��
    //ý����Ϣ��ѯ/ɾ��
    //ֹͣ�շ��߳�

    //�ظ�״̬
}

/*
��ȡxml������µ�element text
*/
std::string SipServer::getXmlElementText(const char *pszXml, int iXmlLen, const char *pszValue)
{
    std::string strText;
    TiXmlDocument doc;
    doc.Parse(pszXml);

    TiXmlElement *root = doc.FirstChildElement();  //��ȡ���ڵ�Ԫ��
    if (NULL == root)
    {
        printf("xml format error, %s\n", pszXml);
        return strText;
    }

    for (TiXmlElement *pNode = root->FirstChildElement(); pNode; pNode = pNode->NextSiblingElement())
    {
        std::string strValue = pNode->Value();
        std::string strInValue = pszValue;
        if (strInValue == strValue)
        {
            if (pNode->GetText())
            {
                strText = pNode->GetText();
            }
            break;
        }
    }

    doc.Clear();
    return strText;
}

/*
��MESSAGE��Ϣ���ҵ�<CmdType></CmdType>
*/
std::string SipServer::getMsgCmdTypeV3(const char *pszBody, int iBodyLen)
{
    std::string strCmd = "";
    TiXmlDocument doc;
    doc.Parse(pszBody);

    TiXmlElement *pXmlElementHeader = doc.FirstChildElement("IE_HEADER");
    if (NULL == pXmlElementHeader)
    {
        printf("cannot find IE_HEADER\r\n%s\n", pszBody);
        doc.Clear();
        return strCmd;
    }

    TiXmlElement *pXmlElementMessageType = pXmlElementHeader->FirstChildElement("MessageType");
    if (NULL == pXmlElementMessageType)
    {
        printf("cannot find pXmlElementMessageType\r\n%s\n", pszBody);
        doc.Clear();
        printf("handleCatalogMsg end\n");
        return strCmd;
    }

    strCmd = pXmlElementMessageType->GetText();

    return strCmd;
}

/*
��MESSAGE��Ϣ���ҵ�<CmdType></CmdType>
*/
std::string SipServer::getMsgCmdType(const char *pszBody, int iBodyLen)
{
    std::string strCmdType = getXmlElementText(pszBody, iBodyLen, "CmdType");
    if (strCmdType.length() <= 0)
    {
        printf("get cmdtype failed, length<=0\n");
    }

    return strCmdType;
}

//����Keepalive��Ϣ
int SipServer::handleKeepaliveMsg(const char *pszBody, int iBodyLen)
{
    printf("handleKeepaliveMsg begin\n");
    std::string strSipId = getXmlElementText(pszBody, iBodyLen, "DeviceID");
    int ret = g_callMgr.updateSipEndpointKeepalive(strSipId);
    if (0 == ret)
    {
        printf("sip endpoint[%s] update keepalive time.\n", strSipId.c_str());
        printf("handleKeepaliveMsg end\n");
        return 0;
    }
    else
    {
        printf("gb28181 platform unregister. sipid=%s\n", strSipId.c_str());
    }
    printf("handleKeepaliveMsg end\n");
    return -1;
}

int SipServer::initInviteMsg(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, int tid, int cid, int did)
{
    g_callMgr.lockSipEndpoint();

    //INVITE����ת����������ƽ̨	
    std::string strDevId = subject.szSenderId;
    CSipEndpointInfo *pGb28181Platform = g_callMgr.queryGb28181PlatformInfo(strDevId);
    if (pGb28181Platform)
    {
        printf("invite sendto gb28181 platform[%s@%s:%d]\n", pGb28181Platform->sRegInfo.szRegUser,
            pGb28181Platform->sRegInfo.szContactIP, pGb28181Platform->sRegInfo.iContactPort);

        /*INVITE��Ϣת������������ƽ̨*/
        int iInviteRet = SipUA_InitInviteWidthSdpInfo(g_configSipServer.m_strLocalSipId.c_str(),
            strDevId.c_str(), pGb28181Platform->sRegInfo.szContactIP, pGb28181Platform->sRegInfo.iContactPort,
            &subject, &sdpInfo);
        if (iInviteRet > 0)
        {
            int iCalleeCid = iInviteRet;
            SCallContext *pCallCtx = g_callMgr.newCallContext(cid, iCalleeCid);
            if (pCallCtx)
            {
                //�������е�SDP��Ϣ
                memcpy(&pCallCtx->sCaller.sVideoMediaInfo, &sdpInfo.sVideoMediaInfo, sizeof(SMediaInfo));
                memcpy(&pCallCtx->sCaller.sAudioMediaInfo, &sdpInfo.sAudioMediaInfo, sizeof(SMediaInfo));
                //�����������е�sipid
                strncpy(pCallCtx->sCaller.szSipEndpointId, sHdrFrom.szUser, strlen(sHdrFrom.szUser));
                strncpy(pCallCtx->sCallee.szSipEndpointId, pGb28181Platform->sRegInfo.szRegUser, strlen(pGb28181Platform->sRegInfo.szRegUser));
            }
            else
            {
                printf("new call context failed, return null.\n");
            }

            //update������tid, did			
            g_callMgr.updateCallerCallLeg(cid, tid, did);
        }
        else
        {
            printf("invite callee failed, ret=%d.\n", iInviteRet);
        }
    }
    else
    {
        printf("cannot find gb28181 platform, sender[%s:%s], recver[%s:%s]\n", subject.szSenderId, subject.szSenderStreamSeq,
            subject.szRecverId, subject.szRecverStreamSeq);
        SipUA_AnswerInviteFailure(tid, 404);
    }

    g_callMgr.unlockSipEndpoint();

    return 0;
}

/*����ʵʱ��Ƶ��INVITE����*/
void SipServer::handleInviteRealPlay(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr)
{
    printf("handleInviteRealPlay begin\n");
    int cid = SipUA_GetCallCallId(pMsgPtr);
    int tid = SipUA_GetCallTransId(pMsgPtr);
    int did = SipUA_GetCallDialogsId(pMsgPtr);

    initInviteMsg(sdpInfo, subject, sHdrFrom, tid, cid, did);

    printf("handleInviteRealPlay end\n");
}

/*������ʷ��Ƶ��INVITE����*/
void SipServer::handleInvitePlayback(SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom, void *pMsgPtr)
{
    printf("handleInvitePlayback begin\n");

    g_callMgr.lockSipEndpoint();
    std::string strDevId = subject.szSenderId;
    CSipEndpointInfo *pGb28181Platform = g_callMgr.queryGb28181PlatformInfo(strDevId);
    if (pGb28181Platform)
    {
        char szStartTime[1024] = { 0 };
        char szEndTime[1024] = { 0 };
        Comm_UnixTime2StrTime(sdpInfo.i64TimeBegin, szStartTime);
        Comm_UnixTime2StrTime(sdpInfo.i64TimeEnd, szEndTime);

        /*����¼��ǰ������Ҫ��ѯ¼�񣬷�RecordInfo��message��Ϣ������*/
        char szReqBody[1024] = { 0 };
        int iBodyLen = sprintf(szReqBody,
            "<?xml version=\"1.0\"?>\r\n"
            "<Query>\r\n"
            "<CmdType>RecordInfo</CmdType>\r\n"
            "<SN>%ld</SN>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<StartTime>%s</StartTime>\r\n"
            "<EndTime>%s</EndTime>\r\n"
            "<FilePath>%s</FilePath>\r\n"
            "<Address>address1</Address>\r\n"
            "<Secrecy>0</Secrecy>\r\n"
            "<Type>time</Type>\r\n"
            "</Query>\r\n",
            g_ulMsgReqSN,
            subject.szSenderId,
            szStartTime,
            szEndTime,
            pGb28181Platform->sRegInfo.szRegUser);

        /*����������Ϣ���ȱ���RecordInfo��message����ظ����ٰ��������󷢵����ж�*/
        int cid = SipUA_GetCallCallId(pMsgPtr);
        int tid = SipUA_GetCallTransId(pMsgPtr);
        int did = SipUA_GetCallDialogsId(pMsgPtr);
        g_callMgr.insertPlaybackCallerInfo(strDevId, tid, did, cid, sdpInfo, subject, sHdrFrom);

        SipUA_InitMessage(g_configSipServer.m_strLocalSipId.c_str(), pGb28181Platform->sRegInfo.szRegUser,
            pGb28181Platform->sRegInfo.szContactIP, pGb28181Platform->sRegInfo.iContactPort,
            szReqBody, iBodyLen);
        printf("send message request recordinfo: %s", szReqBody);

        //SN����
        g_ulMsgReqSN++;

    }
    else
    {
        printf("cannot find gb28181 platform, sender[%s:%s], recver[%s:%s]\n", subject.szSenderId, subject.szSenderStreamSeq,
            subject.szRecverId, subject.szRecverStreamSeq);
        int tid = SipUA_GetCallTransId(pMsgPtr);
        SipUA_AnswerInviteFailure(tid, 404);
    }

    g_callMgr.unlockSipEndpoint();

    printf("handleInvitePlayback end\n");
}

int SipServer::StartSipServer(int transport, int iListenPort, const char *pszSrcAddr, int iSrcPort,
    fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB, int nWorkMode)
{
    /*��ʼ��SIPUA*/
    int ret = 0;
    ret = SipUA_Init(transport, iListenPort,
            pszSrcAddr,
            iSrcPort, fnLogCB, pParamLogCB);
    if (0 != ret)
    {
        printf("init sipua failed. transport[%d], addr[%s:%d]\n", transport,
            pszSrcAddr, iSrcPort);
        return -1;
    }

    ret = g_callMgr.initCallManager();
    if (0 != ret)
    {
        printf("init call manager failed\n");
        //return -1;
    }

    ret = SipUA_StartEventLoop(SipServerEventCB, this);
    if (0 != ret)
    {
        printf("sipua start event loop failed.\n");
        return -1;
    }

    /*ý��������Ϊ�¼�������ע�ᵽ�ϼ�ƽ̨*/
    if (GB28181_MODE_SIP_CLIENT == nWorkMode)
    {
        int regid = SipUA_RegisterUnauthorV3(
            g_configSipServer.m_strLocalSipId.c_str(), g_configSipServer.m_strSipRegion.c_str(), g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_iSipPort, g_configSipServer.m_iExpires,
            g_configSipServer.m_strServerId.c_str(), g_configSipServer.m_strServerRegion.c_str(), g_configSipServer.m_strServerIP.c_str(),
            g_configSipServer.m_iServerPort, NULL, 0);
    }

    return 0;

}

int SipServer::StopSipServer()
{
    SipUA_StopEventLoop();
    SipUA_Release();

    return 0;
}
