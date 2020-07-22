#ifndef __CALL_MANAGER_H__
#define __CALL_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SipUA.h"
#include "json/json.h"
#include <list>
#include <string>
#include <pthread.h>
#include "ThreadTimer.h"
#include "SipEndpointInfo.h"


//终端类型: device cloud
#define SIP_ENDPOINT_DEVICECLOUD		"device_cloud_media"
//第三方国标平台
#define SIP_ENDPOINT_GB_PLATFORM		"gb28181_platform"


/*呼叫信息*/
typedef struct tagSCallInfo
{
	/*基于exosip的tid, did, cid*/
	int tid; 
	int did;
	int cid;
	//sdp视频信息
	SMediaInfo sVideoMediaInfo;
	//sdp音频信息
	SMediaInfo sAudioMediaInfo;
	//sip终端的sipid
	char szSipEndpointId[SIPUA_STRING_LINE_LEN];
}SCallInfo;

/*gb28181只需要支持单呼，不需要组呼*/
typedef struct tagSCallContext
{
	SCallInfo sCaller; /*主叫*/
	SCallInfo sCallee; /*被叫*/
}SCallContext;

/*主叫*/
#define CALL_LEG_TYPE_CALLER    0

/*被叫*/
#define CALL_LEG_TYPE_CALLEE    1

/*录像回放时，暂存主叫INVITE信息
录像回放请求发起前，要先用MESSAGE消息查询录像文件列表，所以暂存主叫的INVITE请求信息，等MESSAGE消息回复后，
sip server再把主叫的INVITE请求转到被叫
*/
typedef struct tagSPlaybackCallerInfo
{
	/*基于exosip的tid, did, cid*/
	int tid; 
	int did;
	int cid;
	//主叫INVITE信息
	SSdpInfo sdpInfo; //sdp
	SSubjectInfo subject; //subject
	SFromHeader sHdrFrom; //from头域
	//主叫发起INVITE时间，长时间没有收到message的回复，需要释放主叫INVITE信息
	long long i64InviteTime;
}SPlaybackCallerInfo;


class CCallManager
{
public:
    CCallManager();
    ~CCallManager();

	//初始化呼叫管理
	int initCallManager();
	//释放呼叫管理
	int releaseCallManager();


	/****************************************************************
	 管理sip客户端的呼叫业务
	 ****************************************************************/

    /*开始一个新的呼叫
        @iCallerCid: 主叫cid
        @iCalleeCid: 被叫cid
        */
    SCallContext *newCallContext(int iCallerCid, int iCalleeCid);

    /*释放呼叫*/
    int removeCallContext(SCallContext *pCtx);

	/*删除所有的呼叫上下文*/
	int removeAllCallContext();

    /*基于exosip更新主叫呼叫腿信息
        @cid: 呼叫腿的cid
        @tid, did: 更新其tid, did
        */
    int updateCallerCallLeg(int cid, int tid, int did);

    /*基于exosip更新被叫呼叫腿信息
        @cid: 呼叫腿的cid
        @tid, did: 更新其tid, did
        */
    int updateCalleeCallLeg(int cid, int tid, int did);

    /*根据呼叫腿类型查找call context*/
	//返回的SCallContext会被外部使用，所以在使用此返回值时应该加锁保证安全
    SCallContext *getCallContextByCallLeg(int cid, int type);

    /*根据cid查找call context*/
	//返回的SCallContext会被外部使用，所以在使用此返回值时应该加锁保证安全
    SCallContext *getCallContext(int cid);

	//统计当前在线的呼叫信息
    int getCallStatus(Json::Value &jsCallStatus);

	//当sip客户端离线或者重新注册，sip server需要把与此sip客户端相关的呼叫都释放
	//@strSipId: 离线客户端的sipid
	int removeCallContextBySipId(std::string strSipId);

	//锁住呼叫上下文列表
	void lockCallContext();
	void unlockCallContext();

	/*主叫发起INVITE，要请求录像回放，暂存主叫INVITE信息
		@strDevId: 被叫的国标编码
		@tid, did, cid: 主叫的SipUA相关资源
		@sdpinfo, subject, sHdrFrom: ...
	*/
	int insertPlaybackCallerInfo(std::string strDevId, int tid, int did, int cid, 
		SSdpInfo &sdpInfo, SSubjectInfo &subject, SFromHeader &sHdrFrom);

	/*删除录像回放的主叫INVITE信息
		@strDevId: 被叫的国标编码
	*/
	int removePlaybackCallerInfo(std::string strDevId);

	/*获取主叫INVITE信息
		@strDevId: 被叫的国标编码
		@sCallerInfo: 返回的INVITE信息
		返回值：0成功，其他失败
	*/
	int getPlaybackCallerInfo(std::string strDevId, SPlaybackCallerInfo &sCallerInfo);
	
private:
    std::list<SCallContext*> m_lsCallContext;
	pthread_mutex_t m_tLockCallContext;
	
    std::map<std::string, SPlaybackCallerInfo*> m_mapPlaybackCallerInfo; //录像回放建立时的主叫
    pthread_mutex_t m_tLockPlaybackCaller;


	/****************************************************************
	 管理sip客户端的注册，维持sip注册心跳
	****************************************************************/

public:
	/*新的SIP客户端注册到服务器，保存其注册信息
	返回值表示是否需要更新设备目录，1需要，0不需要
	*/
	int insertEndpoint(SRegisterInfo *pRegInfo, std::string strType);
	//移除sip客户端
	int removeEndpoint(std::string strSipId);

	//统计注册客户端情况
	void statisticsEndpoint(Json::Value &jsVal);

	//通过平台的国标编码，返回平台的注册信息
	//返回的SSipEndpointInfo会被外部使用，所以在使用此返回值时应该加锁保证安全
	CSipEndpointInfo* getSipEndpointInfo(std::string strSipId);

	//通过设备ID，返回设备所在的第三方平台信息
	//返回的SSipEndpointInfo会被外部使用，所以在使用此返回值时应该加锁保证安全
	CSipEndpointInfo* queryGb28181PlatformInfo(std::string strDevId);

	//更新sip客户端的Keepalive心跳时间
	int updateSipEndpointKeepalive(std::string strSipId);

	//定时器，检测sip客户端心跳，如果长时间未有心跳，则删除此sip客户端
	static void monitorSipEndpointTimer(HTIMER hTimer, void *pUserData);
	void doMonitorSipEndpoint();

	/*检查sip客户端的心跳时间，超时删除sip客户端*/
	void checkSipEndpointHeartbeat(long long i64CurTime);

	/*检查录像回放的主叫INVITE*/
	void checkPlaybackCallerInfo(long long i64CurTime);

	//为保证线程安全，锁住m_mapSipEndpoints
	void lockSipEndpoint();
	void unlockSipEndpoint();

	

private:
	std::map<std::string, CSipEndpointInfo*> m_mapSipEndpoints; //所有注册上来的SIP客户端
	pthread_mutex_t m_tLockSipEndpoint;
	
	HTIMER m_hCheckSipEndpointTimer; //检测sip客户端状态的定时器    
};






#endif


