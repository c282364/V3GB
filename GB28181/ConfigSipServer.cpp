#include "ConfigSipServer.h"

CConfigSipServer::CConfigSipServer()
{
}
CConfigSipServer::~CConfigSipServer()
{
}


/*读配置*/
Comm_Bool CConfigSipServer::readCfg(std::string strCfgFile)
{
	m_strCfgFile = strCfgFile;
	
	FILE *flCfg = fopen(strCfgFile.c_str(), "rb");
    if(NULL == flCfg)
    {
        printf("cannot open cfg file %s\n", strCfgFile.c_str());
        return -1;
    }

    int iLen = Comm_FileSize(flCfg);
    char szBuff[4096] = {0};
    fread(szBuff, 1, iLen, flCfg);
    fclose(flCfg);

    Json::Reader jsonReader;
    std::string strCfg = szBuff;
    Json::Value jsVal;
	if (jsonReader.parse(strCfg, jsVal) == false)
    {
        printf("cfg data error! file %s\n", strCfgFile.c_str());
        return Comm_False;
    }

	/*http api配置*/
	if (jsVal.isMember("httpApi"))
	{
		GET_JSON_CFG_INT(jsVal["httpApi"], "httpPort", m_iHttpApiPort, 8799);
		GET_JSON_CFG_STRING(jsVal["httpApi"], "httpRoot", m_strHttpApiRoot);
	}

	//work mode
	GET_JSON_CFG_INT(jsVal, "sipMode", m_iWorkMode, GB28181_MODE_SIP_SERVER);

	//日志参数
	if (jsVal.isMember("logManager"))
	{
		GET_JSON_CFG_STRING(jsVal["logManager"], "logLevel", m_strLogLevel);
	}

	//local sip param
	if (jsVal.isMember("localSipParam"))
	{
		GET_JSON_CFG_INT(jsVal["localSipParam"], "transport", m_iTransport, SIPUA_TANSPORT_UDP);
		GET_JSON_CFG_STRING(jsVal["localSipParam"], "sipAddr", m_strSipAddr);
		GET_JSON_CFG_INT(jsVal["localSipParam"], "sipPort", m_iSipPort, 5070);
		GET_JSON_CFG_STRING(jsVal["localSipParam"], "sipId", m_strLocalSipId);
		GET_JSON_CFG_STRING(jsVal["localSipParam"], "sipPassword", m_strPwd);
		GET_JSON_CFG_INT(jsVal["localSipParam"], "expires", m_iExpires, 3600);
		GET_JSON_CFG_INT(jsVal["localSipParam"], "monitorKeepalive", m_iMonitorKeepalive, 120);
	}	

	//remote sip param
	if (jsVal.isMember("remoteSipParam"))
	{
		GET_JSON_CFG_STRING(jsVal["remoteSipParam"], "serverId", m_strServerId);
		GET_JSON_CFG_STRING(jsVal["remoteSipParam"], "serverIP", m_strServerIP);
		GET_JSON_CFG_INT(jsVal["remoteSipParam"], "serverPort", m_iServerPort, 5060);
	}

	//中心服务器
	if (jsVal.isMember("centerServer"))
	{
		GET_JSON_CFG_INT(jsVal["centerServer"], "enable", m_iEnableCenterServer, 1);
		GET_JSON_CFG_STRING(jsVal["centerServer"], "centerServerIP", m_strCenterServerIP);
		GET_JSON_CFG_INT(jsVal["centerServer"], "centerServerPort", m_iCenterServerPort, 10010);
		GET_JSON_CFG_INT(jsVal["centerServer"], "sipServerId", m_iSipServerId, 10100);
		GET_JSON_CFG_INT(jsVal["centerServer"], "registerTime", m_iRegisterCenterTime, 30);
	}
	
	return Comm_True;
}

/*写配置*/
Comm_Bool CConfigSipServer::writeCfg()
{
	Json::Value jsVal;	

	jsVal["httpApi"]["httpPort"] = m_iHttpApiPort;
	jsVal["httpApi"]["httpRoot"] = m_strHttpApiRoot;

	//work mode
	jsVal["sipMode"] = m_iWorkMode;

	//日志参数
	jsVal["logManager"]["logLevel"] = m_strLogLevel;

	//local sip param
	jsVal["localSipParam"]["transport"] = m_iTransport;
	jsVal["localSipParam"]["sipAddr"] = m_strSipAddr;
	jsVal["localSipParam"]["sipPort"] = m_iSipPort;
	jsVal["localSipParam"]["sipId"] = m_strLocalSipId;
	jsVal["localSipParam"]["sipPassword"] = m_strPwd;
	jsVal["localSipParam"]["expires"] = m_iExpires;
	jsVal["localSipParam"]["monitorKeepalive"] = m_iMonitorKeepalive;

	//remote sip param
	jsVal["remoteSipParam"]["serverId"] = m_strServerId;
	jsVal["remoteSipParam"]["serverIP"] = m_strServerIP;
	jsVal["remoteSipParam"]["serverPort"] = m_iServerPort;

	//中心服务器
	jsVal["centerServer"]["enable"] = m_iEnableCenterServer;
	jsVal["centerServer"]["centerServerIP"] = m_strCenterServerIP;
	jsVal["centerServer"]["centerServerPort"] = m_iCenterServerPort;
	jsVal["centerServer"]["sipServerId"] = m_iSipServerId;
	jsVal["centerServer"]["registerTime"] = m_iRegisterCenterTime;
		
	FILE *flCfg = fopen(m_strCfgFile.c_str(), "wb");
    if(NULL == flCfg)
    {
        printf("cannot open cfg file %s\n", m_strCfgFile.c_str());
        return Comm_False;
    }
	
    std::string strCfg = jsVal.toStyledString();    
    fwrite(strCfg.c_str(), 1, strCfg.length(), flCfg);
    fclose(flCfg); 

	return Comm_True;
}



