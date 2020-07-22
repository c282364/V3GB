#include "CallManager.h"
//#include "LogManager.h"
#include "CommDefine.h"
#include "ConfigSipServer.h"

//本地配置
extern CConfigSipServer g_configSipServer;


CCallManager::CCallManager()
{
	pthread_mutex_init(&m_tLockSipEndpoint, NULL);
	pthread_mutex_init(&m_tLockCallContext, NULL);	
	pthread_mutex_init(&m_tLockPlaybackCaller, NULL);		
}
CCallManager::~CCallManager()
{
	releaseCallManager();
	
	pthread_mutex_destroy(&m_tLockSipEndpoint);
	pthread_mutex_destroy(&m_tLockCallContext);	
	pthread_mutex_destroy(&m_tLockPlaybackCaller);		
}

//初始化呼叫管理
int CCallManager::initCallManager()
{
	m_hCheckSipEndpointTimer = Comm_CreateThreadTimer(10*1000, monitorSipEndpointTimer, this);
	return 0;
}
//释放呼叫管理
int CCallManager::releaseCallManager()
{
	if (m_hCheckSipEndpointTimer)
	{
		Comm_ReleaseThreadTimer(m_hCheckSipEndpointTimer);
		m_hCheckSipEndpointTimer = NULL;
	}
	
	removeAllCallContext();

	return 0;
}


/****************************************************************
 管理sip客户端的呼叫业务
 ****************************************************************/

/*开始一个新的呼叫
        @iCallerCid: 主叫cid
        @iCalleeCid: 被叫cid
        */
SCallContext *CCallManager::newCallContext(int iCallerCid, int iCalleeCid)
{
	pthread_mutex_lock(&m_tLockCallContext);
	SCallContext *pCallCtx = new SCallContext;
	memset(pCallCtx, 0, sizeof(SCallContext));
	pCallCtx->sCaller.cid = iCallerCid;
	pCallCtx->sCallee.cid = iCalleeCid;
	m_lsCallContext.push_back(pCallCtx);
	pthread_mutex_unlock(&m_tLockCallContext);
	
	return pCallCtx;
}

/*释放呼叫*/
int CCallManager::removeCallContext(SCallContext *pCtx)	
{
	pthread_mutex_lock(&m_tLockCallContext);
	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCallCtx = (*iter);
		if (pCallCtx == pCtx)
		{
            printf("remove call context, caller[%s@%s:%d], callee[%s@%s:%d]\n",
				pCtx->sCaller.szSipEndpointId, pCtx->sCaller.sVideoMediaInfo.szAddr, pCtx->sCaller.sVideoMediaInfo.port,
				pCtx->sCallee.szSipEndpointId, pCtx->sCallee.sVideoMediaInfo.szAddr, pCtx->sCallee.sVideoMediaInfo.port);
			
			delete pCallCtx;
			iter = m_lsCallContext.erase(iter);
			break;
		}
		iter++;
	}	
	pthread_mutex_unlock(&m_tLockCallContext);
	return 0;
}

/*删除所有的呼叫上下文*/
int CCallManager::removeAllCallContext()
{
	pthread_mutex_lock(&m_tLockCallContext);
	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCallCtx = (*iter);
		delete pCallCtx;
		iter = m_lsCallContext.erase(iter);
		if (iter == m_lsCallContext.end())
			break;
	}
	pthread_mutex_unlock(&m_tLockCallContext);
	return 0;
}



/*基于exosip更新主叫呼叫腿信息
        @cid: 呼叫腿的cid
        @tid, did: 更新其tid, did
*/
int CCallManager::updateCallerCallLeg(int cid, int tid, int did)
{
	pthread_mutex_lock(&m_tLockCallContext);
	SCallContext *pCtx = getCallContextByCallLeg(cid, CALL_LEG_TYPE_CALLER);
	if (pCtx)
	{
		pCtx->sCaller.tid = tid;
		pCtx->sCaller.did = did;
	}
	pthread_mutex_unlock(&m_tLockCallContext);
	
	return 0;
}

