/******************************************************************
* Author     : lizhigang (li.zhigang@intellif.com)
* CreateTime : 2019/7/9
* Copyright (c) 2019 Shenzhen Intellifusion Technologies Co., Ltd.
* File Desc  : sip server的配置管理
*******************************************************************/

#ifndef __CONFIG_SIP_SERVER_H__
#define __CONFIG_SIP_SERVER_H__

#include <stdlib.h>
#include <string>
#include "CommDefine.h"
//#include "LogManager.h"
#include "json/json.h"
#include "SipUA.h"

/*SIP Server的工作模式: 作为国标上级平台*/
#define GB28181_MODE_SIP_SERVER		0

/*SIP Server的工作模式: 作为国标下级平台*/
#define GB28181_MODE_SIP_CLIENT		1


class CConfigSipServer
{
public:
	CConfigSipServer();
	~CConfigSipServer();

	Comm_Bool readCfg(std::string strCfgFile);

	Comm_Bool writeCfg();

	std::string m_strCfgFile;

	/*本地SIP配置参数*/
	int m_iTransport = SIPUA_TANSPORT_UDP;
	std::string m_strSipAddr;
	int m_iSipPort = 0;
	std::string m_strLocalSipId;//"34020100002000000001";
    std::string m_strSipRegion;
    std::string m_strSipDomain;
	std::string m_strPwd; //"123456"; //
	int m_iExpires;
	int m_iMonitorKeepalive; //监视下级平台的keepalive超时时间
    int m_iGbMode;

	/*远端SIP服务器配置参数*/
	std::string m_strServerId;
    std::string m_strServerRegion;
	std::string m_strServerIP; //"192.168.2.5"; //
	int m_iServerPort;

	/*http api配置参数*/
	int m_iHttpApiPort;
	std::string m_strHttpApiRoot;

	//sip server的工作模式
	int m_iWorkMode = GB28181_MODE_SIP_SERVER;

	//log等级
	std::string m_strLogLevel;

	//中心服务器配置
	int m_iEnableCenterServer; //是否启用与中心服务器间交互
	std::string m_strCenterServerIP; //中心服务器的thrift地址
	int m_iCenterServerPort;
	int m_iSipServerId;
	int m_iRegisterCenterTime; //向中心服务器定时注册的时间

};


#endif

