#ifndef __GB28181_MEDIA_GATE_H__
#define __GB28181_MEDIA_GATE_H__

#include "SipUA.h"

/*媒体网关消息*/
typedef enum tagEMeidaGateMsg
{
    MEDIA_GATE_MSG_UNKNOWN = 0,
    MEDIA_GATE_MSG_CALL_INIT, /*呼叫发起消息*/
    MEDIA_GATE_MSG_CALL_RELEASE, /*呼叫释放消息*/
}EMeidaGateMsg;

/*handle to media gate*/
typedef void* HMG;

/*媒体网关的返回值*/
#define MEDIA_GATE_RETURN_SUCCESS       0
#define MEDIA_GATE_RETURN_FAILURE       1


/*媒体网关只要关心呼叫类消息
@eMsg: 消息类型
@pszDevAddr: 设备地址，由接入层使用此地址取流
@hPusher: 推流句柄。MediaGate内部会初始化发送PS流的端口，接入层的流使用此句柄发送到远方
返回值: MediaGate会根据返回值回复相应的信令，成功回复200 OK，失败回复400
*/
typedef int (*fnMediaGateEvent)(EMeidaGateMsg eMsg, const char *pszStreamId, const char *pszDevAddr, HMG hPusher, void *pParamCB);

/*媒体网关拉第三方国标平台流*/
typedef void (*fnMediaGateStreamCB)(void *pPuller, int iStreamType, 
        unsigned char *pBuffer, int iBufSize, void *param);


/*初始化国标媒体网关
@transport: UDP/TCP模式传输SIP信令
@pszLocalSipId: 媒体网关的SIP编码
@pszSignalAddr, iSignalPort: 信令面地址
@pszMediaAddr, iMediaStartPort: 媒体面的数据地址
@pszSipServerId, pszSipServerIP, iSipServerPort: sip server的id, sip信令地址
@fnCB, pParam: 事件回调函数
@iProxyEnable, 是否使用代理
@pszProxyIP, iProxySignalPort, iProxyStreamPort: 代理的IP，信令端口与媒体端口
*/
int GB28181MediaGate_Init(int transport, const char *pszLocalSipId,
    const char * pszSignalAddr, int iSignalPort, 
    const char * pszMediaAddr, int iMediaStartPort, 
    const char *pszSipServerId, const char * pszSipServerIP, int iSipServerPort,
    fnMediaGateEvent fnCB, void *pParam,
    fnSipUA_OutputLogCB fnLogCB, void *pParamLogCB,
    int iProxyEnable, const char *pszProxyIP, int iProxySignalPort, int iProxyStreamPort);


int GB28181MediaGate_Release();


/*向远端国标平台推送视频流*/
int GB28181MediaGate_PushFrame(HMG hPusher, unsigned char *pFrameData, int iFrameLen, unsigned long long i64TimeStamp);


/*创建拉流句柄，从远端国标平台获取视频
@pszStreamId: 视频流ID
@pszDestId: 请求的设备ID
@pszDestIP, iDestPort: sip server的地址
@iPlaybackFlag: 历史视频标记，0实时视频，1历史视频
@i64StartTime， i64StopTime：开始，结束时间，如果iPlaybackFlag为实时视频，这两参数忽略
@fnStreamCB, pParam: 视频流的回调接口
*/
HMG GB28181MediaGate_CreateStreamPuller(const char *pszStreamId, 
	const char * pszDestId,const char * pszDestIP, int iDestPort,
	int iPlaybackFlag, long long i64StartTime, long long i64StopTime,
	fnMediaGateStreamCB fnStreamCB, void *pParam);

/*删除拉流句柄*/
int GB28181MediaGate_DeleteStreamPuller(HMG hPuller);



#endif

