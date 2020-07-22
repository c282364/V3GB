/******************************************************************
* Author     : lizhigang (li.zhigang@intellif.com)
* CreateTime : 2019/7/25
* Copyright (c) 2019 Shenzhen Intellifusion Technologies Co., Ltd.
* File Desc  : 构造RTCP数据包
*******************************************************************/



#ifndef __RTCP_PACKET_H__
#define __RTCP_PACKET_H__



//rtcp结构体转成字节数组,参数都要求输入网络字节序
int makeRtcpPacketBuff(unsigned long ulSenderId, unsigned long ssrc, 
	unsigned long ulTimeStamp, unsigned short usSeq, unsigned char *pOutBuff);


#endif



