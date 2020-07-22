#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "psmux.h"

typedef struct bits_buffer_s
{
    int i_size;
    int i_data;
    unsigned char i_mask;
    unsigned char *p_data;
} bits_buffer_t;

typedef struct tagData_Info_s
{
	unsigned char IFrame;
	char *szBuff;
	unsigned short u16CSeq;
	long long s64CurPts;
	unsigned long u32Ssrc;
}Data_Info_s;

int gb28181_make_rtp_header(char *pData, int marker_flag, unsigned short cseq, long long curpts, unsigned int ssrc);

int gb28181_streampackageForH264(char *pData, int nFrameLen, Data_Info_s* pPacker, int stream_type);

int gb28181_make_ps_header(char *pData, unsigned long long s64Scr);

int gb28181_make_sys_header(char *pData);

int gb28181_make_psm_header(char *pData);

int gb28181_make_pes_header(char *pData, int stream_id, int payload_len, unsigned long long pts, unsigned long long dts);

int gb28181_send_rtp_pack(char *databuff, int nDataLen, int mark_flag, Data_Info_s* pPacker);


/***
 *@remark:  讲传入的数据按地位一个一个的压入数据
 *@param :  buffer   [in]  压入数据的buffer
 *          count    [in]  需要压入数据占的位数
 *          bits     [in]  压入的数值
 */
#define bits_write(buffer, count, bits)\
{\
	bits_buffer_s *p_buffer = (buffer);\
	int i_count = (count);\
	uint64_t i_bits = (bits);\
	while( i_count > 0 )\
	{\
		i_count--;\
		if( ( i_bits >> i_count )&0x01 )\
		{\
			p_buffer->p_data[p_buffer->i_data] |= p_buffer->i_mask;\
		}\
		else\
		{\
			p_buffer->p_data[p_buffer->i_data] &= ~p_buffer->i_mask;\
		}\
		p_buffer->i_mask >>= 1;         /*操作完一个字节第一位后，操作第二位*/\
		if( p_buffer->i_mask == 0 )     /*循环完一个字节的8位后，重新开始下一位*/\
		{\
			p_buffer->i_data++;\
			p_buffer->i_mask = 0x80;\
		}\
	}\
}


int gb28181_make_rtp_header(char *pData, int marker_flag, unsigned short cseq, long long curpts, unsigned int ssrc)
{
	bits_buffer_s  	bitsBuffer;
	if (pData == NULL)
		return -1;
	bitsBuffer.i_size = RTP_HDR_LEN;
	bitsBuffer.i_data = 0;
	bitsBuffer.i_mask = 0x80;
	bitsBuffer.p_data =	(unsigned char *)(pData);
	memset(bitsBuffer.p_data, 0, RTP_HDR_SIZE);
	bits_write(&bitsBuffer, 2, RTP_VERSION);	/* rtp version 	*/
	bits_write(&bitsBuffer, 1, 0);				/* rtp padding 	*/
	bits_write(&bitsBuffer, 1, 0);				/* rtp extension 	*/
	bits_write(&bitsBuffer, 4, 0);				/* rtp CSRC count */
	bits_write(&bitsBuffer, 1, (marker_flag));			/* rtp marker  	*/
	bits_write(&bitsBuffer, 7, 96);			/* rtp payload type*/
	bits_write(&bitsBuffer, 16, (cseq));			/* rtp sequence 	 */
	bits_write(&bitsBuffer, 32, (curpts)); 		/* rtp timestamp 	 */
	bits_write(&bitsBuffer, 32, (ssrc)); 		/* rtp SSRC	 	 */
	return 0;
}

/***
 *@remark:  音视频数据的打包成ps流，并封装成rtp
 *@param :  pData      [in] 需要发送的音视频数据
 *          nFrameLen  [in] 发送数据的长度
 *          pPacker    [in] 数据包的一些信息，包括时间戳，rtp数据buff，发送的socket相关信息
 *          stream_type[in] 数据类型 0 视频 1 音频
 *@return:  0 success others failed
*/
 
