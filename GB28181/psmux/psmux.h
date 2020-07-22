
#ifndef __PS_MUX_H__
#define __PS_MUX_H__


#define PS_HDR_LEN  14
#define SYS_HDR_LEN 18
#define PSM_HDR_LEN 24
#define PES_HDR_LEN 19
#define RTP_HDR_LEN 12
#define RTP_HDR_SIZE 12
#define RTP_VERSION 2

#define PS_SYS_MAP_SIZE 24

#define PS_PES_PAYLOAD_SIZE (65535-13)
#define RTP_MAX_PACKET_BUFF 1300


//帧类型定义
enum NAL_type
{
    NAL_IDR,
    NAL_SPS,
    NAL_PPS,
    NAL_SEI,
    NAL_PFRAME,
    NAL_VPS,
    NAL_SEI_PREFIX,
    NAL_SEI_SUFFIX,
    NAL_other,
    NAL_TYPE_NUM
};

NAL_type getH264NALtype(unsigned char c);



/*H264视频进行PS封装

*/
int h264PsMux(unsigned char *pH264RawData, int iH264DataLen, NAL_type nalType, 
        unsigned long long s64Scr, unsigned char *pOutPsData);



#endif

