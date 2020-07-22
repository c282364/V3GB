/******************************************************************
* Author     : lizhigang (li.zhigang@intellif.com)
* CreateTime : 2019/7/2
* Copyright (c) 2019 Shenzhen Intellifusion Technologies Co., Ltd.
* File Desc  : 定时器接口
*******************************************************************/

#ifndef __TIMER_H__
#define __TIMER_H__

typedef void (*fnTimerCallBack)(int iTimerId);


long Comm_SetTimer(unsigned long ulMilliSec, fnTimerCallBack fnCB);


int Comm_KillTimer(long lTimerId);

#endif








