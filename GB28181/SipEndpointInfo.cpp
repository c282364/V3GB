#include "SipEndpointInfo.h"
//#include "LogManager.h"
#include "CommDefine.h"

CSipEndpointInfo::CSipEndpointInfo()
{

}


CSipEndpointInfo::~CSipEndpointInfo()
{
	removeAllSipDevice();
}

/*解析json数组，添加设备信息，sip设备信息都保存于json数组*/
int CSipEndpointInfo::appendSipDevice(const Json::Value &jsonDevice)
{
	m_jsonCatalog.append(jsonDevice);

	if (jsonDevice.isMember("DeviceID"))
	{
		std::string strId = jsonDevice["DeviceID"].asString();
		if (false == isExistSipDevice(strId))
		{
			CSipDeviceInfo* pDevInfo = new CSipDeviceInfo;
			pDevInfo->m_strDevId = strId;
			std::string strName;
			GET_JSON_CFG_STRING(jsonDevice, "Name", strName);
			pDevInfo->m_strDevName = strName;
			m_mapSipDeviceInfo.insert(std::pair<std::string, CSipDeviceInfo*>(strId, pDevInfo));
			printf("sip endpoint[%s] insert dev, name[%s], id[%s].\n", sRegInfo.szRegUser, pDevInfo->m_strDevName.c_str(),
				pDevInfo->m_strDevId.c_str());
		}
		else
		{
            printf("sip endpoint[%s] exists dev id[%s].\n", sRegInfo.szRegUser, strId.c_str());
		}
	}
	else
	{
		std::string strJson = jsonDevice.toStyledString();
        printf("sip endpoint[%s] append sipdev failed, %s\n", strJson.c_str());
	}
	
	return 0;
}

/*移除所有的sip设备*/
int CSipEndpointInfo::removeAllSipDevice()
{
	//删除json数据
	m_jsonCatalog.clear();	

	//删除sip设备
	std::map<std::string, CSipDeviceInfo*>::iterator iter = m_mapSipDeviceInfo.begin();
	while (iter != m_mapSipDeviceInfo.end())
	{
		CSipDeviceInfo *pSipDev = iter->second;
        printf("sip endpoint[%s] remove dev, name[%s], id[%s].\n", sRegInfo.szRegUser, pSipDev->m_strDevName.c_str(),
			pSipDev->m_strDevId.c_str());
		/*释放内存*/
		delete pSipDev;		
		m_mapSipDeviceInfo.erase(iter++);		
	}
	
	return 0;
}

/*是否存在SIP设备*/
bool CSipEndpointInfo::isExistSipDevice(std::string strSipDevId)
{
	std::map<std::string, CSipDeviceInfo*>::iterator iter;
	iter = m_mapSipDeviceInfo.find(strSipDevId);
	if (iter == m_mapSipDeviceInfo.end())
	{
		return false;
	}
	
	return true;
}

/*统计sip终端下的设备信息*/
void CSipEndpointInfo::statisticSipDevice(Json::Value &jsonSipEndpoint)
{
	Json::Value jsonDevInfo;
	int iDevCount = 0;
	std::map<std::string, CSipDeviceInfo*>::iterator iter = m_mapSipDeviceInfo.begin();
	while (iter != m_mapSipDeviceInfo.end())
	{
		Json::Value jsonDev;
		CSipDeviceInfo* pDevInfo = iter->second;
		jsonDev["id"] = pDevInfo->m_strDevId;
		jsonDev["name"] = pDevInfo->m_strDevName;
		jsonDevInfo.append(jsonDev);
		iDevCount++;
		iter++;
	}

	jsonSipEndpoint["deviceCount"] = iDevCount;
	jsonSipEndpoint["deviceInfo"] = jsonDevInfo;
}

/*设备目录数量*/
int CSipEndpointInfo::getCatalogSize()
{
	int ret = (int)m_jsonCatalog.size();
	return ret;
}