/*基于exosip更新被叫呼叫腿信息
        @cid: 呼叫腿的cid
        @tid, did: 更新其tid, did
*/
int CCallManager::updateCalleeCallLeg(int cid, int tid, int did)
{
	pthread_mutex_lock(&m_tLockCallContext);
	
	SCallContext *pCtx = getCallContextByCallLeg(cid, CALL_LEG_TYPE_CALLEE);
	if (pCtx)
	{
		pCtx->sCallee.tid = tid;
		pCtx->sCallee.did = did;
	}

	pthread_mutex_unlock(&m_tLockCallContext);
	return 0;
}

/*查找call context*/
SCallContext *CCallManager::getCallContextByCallLeg(int cid, int type)
{
	SCallContext *pRetCtx = NULL;

	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCtx = (*iter);
		if (CALL_LEG_TYPE_CALLER == type) 
		{
			if (cid == pCtx->sCaller.cid)
			{
				pRetCtx = pCtx;
				break;
			}
		}
		else if (CALL_LEG_TYPE_CALLEE == type)
		{
			if (cid == pCtx->sCallee.cid)
			{
				pRetCtx = pCtx;
				break;
			}
		}
		
		iter++;
	}

	return pRetCtx;
}

/*根据cid查找call context*/
SCallContext *CCallManager::getCallContext(int cid)
{
	SCallContext *pRetCtx = NULL;	
	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCtx = (*iter);
		if (cid == pCtx->sCaller.cid || cid == pCtx->sCallee.cid)
		{
			pRetCtx = pCtx;
			break;
		}
		
		iter++;
	}

	return pRetCtx;
}

//统计当前在线的呼叫信息
int CCallManager::getCallStatus(Json::Value &jsCallStatus)
{
	pthread_mutex_lock(&m_tLockCallContext);
	
	int ret = 0;
	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCtx = (*iter);
		Json::Value jsCtx;

		//带audio的呼叫信息
		#if 0
		//主叫信息
		char szCallerInfo[1024] = {0}; 
		sprintf(szCallerInfo, "video[%s:%d]; audio[%s:%d]", pCtx->sCaller.sVideoMediaInfo.szAddr, pCtx->sCaller.sVideoMediaInfo.port,
			pCtx->sCaller.sAudioMediaInfo.szAddr, pCtx->sCaller.sAudioMediaInfo.port);		
		jsCtx["caller"] = szCallerInfo;
		//被叫信息
		char szCalleeInfo[1024] = {0};
		sprintf(szCalleeInfo, "video[%s:%d]; audio[%s:%d]", pCtx->sCallee.sVideoMediaInfo.szAddr, pCtx->sCallee.sVideoMediaInfo.port,
			pCtx->sCallee.sAudioMediaInfo.szAddr, pCtx->sCallee.sAudioMediaInfo.port);
		jsCtx["callee"] = szCalleeInfo;
		#endif 

		//主叫信息
		char szCallerInfo[1024] = {0}; 
		sprintf(szCallerInfo, "%s@%s:%d", pCtx->sCaller.szSipEndpointId,
			pCtx->sCaller.sVideoMediaInfo.szAddr, pCtx->sCaller.sVideoMediaInfo.port);		
		jsCtx["caller"] = szCallerInfo;
		//被叫信息
		char szCalleeInfo[1024] = {0};
		sprintf(szCalleeInfo, "%s@%s:%d", pCtx->sCallee.szSipEndpointId,
			pCtx->sCallee.sVideoMediaInfo.szAddr, pCtx->sCallee.sVideoMediaInfo.port);
		jsCtx["callee"] = szCalleeInfo;
		
		jsCallStatus.append(jsCtx);
		iter++;
		ret++;
	}

	pthread_mutex_unlock(&m_tLockCallContext);
	
	return ret;
}

