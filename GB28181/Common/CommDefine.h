#ifndef __COMM_DEFINE_H__
#define __COMM_DEFINE_H__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>


#ifndef U8
#define U8 unsigned char 
#endif

#ifndef U16
#define U16 unsigned short 
#endif

#ifndef U32
#define U32 unsigned long 
#endif

#ifndef S8
#define S8 char 
#endif

#ifndef S16
#define S16 short 
#endif

#ifndef S32
#define S32 int 
#endif

#ifndef Comm_Bool
#define Comm_Bool U8
#endif

#ifndef Comm_True
#define Comm_True   1
#endif

#ifndef Comm_False
#define Comm_False  0
#endif

/*向中心服务器发起注册时，device cloud media的type和typeValue*/
#define REG_CENTER_SERVER_TYPE_MEDIA			2
#define REG_CENTER_SERVER_TYPEVALUE_MEDIA		"streamMedia"

/*向中心服务器发起注册时，sip server的type和typeValue*/
#define REG_CENTER_SERVER_TYPE_SIPSERVER		4
#define REG_CENTER_SERVER_TYPEVALUE_SIPSERVER	"sipServer"


/*
引擎各组件表征返回状态的字段说明：
（1）字段说明
“respCode” -- 接口返回码，整型
“respMessage” -- 接口返回的结果的描述，字符串，一般是对返回码的描述
“respRemark” -- 接口返回错误/状态的原因，或错误解决办法的提示信息 字符串

（2）接口正常返回，统一返回：
“respCode”: 10000000
“respMessage”: "OK"
“respRemark”: ""

2、接口异常返回说明
（1） 返回码定义
十进制，8位
11xxxxxx
     -- 占用2位，11，固定不变，代表引擎相关组件
     -- 引擎相关的服务/组件字段
     -- 各服务/组件返回的具体错误/状态码
（2）各服务接口返回码定义：
1101xxxx --- 中心服务接口返回的状态、错误字段
1102xxxx --- 结构化引擎接口返回的状态、错误字段
1103xxxx --- 接入引擎接口返回的状态、错误字段
1104xxxx --- 布控引擎的返回的状态、错误字段
1105xxxx --- DeviceCloudMedia(流媒体) 的状态、错误字段
1106xxxx --- SipServer的状态、错误字段
*/


/*中心服务器回复OK代码*/
#define CENTER_RSP_OK_CODE	10000000

/*device cloud media回复状态码*/
#define MEDIA_RSP_CODE_OK				10000000 /*回复OK*/
#define MEDIA_RSP_CODE_PARAM_ERROR		11050001 /*请求参数错误*/
#define MEDIA_RSP_CODE_SERVICE_ERROR	11050002 /*服务器处理发生错误*/

/*sip server回复状态码*/
#define SIPSERVER_RSP_CODE_OK				10000000 /*回复OK*/
#define SIPSERVER_RSP_CODE_PARAM_ERROR		11060001 /*请求参数错误*/
#define SIPSERVER_RSP_CODE_SERVICE_ERROR	11060002 /*服务器处理发生错误*/


#define GET_JSON_CFG_INT(jsVal, strMember, iVal, iDefault) \
{\
	if ((jsVal).isMember(strMember))\
		iVal = atoi((jsVal)[strMember].asString().c_str());\
	else\
		iVal = iDefault;\
}

#define GET_JSON_CFG_STRING(jsVal, strMember, strVal)\
{\
	if ((jsVal).isMember(strMember))\
		strVal = (jsVal)[strMember].asString();\
}


typedef struct tagSSystemTime
{
    U16 usYear;
    U16 usMonth;
    U16 usDay;
    U16 usHour;
    U16 usMinute;
    U16 usSecond;
    U16 usMilliSec;
}SSystemTime;

/*获取系统开机以来的毫秒数，类似windows函数GetTickCount*/
U32 Comm_GetTickCount();

/*获取1970年至当前时间的毫秒数*/
long long Comm_GetMilliSecFrom1970();

/*获取文件长度*/
long Comm_FileSize(FILE *f);

/*获取当前的系统时间*/
void Comm_GetNowTime(SSystemTime *pTime);
	
/*从字符串中得到key=val的val值*/
int Comm_GetKeyValue(char *pszInStr, const char *pszKey, char *pszVal, int iValMaxSize);

/*字符串时间2018-11-13 12:05:00转换成unix时间，单位秒*/
long long Comm_StrTime2UnixTime(const char *pszStrTime);

/*unix时间转字符串2019-11-27T12:05:00*/
void Comm_UnixTime2StrTime(long long lUnixTime, char *pszStrTime);

#endif



