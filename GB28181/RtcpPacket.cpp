#include "RtcpPacket.h"
#include <string>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "MediaGateCommon.h"

#define memcpy_rtcpdata(buff, pos, val, len) {memcpy((buff+pos), &val, len); pos+=len;}


//rtcp结构体转成字节数组,参数都要求输入网络字节序
int makeRtcpPacketBuff(unsigned long ulSenderId, unsigned long ssrc, 
	unsigned long ulTimeStamp, unsigned short usSeq, unsigned char *pOutBuff)
{
	//DEBUG_LOG_MEDIAGATE("sender:%lu, ssrc:%lu, time:%lu, seq:%lu", ulSenderId, ssrc, ulTimeStamp, ulSeq);

//        0               1               2               3
//        0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//byte=0 |V=2|P|    RC   |   PT=SR=201   |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     4 |                         SSRC of sender                        |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//     8 |              identifier: ssrc of rtp				             |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    12 | fraction lost |  cumulative number of packets lost            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    16 | sequence number cycles count  | highest sequence number recved|
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    20 |           		interarrival jitter		                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    24 |                  last SR timestamp		                     |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    28 |           delay since last SR timestamp		                 |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    32 |V=2|P|    SC   |  PT=SDES=202  |             length            |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    36 |                          SSRC of sender                       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    40 |    CNAME=1    |     length    | user and domain name ...	 end(0)
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	int ret = 0;	
	unsigned long ulValueZero = 0;
	//构造receiver report
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//byte=0 |V=2|P|    RC   |   PT=SR=200   |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	pOutBuff[0] = 0x81; 
	pOutBuff[1] = 0xC9;
	pOutBuff[2] = 0x00; 
	pOutBuff[3] = 0x07;
	ret = 4;
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     4 |                         SSRC of sender                        |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	memcpy_rtcpdata(pOutBuff, ret, ulSenderId, 4);
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//	   8 |				identifier: ssrc of rtp 						 |
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	memcpy_rtcpdata(pOutBuff, ret, ssrc, 4);
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	  12 | fraction lost |	cumulative number of packets lost			 |
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	unsigned long ulFractLost = 0;
	memcpy_rtcpdata(pOutBuff, ret, ulFractLost, 4);
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    16 | sequence number cycles count  | highest sequence number recved|
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	unsigned short usSeqCyclesCount = 2;
	memcpy_rtcpdata(pOutBuff, ret, usSeqCyclesCount, 2);
	memcpy_rtcpdata(pOutBuff, ret, usSeq, 2);
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    20 |           		interarrival jitter		                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	memcpy_rtcpdata(pOutBuff, ret, ulValueZero, 4);
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	  24 |					last SR timestamp							 |
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	memcpy_rtcpdata(pOutBuff, ret, ulValueZero, 4);
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//	  28 |			 delay since last SR timestamp						 |
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	memcpy_rtcpdata(pOutBuff, ret, ulValueZero, 4);
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    32 |V=2|P|    SC   |  PT=SDES=202  |             length            |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	pOutBuff[ret] = 0x81; 
	pOutBuff[ret+1] = 0xCA;
	pOutBuff[ret+2] = 0x00; 
	pOutBuff[ret+3] = 0x06;
	ret += 4;
//		 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//	  36 |							SSRC of sender						 |
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	memcpy_rtcpdata(pOutBuff, ret, ulSenderId, 4);
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	  40 |	  CNAME=1	 |	   length	 | user and domain name ...  end(0)
//		 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	pOutBuff[ret] = 0x01; //cname=1
	ret++;
	const unsigned char ucTextLen = 15;
	pOutBuff[ret] = ucTextLen; //length=15
	ret++;
	char szText[ucTextLen+1] = "intellif-PC1161"; //TEXT
	memcpy(pOutBuff+ret, szText, ucTextLen);
	ret += ucTextLen;
	pOutBuff[ret] = 0x00; //end
	ret++;
	
	return ret;
}