//当sip客户端离线或者重新注册，sip server需要把与此sip客户端相关的呼叫都释放
//@strSipId: 离线客户端的sipid
int CCallManager::removeCallContextBySipId(std::string strSipId)
{
	//ENTER_FUNC();
	
	pthread_mutex_lock(&m_tLockCallContext);

	std::list<SCallContext *>::iterator iter = m_lsCallContext.begin();
	while (iter != m_lsCallContext.end())
	{
		SCallContext *pCtx = (*iter);
		//主叫或者被叫的sipid与要删除的sipid一致，删除其所有正在进行的呼叫
		std::string strCallerSipId = pCtx->sCaller.szSipEndpointId;
		std::string strCalleeSipId = pCtx->sCallee.szSipEndpointId;
		if (strCallerSipId == strSipId || strCalleeSipId == strSipId)
		{
            printf("remove call context, caller[%s@%s:%d], callee[%s@%s:%d]\n",
				pCtx->sCaller.szSipEndpointId, pCtx->sCaller.sVideoMediaInfo.szAddr, pCtx->sCaller.sVideoMediaInfo.port,
				pCtx->sCallee.szSipEndpointId, pCtx->sCallee.sVideoMediaInfo.szAddr, pCtx->sCallee.sVideoMediaInfo.port);
			//分别bye主叫与被叫
			SipUA_TerminateCall(pCtx->sCaller.cid, pCtx->sCaller.did);
			SipUA_TerminateCall(pCtx->sCallee.cid, pCtx->sCallee.did);
				
			delete pCtx;
			iter = m_lsCallContext.erase(iter);
			
		}
		else
		{
			iter++;
		}
	}	

	pthread_mutex_unlock(&m_tLockCallContext);

	//EXIT_FUNC();
	return 0;
}

//锁住呼叫上下文列表
void CCallManager::lockCallContext()
{
	pthread_mutex_lock(&m_tLockCallContext);
}
void CCallManager::unlockCallContext()
{
	pthread_mutex_unlock(&m_tLockCallContext);
}



/****************************************************************
 管理sip客户端的注册，维持sip注册心跳
****************************************************************/

/*新的SIP客户端注册到服务器，保存其注册信息
返回值表示是否需要更新设备目录，1需要，0不需要
*/
int CCallManager::insertEndpoint(SRegisterInfo *pRegInfo, std::string strType)
{
	int ret = 0;
	long long i64CurTime = Comm_GetMilliSecFrom1970();
	std::string strSipId = pRegInfo->szRegUser;
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	iter = m_mapSipEndpoints.find(strSipId);
	if (iter != m_mapSipEndpoints.end()) //sip客户端已经存在
	{
		printf("sip endpoint %s existed.\n", strSipId.c_str());
		CSipEndpointInfo *pExistEndpoint = iter->second; //已经存在的sip endpoint		
		std::string strInCallId = pRegInfo->szCallId;
		std::string strExistCallId = pExistEndpoint->sRegInfo.szCallId;
		if (strInCallId == strExistCallId) //两次注册的callid一样，刷新注册
		{
            printf("sip endpoint[%s] refresh register, callid=%s\n", strSipId.c_str(), strInCallId.c_str());
			ret = 0; //刷新注册，不更新设备目录
		}
		else //sipid存在的终端，但前后两次的callid不一致，可能sip客户端重启后重新注册，要把其存在的呼叫资源释放
		{
            printf("sip endpoint[%s] register new, close all call context, new_callid=%s, old_callid=%s\n",
				strSipId.c_str(), strInCallId.c_str(), strExistCallId.c_str());	
			//结束sip客户端下的所有呼叫
			removeCallContextBySipId(strSipId);
			//释放sip客户端下的设备信息
			pExistEndpoint->removeAllSipDevice();
			ret = 1; //更新设备目录
		}

		//刷新注册时间
		memcpy(&pExistEndpoint->sRegInfo, pRegInfo, sizeof(SRegisterInfo));	
		pExistEndpoint->strType = strType;
		pExistEndpoint->i64KeepaliveTime = i64CurTime;
		pExistEndpoint->i64RegisterTime = i64CurTime;		
		return ret;
	}

	//新插入sip客户端
	CSipEndpointInfo *pEndpointInfo = new CSipEndpointInfo;
	memcpy(&pEndpointInfo->sRegInfo, pRegInfo, sizeof(SRegisterInfo));	
	pEndpointInfo->strType = strType;
	pEndpointInfo->i64KeepaliveTime = i64CurTime;
	pEndpointInfo->i64RegisterTime = i64CurTime;
	m_mapSipEndpoints.insert(std::pair<std::string, CSipEndpointInfo*>(strSipId, pEndpointInfo));
    printf("insert sip endpoint[%s]\n", strSipId.c_str());
	ret = 1; //更新设备目录
	return ret;
}