int gb28181_streampackageForH264(char *pData, int nFrameLen, Data_Info_s* pPacker, int stream_type)
{
	char szTempPacketHead[256];
	int  nSizePos = 0;
	int  nSize = 0;		
	char *pBuff = NULL;
	memset(szTempPacketHead, 0, 256);
	//---- 1 package for ps header 
	gb28181_make_ps_header(szTempPacketHead + nSizePos, pPacker->s64CurPts);
	nSizePos += PS_HDR_LEN;	
	//----2 system header 
	if( pPacker->IFrame == 1 )
	{
        // 如果是I帧的话，则添加系统头
		gb28181_make_sys_header(szTempPacketHead + nSizePos);
		nSizePos += SYS_HDR_LEN;
        //这个地方我是不管是I帧还是p帧都加上了map的，貌似只是I帧加也没有问题
//		gb28181_make_psm_header(szTempPacketHead + nSizePos);
//		nSizePos += PSM_HDR_LEN;
 
	}
    // psm头 (也是map)
	gb28181_make_psm_header(szTempPacketHead + nSizePos);
    nSizePos += PSM_HDR_LEN;
 
    //加上rtp发送出去，这样的话，后面的数据就只要分片分包就只有加上pes头和rtp头了
	if(gb28181_send_rtp_pack(szTempPacketHead, nSizePos, 0, pPacker) != 0 )
		return -1;	
 
    // 这里向后移动是为了方便拷贝pes头
    //这里是为了减少后面音视频裸数据的大量拷贝浪费空间，所以这里就向后移动，在实际处理的时候，要注意地址是否越界以及覆盖等问题
	pBuff = pData - PES_HDR_LEN;
	while(nFrameLen > 0)
	{
        //每次帧的长度不要超过short类型，过了就得分片进循环行发送
		nSize = (nFrameLen > PS_PES_PAYLOAD_SIZE) ? PS_PES_PAYLOAD_SIZE : nFrameLen;
        // 添加pes头
		gb28181_make_pes_header(pBuff, stream_type ? 0xC0:0xE0, nSize, (pPacker->s64CurPts / 100), (pPacker->s64CurPts/300));
 
        //最后在添加rtp头并发送数据
		if( gb28181_send_rtp_pack(pBuff, nSize + PES_HDR_LEN, ((nSize == nFrameLen)?1:0), pPacker) != 0 )
		{
			printf("gb28181_send_pack failed!\n");
			return -1;
		}
        //分片后每次发送的数据移动指针操作
		nFrameLen -= nSize;
        //这里也只移动nSize,因为在while向后移动的pes头长度，正好重新填充pes头数据
        pBuff     += nSize;
		
	}
	return 0;
}

/***
 *@remark:   ps头的封装,里面的具体数据的填写已经占位，可以参考标准
 *@param :   pData  [in] 填充ps头数据的地址
 *           s64Src [in] 时间戳
 *@return:   0 success, others failed
*/
int gb28181_make_ps_header(char *pData, unsigned long long s64Scr)
{
	unsigned long long lScrExt = (s64Scr) % 100;	
    s64Scr = s64Scr / 100;
    // 这里除以100是由于sdp协议返回的video的频率是90000，帧率是25帧/s，所以每次递增的量是3600,
    // 所以实际你应该根据你自己编码里的时间戳来处理以保证时间戳的增量为3600即可，
    //如果这里不对的话，就可能导致卡顿现象了
	bits_buffer_s  	bitsBuffer;
	bitsBuffer.i_size = PS_HDR_LEN;	
	bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80; // 二进制：10000000 这里是为了后面对一个字节的每一位进行操作，避免大小端夸字节字序错乱
	bitsBuffer.p_data =	(unsigned char *)(pData);
	memset(bitsBuffer.p_data, 0, PS_HDR_LEN);
    bits_write(&bitsBuffer, 32, 0x000001BA);			/*start codes*/
	bits_write(&bitsBuffer, 2, 	1);						/*marker bits '01b'*/
    bits_write(&bitsBuffer, 3, 	(s64Scr>>30)&0x07);     /*System clock [32..30]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
    bits_write(&bitsBuffer, 15, (s64Scr>>15)&0x7FFF);   /*System clock [29..15]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
    bits_write(&bitsBuffer, 15, s64Scr&0x7fff);         /*System clock [29..15]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 9, 	lScrExt&0x01ff);		/*System clock [14..0]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 22, (255)&0x3fffff);		/*bit rate(n units of 50 bytes per second.)*/
	bits_write(&bitsBuffer, 2, 	3);						/*marker bits '11'*/
	bits_write(&bitsBuffer, 5, 	0x1f);					/*reserved(reserved for future use)*/
	bits_write(&bitsBuffer, 3, 	0);						/*stuffing length*/
	return 0;
}


