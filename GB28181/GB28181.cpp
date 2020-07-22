// GB28181.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "SipServer.h"
extern CConfigSipServer g_configSipServer;
void LogCB(int iLogLevel, const char *fn, const char *szFile, const int iLine, const char* pszLogInfo, void *pParamCB)
{
    printf("UA LogCB %s\n", pszLogInfo);
}
int main()
{
    SipServer oSipServer;
    oSipServer.StartSipServer(g_configSipServer.m_iTransport, g_configSipServer.m_iSipPort, g_configSipServer.m_strSipAddr.c_str(), g_configSipServer.m_iSipPort, LogCB, NULL, g_configSipServer.m_iGbMode);
    //oSipServer.StopSipServer();

    do 
    {
        sleep(3);
    } while (1);

    std::cout << "Hello World!\n";


    //eXosip_event_t    *je = (eXosip_event_t *)pMsg->MsgBody;
    //CString strBuf;
    //if (!je->response)
    //{
    //    return;
    //}
    //osip_message_t *ack = NULL;
    //TRACE("\nRequest Failure, receive code %d\n", je->response->status_code);
    //// Handle 4XX errors
    //switch (je->response->status_code)
    //{
    //case SIP_UNAUTHORIZED:


    //eXosip_default_action
}


