#ifndef __SIP_ENDPOINT_INFO_H__
#define __SIP_ENDPOINT_INFO_H__

#include "SipUA.h"
#include "json/json.h"
#include <map>
#include <string>

/*SIP设备信息*/
class CSipDeviceInfo
{
public:
	CSipDeviceInfo(){}
	~CSipDeviceInfo(){}

	std::string m_strDevName;
	std::string m_strDevId;
};

//sip终端的注册信息
class CSipEndpointInfo
{
public:
	CSipEndpointInfo();
	~CSipEndpointInfo();

	/*添加一个设备信息，sip设备信息都保存于json数组*/
	int appendSipDevice(const Json::Value &jsonDevice);

	/*移除所有的sip设备*/
	int removeAllSipDevice();

	/*是否存在SIP设备*/
	bool isExistSipDevice(std::string strSipDevId);

	/*统计sip终端下的设备信息*/
	void statisticSipDevice(Json::Value &jsonSipEndpoint);

	/*设备目录数量*/
	int getCatalogSize();

public:
	SRegisterInfo sRegInfo; //SIP客户端的注册信息
	std::string strType; //sip客户端类型
	long long i64RegisterTime; //上次注册时间
	long long i64KeepaliveTime; //收到Keepalive的时间


	//保存catalog的json
	/*如果下级平台的摄像机数量过多，摄像机列表会分多条SIP MESSAGE上报catalog，
	当完全接收完成，再把摄像机列表上报到中心服务器
	*/
	Json::Value m_jsonCatalog;
	
private:
	std::map<std::string, CSipDeviceInfo*> m_mapSipDeviceInfo; //sip终端下的所有设备
};



#endif