/***
 *@remark:   sys头的封装,里面的具体数据的填写已经占位，可以参考标准
 *@param :   pData  [in] 填充ps头数据的地址
 *@return:   0 success, others failed
*/
int gb28181_make_sys_header(char *pData)
{
	
	bits_buffer_s  	bitsBuffer;
	bitsBuffer.i_size = SYS_HDR_LEN;
	bitsBuffer.i_data = 0;
	bitsBuffer.i_mask = 0x80;
	bitsBuffer.p_data =	(unsigned char *)(pData);
	memset(bitsBuffer.p_data, 0, SYS_HDR_LEN);
	/*system header*/
	bits_write( &bitsBuffer, 32, 0x000001BB);	/*start code*/
    bits_write( &bitsBuffer, 16, SYS_HDR_LEN-6);/*header_length 表示次字节后面的长度，后面的相关头也是次意思*/
    bits_write( &bitsBuffer, 1,	 1);            /*marker_bit*/
	bits_write( &bitsBuffer, 22, 50000);		/*rate_bound*/
    bits_write( &bitsBuffer, 1,  1);            /*marker_bit*/
    bits_write( &bitsBuffer, 6,  1);            /*audio_bound*/
    bits_write( &bitsBuffer, 1,  0);            /*fixed_flag */
    bits_write( &bitsBuffer, 1,  1);        	/*CSPS_flag */
    bits_write( &bitsBuffer, 1,  1);        	/*system_audio_lock_flag*/
    bits_write( &bitsBuffer, 1,  1);        	/*system_video_lock_flag*/
    bits_write( &bitsBuffer, 1,  1);        	/*marker_bit*/
    bits_write( &bitsBuffer, 5,  1);        	/*video_bound*/
    bits_write( &bitsBuffer, 1,  0);        	/*dif from mpeg1*/
    bits_write( &bitsBuffer, 7,  0x7F);     	/*reserver*/
	/*audio stream bound*/
    bits_write( &bitsBuffer, 8,  0xC0);         /*stream_id*/
    bits_write( &bitsBuffer, 2,  3);        	/*marker_bit */
    bits_write( &bitsBuffer, 1,  0);            /*PSTD_buffer_bound_scale*/
    bits_write( &bitsBuffer, 13, 512);          /*PSTD_buffer_size_bound*/
	/*video stream bound*/
    bits_write( &bitsBuffer, 8,  0xE0);         /*stream_id*/
    bits_write( &bitsBuffer, 2,  3);        	/*marker_bit */
    bits_write( &bitsBuffer, 1,  1);        	/*PSTD_buffer_bound_scale*/
    bits_write( &bitsBuffer, 13, 2048);     	/*PSTD_buffer_size_bound*/
	return 0;
}