//移除sip客户端
int CCallManager::removeEndpoint(std::string strSipId)
{
	pthread_mutex_lock(&m_tLockSipEndpoint);
	
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	iter = m_mapSipEndpoints.find(strSipId);
	if (iter == m_mapSipEndpoints.end())
	{
		pthread_mutex_unlock(&m_tLockSipEndpoint);
		return -1;
	}
	
	CSipEndpointInfo *pEndpoint = iter->second;
	delete pEndpoint;
	m_mapSipEndpoints.erase(iter);	
	
	pthread_mutex_unlock(&m_tLockSipEndpoint);
	return 0;
}

//统计注册客户端情况
void CCallManager::statisticsEndpoint(Json::Value &jsVal)
{
	pthread_mutex_lock(&m_tLockSipEndpoint);
	
	Json::Value jsStatistcs;
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	for (iter = m_mapSipEndpoints.begin(); iter != m_mapSipEndpoints.end(); ++iter)
	{
		CSipEndpointInfo *pEndpointInfo = iter->second;
		Json::Value jsEndpoint;
		char szUrl[1024] = {0};
		sprintf(szUrl, "%s@%s:%d", pEndpointInfo->sRegInfo.szRegUser, pEndpointInfo->sRegInfo.szContactIP, pEndpointInfo->sRegInfo.iContactPort);
		jsEndpoint["url"] = szUrl;
		jsEndpoint["type"] = pEndpointInfo->strType;
		pEndpointInfo->statisticSipDevice(jsEndpoint);
		jsStatistcs.append(jsEndpoint);
	}

	jsVal["sipClient"] = jsStatistcs;

	pthread_mutex_unlock(&m_tLockSipEndpoint);
}

//通过平台的国标编码，返回平台的注册信息
//返回的SSipEndpointInfo会被外部使用，所以在使用此返回值时应该加锁保证安全
CSipEndpointInfo* CCallManager::getSipEndpointInfo(std::string strSipId)
{
	CSipEndpointInfo *pFind = NULL;
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	iter = m_mapSipEndpoints.find(strSipId);
	if (iter == m_mapSipEndpoints.end())
	{
        printf("cannot find sip endpoint, sipid=%s\n", strSipId.c_str());
		return NULL;
	}
	pFind = iter->second;	
	return pFind;
}

//通过设备ID，返回设备所在的第三方平台信息
//返回的SSipEndpointInfo会被外部使用，所以在使用此返回值时应该加锁保证安全
CSipEndpointInfo* CCallManager::queryGb28181PlatformInfo(std::string strDevId)
{
	CSipEndpointInfo *pFind = NULL;
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	for (iter = m_mapSipEndpoints.begin(); iter != m_mapSipEndpoints.end(); ++iter)
	{
		CSipEndpointInfo *pEndpointInfo = iter->second;
		/*设备存在第三方平台*/
		bool bDevExist = pEndpointInfo->isExistSipDevice(strDevId);
		if (true == bDevExist && pEndpointInfo->strType == SIP_ENDPOINT_GB_PLATFORM)
		{
			pFind = pEndpointInfo;
			break;
		}
	}
	
	return pFind;
}

