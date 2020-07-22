
#include "CodeConverter.h"
#include <string.h>
#include <iconv.h>


//是否是gbk编码字符串
bool CodeConverter_IsGBK(const char *str)
{
	unsigned int nBytes = 0;
	unsigned char chr = *str;
	bool bAllAscii = true;
	for (unsigned int i = 0; str[i] != '\0'; ++i)
	{
		chr = *(str + i);
		if ((chr & 0x80) != 0 && nBytes == 0)
		{
			bAllAscii = false;
		}
		if (nBytes == 0)
		{
			if (chr >= 0x80)
			{
				if (chr >= 0x81 && chr <= 0xFE)
				{
					nBytes = +2;
				}
				else
				{
					return false;
				}

				nBytes--;
			}
		}
		else
		{
			if (chr < 0x40 || chr>0xFE)
			{
				return false;
			}
			nBytes--;
		}
	}
	if (nBytes != 0)
	{
		return false;
	}

	if (bAllAscii)
	{
		return true;
	}
	return true;
}


// 转换字符编码
class CodeConverter {
private:
	iconv_t cd;
public:
	CodeConverter(const char *from_charset, const char *to_charset) {
		cd = iconv_open(to_charset, from_charset);
	}
	~CodeConverter() {
		iconv_close(cd);
	}
	int convert(const char *inbuf, size_t inlen, char *outbuf, size_t outlen) {
		char **pin = const_cast<char**>(&inbuf);
		char **pout = &outbuf;

		memset(outbuf, 0, outlen);
		return iconv(cd, pin, (size_t *)&inlen, pout, (size_t *)&outlen);
	}
};


// 字符编码，由GB2312转为UTF-8
int CodeConverter_GB2312ToUtf8(const char *pszGbkStr, int iGbkLen, char *pszUtf8Str, int iMaxUtf8Size)
{
	CodeConverter cc("gb2312", "utf-8");
	cc.convert(pszGbkStr, iGbkLen, pszUtf8Str, iMaxUtf8Size);
	int len = strlen(pszUtf8Str);
	return len;
	


#if 0
	iconv_t cd = iconv_open ("gb2312", "utf-8");
	if (cd == (iconv_t)-1)
	{
		return -1;
	}

	/* 进行转换
	size_t iconv(iconv_t cd,char **inbuf,size_t *inbytesleft,char **outbuf,size_t *outbytesleft);
   *@param cd iconv_open()产生的句柄
   *@param srcstart 需要转换的字符串
   *@param srclen 存放还有多少字符没有转换
   *@param tempoutbuf 存放转换后的字符串
   *@param outlen 存放转换后,tempoutbuf剩余的空间
   *
   * */
	char *srcstart = (char*)strGbkCode.c_str();
	size_t srclen = strGbkCode.length();
	char outbuf[4096] = {0};
	size_t outlen = 4096;
	char *tempoutbuf = outbuf;
	size_t ret = iconv(cd, &srcstart, &srclen, &tempoutbuf, &outlen);
	if (ret == -1)
	{
		/* 关闭句柄 */
		iconv_close (cd);
		return -1;	  
	}

	strUtf8 = outbuf;
	/* 关闭句柄 */
	iconv_close (cd);
	return (int)strUtf8.length();
#endif
}