/***
 *@remark:   psm头的封装,里面的具体数据的填写已经占位，可以参考标准
 *@param :   pData  [in] 填充ps头数据的地址
 *@return:   0 success, others failed
*/
int gb28181_make_psm_header(char *pData)
{
	
	bits_buffer_s  	bitsBuffer;
	bitsBuffer.i_size = PSM_HDR_LEN; 
	bitsBuffer.i_data = 0;
	bitsBuffer.i_mask = 0x80;
	bitsBuffer.p_data =	(unsigned char *)(pData);
	memset(bitsBuffer.p_data, 0, PS_SYS_MAP_SIZE);
	bits_write(&bitsBuffer, 24,0x000001);	/*start code*/
	bits_write(&bitsBuffer, 8, 0xBC);		/*map stream id*/
	bits_write(&bitsBuffer, 16,18);			/*program stream map length*/ 
	bits_write(&bitsBuffer, 1, 1);			/*current next indicator */
	bits_write(&bitsBuffer, 2, 3);			/*reserved*/
	bits_write(&bitsBuffer, 5, 0); 			/*program stream map version*/
    bits_write(&bitsBuffer, 7, 0x7F);		/*reserved */
	bits_write(&bitsBuffer, 1, 1);			/*marker bit */
	bits_write(&bitsBuffer, 16,0); 			/*programe stream info length*/
	bits_write(&bitsBuffer, 16, 8); 		/*elementary stream map length	is*/
	/*audio*/
    bits_write(&bitsBuffer, 8, 0x90);       /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xC0);		/*elementary_stream_id*/
	bits_write(&bitsBuffer, 16, 0); 		/*elementary_stream_info_length is*/
	/*video*/
    bits_write(&bitsBuffer, 8, 0x1B);       /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xE0);		/*elementary_stream_id*/
	bits_write(&bitsBuffer, 16, 0); 		/*elementary_stream_info_length */
	/*crc (2e b9 0f 3d)*/
	bits_write(&bitsBuffer, 8, 0x45);		/*crc (24~31) bits*/
	bits_write(&bitsBuffer, 8, 0xBD);		/*crc (16~23) bits*/
	bits_write(&bitsBuffer, 8, 0xDC);		/*crc (8~15) bits*/
	bits_write(&bitsBuffer, 8, 0xF4);		/*crc (0~7) bits*/
	return 0;
}

/***
 *@remark:   pes头的封装,里面的具体数据的填写已经占位，可以参考标准
 *@param :   pData      [in] 填充ps头数据的地址
 *           stream_id  [in] 码流类型
 *           paylaod_len[in] 负载长度
 *           pts        [in] 时间戳
 *           dts        [in]
 *@return:   0 success, others failed
*/
int gb28181_make_pes_header(char *pData, int stream_id, int payload_len, unsigned long long pts, unsigned long long dts)
{
	
	bits_buffer_s  	bitsBuffer;
	bitsBuffer.i_size = PES_HDR_LEN;
	bitsBuffer.i_data = 0;
	bitsBuffer.i_mask = 0x80;
	bitsBuffer.p_data =	(unsigned char *)(pData);
	memset(bitsBuffer.p_data, 0, PES_HDR_LEN);
	/*system header*/
	bits_write( &bitsBuffer, 24,0x000001);	/*start code*/
	bits_write( &bitsBuffer, 8, (stream_id));	/*streamID*/
	bits_write( &bitsBuffer, 16,(payload_len)+13);	/*packet_len*/ //指出pes分组中数据长度和该字节后的长度和
	bits_write( &bitsBuffer, 2, 2 );		/*'10'*/
	bits_write( &bitsBuffer, 2, 0 );		/*scrambling_control*/
	bits_write( &bitsBuffer, 1, 0 );		/*priority*/
	bits_write( &bitsBuffer, 1, 0 );		/*data_alignment_indicator*/
	bits_write( &bitsBuffer, 1, 0 );		/*copyright*/
	bits_write( &bitsBuffer, 1, 0 );		/*original_or_copy*/
	bits_write( &bitsBuffer, 1, 1 );		/*PTS_flag*/
	bits_write( &bitsBuffer, 1, 1 );		/*DTS_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*ESCR_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*ES_rate_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*DSM_trick_mode_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*additional_copy_info_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*PES_CRC_flag*/
	bits_write( &bitsBuffer, 1, 0 );		/*PES_extension_flag*/
	bits_write( &bitsBuffer, 8, 10);		/*header_data_length*/ 
	// 指出包含在 PES 分组标题中的可选字段和任何填充字节所占用的总字节数。该字段之前
	//的字节指出了有无可选字段。
	
	/*PTS,DTS*/	
    bits_write( &bitsBuffer, 4, 3 );                    /*'0011'*/
    bits_write( &bitsBuffer, 3, ((pts)>>30)&0x07 );     /*PTS[32..30]*/
	bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,((pts)>>15)&0x7FFF);    /*PTS[29..15]*/
	bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,(pts)&0x7FFF);          /*PTS[14..0]*/
	bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 4, 1 );                    /*'0001'*/
    bits_write( &bitsBuffer, 3, ((dts)>>30)&0x07 );     /*DTS[32..30]*/
	bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,((dts)>>15)&0x7FFF);    /*DTS[29..15]*/
	bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,(dts)&0x7FFF);          /*DTS[14..0]*/
	bits_write( &bitsBuffer, 1, 1 );
	return 0;
}

