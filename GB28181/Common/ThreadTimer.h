/******************************************************************
* Author     : lizhigang (li.zhigang@intellif.com)
* CreateTime : 2019/7/9
* Copyright (c) 2019 Shenzhen Intellifusion Technologies Co., Ltd.
* File Desc  : 线程循环实现定时器功能
*******************************************************************/

#ifndef __THREAD_TIMER_H__
#define __THREAD_TIMER_H__

typedef void* HTIMER;

//定时器触发函数
typedef void (*fnThreadTimerCB)(HTIMER hTimer, void *pUserData);

//开始定时器
HTIMER Comm_CreateThreadTimer(unsigned long ulMilliSec, fnThreadTimerCB fn, void *pUserData);

//结束定时器
int Comm_ReleaseThreadTimer(HTIMER hTimer);


#endif


