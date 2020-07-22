#ifndef _STRINGUTIL_H
#define _STRINGUTIL_H

#include<string>
using namespace std;

namespace stringutil
{
	void trimleft(string &str,char c=' ');

	void trimright(string &str,char c=' ');

	void trim(string &str);

}

#endif