int SendDataBuff(char *databuff, int len, Data_Info_s* pPacker)
{
	return len;
}


/***
 *@remark:   rtp头的打包，并循环发送数据
 *@param :   pData      [in] 发送的数据地址
 *           nDatalen   [in] 发送数据的长度
 *           mark_flag  [in] mark标志位
 *           curpts     [in] 时间戳
 *           pPacker    [in] 数据包的基本信息
 *@return:   0 success, others failed
*/
 
int gb28181_send_rtp_pack(char *databuff, int nDataLen, int mark_flag, Data_Info_s* pPacker)
{
	int nRes = 0;
	int nPlayLoadLen = 0;
	int nSendSize    = 0;
	char szRtpHdr[RTP_HDR_LEN];
	int nRet = 0;
	memset(szRtpHdr, 0, RTP_HDR_LEN);
	
    if(nDataLen + RTP_HDR_LEN <= RTP_MAX_PACKET_BUFF)// 1460 pPacker指针本来有一个1460大小的buffer数据缓存
    {
        // 一帧数据发送完后，给mark标志位置1
		gb28181_make_rtp_header(szRtpHdr, ((mark_flag == 1 )? 1 : 0 ), ++pPacker->u16CSeq, (pPacker->s64CurPts /300), pPacker->u32Ssrc);
		memcpy(pPacker->szBuff, szRtpHdr, RTP_HDR_LEN);
		memcpy(pPacker->szBuff + RTP_HDR_LEN, databuff, nDataLen);
        nRet = SendDataBuff(databuff, RTP_HDR_LEN + nSendSize, pPacker);
		if (nRes != (RTP_HDR_LEN + nDataLen))
		{
			printf(" udp send error !\n");
			return -1;
		}
		
	}
	else 
	{
		nPlayLoadLen = RTP_MAX_PACKET_BUFF - RTP_HDR_LEN; // 每次只能发送的数据长度 除去rtp头
		gb28181_make_rtp_header(pPacker->szBuff, 0, ++pPacker->u16CSeq, (pPacker->s64CurPts/100), pPacker->u32Ssrc);
		memcpy(pPacker->szBuff + RTP_HDR_LEN, databuff, nPlayLoadLen);
        nRet = SendDataBuff(databuff, RTP_HDR_LEN + nSendSize, pPacker);
		if (nRes != (RTP_HDR_LEN + nPlayLoadLen))
		{
			printf(" udp send error !\n");
			return -1;
		}
		
		nDataLen -= nPlayLoadLen;
		// databuff += (nPlayLoadLen - RTP_HDR_LEN);
		databuff += nPlayLoadLen; // 表明前面到数据已经发送出去 		
		databuff -= RTP_HDR_LEN; // 用来存放rtp头
		while(nDataLen > 0)
		{
			if(nDataLen <= nPlayLoadLen)
			{
				//一帧数据发送完，置mark标志位
				gb28181_make_rtp_header(databuff, mark_flag, ++pPacker->u16CSeq, (pPacker->s64CurPts/100), pPacker->u32Ssrc);
				nSendSize = nDataLen;
			}
			else 
			{
				gb28181_make_rtp_header(databuff, 0, ++pPacker->u16CSeq, (pPacker->s64CurPts/100), pPacker->u32Ssrc);
				nSendSize = nPlayLoadLen;
			}

			nRet = SendDataBuff(databuff, RTP_HDR_LEN + nSendSize, pPacker);
			if (nRes != (RTP_HDR_LEN + nSendSize))
			{
				printf(" udp send error !\n");
				return -1;
			}
			nDataLen -= nSendSize;
			databuff += nSendSize; 
			//因为buffer指针已经向后移动一次rtp头长度后，
			//所以每次循环发送rtp包时，只要向前移动裸数据到长度即可，这是buffer指针实际指向到位置是
			//databuff向后重复的rtp长度的裸数据到位置上 
 
		}
 
	}
	return 0;
}