//更新sip客户端的Keepalive心跳时间
int CCallManager::updateSipEndpointKeepalive(std::string strSipId)
{
	pthread_mutex_lock(&m_tLockSipEndpoint);
	
	std::map<std::string, CSipEndpointInfo*>::iterator iter;
	iter = m_mapSipEndpoints.find(strSipId);
	if (iter == m_mapSipEndpoints.end())
	{
        printf("cannot find sip endpoint, sipid=%s\n", strSipId.c_str());
		pthread_mutex_unlock(&m_tLockSipEndpoint);
		return -1;
	}
	
	CSipEndpointInfo *pEndpoint = iter->second;
	pEndpoint->i64KeepaliveTime = Comm_GetMilliSecFrom1970();

	pthread_mutex_unlock(&m_tLockSipEndpoint);
	
	return 0;
}


//定时器，检测sip客户端心跳，如果长时间未有心跳，则删除此sip客户端
void CCallManager::monitorSipEndpointTimer(HTIMER hTimer, void *pUserData)
{
	CCallManager *pMgr = (CCallManager*)pUserData;
	pMgr->doMonitorSipEndpoint();
}
void CCallManager::doMonitorSipEndpoint()
{
	long long i64CurTime = Comm_GetMilliSecFrom1970();

	checkSipEndpointHeartbeat(i64CurTime);

	checkPlaybackCallerInfo(i64CurTime);	
}

/*检查sip客户端的心跳时间，超时删除sip客户端*/
void CCallManager::checkSipEndpointHeartbeat(long long i64CurTime)
{
	pthread_mutex_lock(&m_tLockSipEndpoint);
		
	std::map<std::string, CSipEndpointInfo*>::iterator iter = m_mapSipEndpoints.begin();
	while (iter != m_mapSipEndpoints.end())
	{
		bool bNeedDelEndpoint = false; //是否需要删除sip客户端标记		
		CSipEndpointInfo *pEndpointInfo = iter->second;
		//检查心跳
		long long i64KeepaliveGap = i64CurTime - pEndpointInfo->i64KeepaliveTime;
		if (i64KeepaliveGap >= (g_configSipServer.m_iMonitorKeepalive*1000)) //120秒没有收到sip客户端的keepalive消息
		{
			printf("sip endpoint[%s] Keepalive msg timeout, curtime=%lld, keepalivetime=%lld\n", iter->first.c_str(),
				i64CurTime, pEndpointInfo->i64KeepaliveTime);
			bNeedDelEndpoint = true;
		}

		//检查注册超时
		long long i64RegisterGap = i64CurTime - pEndpointInfo->i64RegisterTime;
		if (i64RegisterGap >= pEndpointInfo->sRegInfo.expires * 1000)
		{
            printf("sip endpoint[%s] register timeout, curtime=%lld, registertime=%lld, expires=%d\n", iter->first.c_str(),
				i64CurTime, pEndpointInfo->i64RegisterTime, pEndpointInfo->sRegInfo.expires);
			bNeedDelEndpoint = true;
		}

		if (bNeedDelEndpoint)
		{
			//结束sip客户端下的所有呼叫
            printf("erase sip endpoint[%s], call context will removed.\n", iter->first.c_str());
			removeCallContextBySipId(iter->first.c_str());
			delete pEndpointInfo;
			m_mapSipEndpoints.erase(iter++);
		}
		else
		{
			iter++;
		}
	}

	pthread_mutex_unlock(&m_tLockSipEndpoint);

}

/*检查录像回放的主叫INVITE*/
void CCallManager::checkPlaybackCallerInfo(long long i64CurTime)
{
	pthread_mutex_lock(&m_tLockPlaybackCaller);

	std::map<std::string, SPlaybackCallerInfo*>::iterator iter = m_mapPlaybackCallerInfo.begin();
	while (iter != m_mapPlaybackCallerInfo.end())
	{
		SPlaybackCallerInfo *pCallerInfo = iter->second;
		long long i64TimeGap = i64CurTime - pCallerInfo->i64InviteTime;
		if (i64TimeGap >= 30*1000) //30秒还未处理完录像回放，认为异常的录像回放请求，删除主叫INVITE信息
		{
			std::string strDevId = iter->first;
            printf("invite playback timeout, remove caller info, callee[%s]\n", strDevId.c_str());
			delete pCallerInfo;
			m_mapPlaybackCallerInfo.erase(iter++);
		}
		else
		{
			iter++;
		}
	}

	pthread_mutex_unlock(&m_tLockPlaybackCaller);
}



