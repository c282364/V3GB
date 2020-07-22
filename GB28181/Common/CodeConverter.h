/******************************************************************
* Author     : lizhigang (li.zhigang@intellif.com)
* CreateTime : 2019/7/13
* Copyright (c) 2019 Shenzhen Intellifusion Technologies Co., Ltd.
* File Desc  : 封装字符编码格式的转换
*******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string>

//是否是gbk编码字符串
bool CodeConverter_IsGBK(const char *str);

/* 字符编码，由GB2312转为UTF-8
int CodeConverter_GB2312ToUtf8(std::string &strGbkCode, std::string &strUtf8);
gcc5.4中C++11对一些变量的解析不太一样（表达不太好）
通过命令查看.o文件的导出接口，nm -C CodeConverter.o
CodeConverter_GB2312ToUtf8(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&, 
	std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&)
在链接时候会报错，undefined reference to `CodeConverter_GB2312ToUtf8(std::string&, std::string&)'
为了规避gcc5.4编译问题，CodeConverter_GB2312ToUtf8接口重新替换成C风格接口
*/
int CodeConverter_GB2312ToUtf8(const char *pszGbkStr, int iGbkLen, char *pszUtf8Str, int iMaxUtf8Size);