NAL_type getH264NALtype(unsigned char c)
{
    switch(c & 0x1f){
        case 6:
            return NAL_SEI;
            break;
        case 7:
            return NAL_SPS;
            break;
        case 8:
            return NAL_PPS;
            break;
        case 5:
            return NAL_IDR;
            break;
        case 1:
            return NAL_PFRAME;
            break;
        default:
            return NAL_other;
            break;
    }
    return NAL_other;
}

int addPesHeader(unsigned char *pH264RawData, int iH264DataLen, unsigned long long s64Scr, unsigned char *pOutPsData)
{
	int iLeftFrameLen = iH264DataLen;
	int iDataPos = 0;
	int iOutPsDataLen = 0;
	while (iLeftFrameLen > 0)
	{	

		int nSize = (iLeftFrameLen > PS_PES_PAYLOAD_SIZE) ? PS_PES_PAYLOAD_SIZE : iLeftFrameLen;
		unsigned char btPesHeader[256] = {0};
		//gb28181_make_pes_header((char*)btPesHeader, stream_type ? 0xC0:0xE0, nSize, (pPacker->s64CurPts / 100), (pPacker->s64CurPts/300));
		gb28181_make_pes_header((char*)btPesHeader, 0xE0, nSize, s64Scr, s64Scr);

		memcpy(pOutPsData+iOutPsDataLen, btPesHeader, PES_HDR_LEN);
		iOutPsDataLen += PES_HDR_LEN;

		memcpy(pOutPsData+iOutPsDataLen, (unsigned char*)pH264RawData+iDataPos, nSize);
		iOutPsDataLen += nSize;
		iDataPos += nSize;

		iLeftFrameLen -= nSize;
	}
	
	return iOutPsDataLen;
}

int h264PsMux(unsigned char *pH264RawData, int iH264DataLen, NAL_type nalType, 
        unsigned long long s64Scr, unsigned char *pOutPsData)
{
	int iRetPsLen = 0;
	int len = 0;
	unsigned char btPsHeader[256] = {0};
	unsigned char btSystemHeader[256] = {0};
	unsigned char btPsmHeader[256] = {0};

	switch (nalType)
	{
		case NAL_SPS:
			gb28181_make_ps_header((char*)((char*)(pOutPsData+iRetPsLen)), s64Scr); 			
			iRetPsLen += PS_HDR_LEN;
			
			gb28181_make_sys_header((char*)(pOutPsData+iRetPsLen));
			iRetPsLen += SYS_HDR_LEN;
			
			gb28181_make_psm_header((char*)(pOutPsData+iRetPsLen));
			iRetPsLen += PSM_HDR_LEN;
			
			len = addPesHeader(pH264RawData, iH264DataLen, s64Scr, pOutPsData+iRetPsLen);
			iRetPsLen += len;
		break;

		case NAL_PPS:
			iRetPsLen = addPesHeader(pH264RawData, iH264DataLen, s64Scr, pOutPsData);			
		break;

		case NAL_IDR:
			iRetPsLen = addPesHeader(pH264RawData, iH264DataLen, s64Scr, pOutPsData);
		break;

		case NAL_PFRAME:
			gb28181_make_ps_header((char*)(pOutPsData+iRetPsLen), s64Scr);
			iRetPsLen += PS_HDR_LEN;
			
			len = addPesHeader(pH264RawData, iH264DataLen, s64Scr, pOutPsData+iRetPsLen);
			iRetPsLen += len;
		break;

		default:
		return iRetPsLen;	
	}


	return iRetPsLen;
}