//为保证线程安全，锁住m_mapSipEndpoints
void CCallManager::lockSipEndpoint()
{
	pthread_mutex_lock(&m_tLockSipEndpoint);
}
void CCallManager::unlockSipEndpoint()
{
	pthread_mutex_unlock(&m_tLockSipEndpoint);
}

/*主叫发起INVITE，要请求录像回放，暂存主叫INVITE信息
@strDevId: 被呼叫的国标编码
@tid, did, cid: 主叫的SipUA相关资源
@sdpinfo, subject, sHdrFrom: ...
*/
int CCallManager::insertPlaybackCallerInfo(std::string strDevId, int tid, int did, int cid, 
		SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom)
{
	pthread_mutex_lock(&m_tLockPlaybackCaller);
	
	std::map<std::string, SPlaybackCallerInfo*>::iterator iter;
	iter = m_mapPlaybackCallerInfo.find(strDevId);
	if (iter == m_mapPlaybackCallerInfo.end()) 
	{
		SPlaybackCallerInfo *pCallerInfo = new SPlaybackCallerInfo;
		pCallerInfo->i64InviteTime = Comm_GetMilliSecFrom1970();
		pCallerInfo->cid = cid;
		pCallerInfo->tid = tid;
		pCallerInfo->did = did;
		memcpy(&pCallerInfo->sdpInfo, &sdpInfo, sizeof(sdpInfo));
		memcpy(&pCallerInfo->subject, &subject, sizeof(subject));
		memcpy(&pCallerInfo->sHdrFrom, &sHdrFrom, sizeof(sHdrFrom));
		m_mapPlaybackCallerInfo.insert(std::pair<std::string, SPlaybackCallerInfo*>(strDevId, pCallerInfo));
        printf("insert playback callee[%s]\n", strDevId.c_str());
	}

	pthread_mutex_unlock(&m_tLockPlaybackCaller);

	return 0;
}

/*删除录像回放的主叫INVITE信息
@strDevId: 被叫的国标编码
*/
int CCallManager::removePlaybackCallerInfo(std::string strDevId)
{
	pthread_mutex_lock(&m_tLockPlaybackCaller);
	
	std::map<std::string, SPlaybackCallerInfo*>::iterator iter;
	iter = m_mapPlaybackCallerInfo.find(strDevId);
	if (iter != m_mapPlaybackCallerInfo.end()) 
	{
        printf("remove playback callee[%s]\n", strDevId.c_str());
		SPlaybackCallerInfo *pCallerInfo = iter->second;
		delete pCallerInfo;
		m_mapPlaybackCallerInfo.erase(iter);
	}

	pthread_mutex_unlock(&m_tLockPlaybackCaller);
	
	return 0;
}

/*获取主叫INVITE信息
@strDevId: 被叫的国标编码
@sCallerInfo: 返回的INVITE信息
返回值：0成功，其他失败
*/
int CCallManager::getPlaybackCallerInfo(std::string strDevId, SPlaybackCallerInfo &sCallerInfo)
{
	memset(&sCallerInfo, 0, sizeof(sCallerInfo));
	int ret = -1;
	
	pthread_mutex_lock(&m_tLockPlaybackCaller);
	
	std::map<std::string, SPlaybackCallerInfo*>::iterator iter;
	iter = m_mapPlaybackCallerInfo.find(strDevId);
	if (iter != m_mapPlaybackCallerInfo.end()) 
	{
		SPlaybackCallerInfo *pCallerInfo = iter->second;
		memcpy(&sCallerInfo, pCallerInfo, sizeof(sCallerInfo));
		ret = 0;
	}
	
	pthread_mutex_unlock(&m_tLockPlaybackCaller);
	
	return ret;
}



