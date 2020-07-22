#ifndef _INIFILE_H
#define _INIFILE_H

#include <map>
#include <vector>
#include <string>
#include <string.h>

#include <iostream>
#include <fstream>  
#include <streambuf>

using namespace std;

namespace inifile
{
const int RET_OK  = 0;
const int RET_ERR = -1;
const string delim = "\n";
const string m_note = ";";
const string m_delim_tab = "    ";
struct IniItem_T
{
    string key;
    string value;
    string comment;
};

struct IniSection_T
{
	typedef vector<IniItem_T>::iterator iterator;
    iterator begin() {return items.begin();}
    iterator end() {return items.end();}

    string name;
    string comment;
	vector<IniItem_T> items;
};


typedef enum {
    IFACE_INI_PARAM_TYPE_NAME = 0x0,
    IFACE_INI_PARAM_TYPE_DATA = 0x1,
};

class IniFile
{
public: 
    IniFile();
    ~IniFile(){release();}

public:
	typedef map<string, IniSection_T *>::iterator iterator;

    iterator begin() {return sections_.begin();}
    iterator end() {return sections_.end();}
public:
    int openini(const string & strIniData, int dwType);
    int save();
    int saveas(const string &fname);
    string getStringValue(const string &section,const string &key,int &ret);
    int getIntValue(const string &section,const string &key,int &ret);
    double getDoubleValue(const string &section,const string &key,int &ret);
    int getValue(const string &section,const string &key,string &value);
    int getValue(const string &section,const string &key,string &value,string &comment);
    int getValues(const string &section,const string &key,vector<string> &values);
    int getValues(const string &section,const string &key,vector<string> &value,vector<string> &comments);
    int getSectionValue(const string &section, vector<IniItem_T> &vecValue);
    bool hasSection(const string &section) ;
    bool hasKey(const string &section,const string &key) ;
    
    int getSectionComment(const string &section,string & comment);
    int setSectionComment(const string &section,const string & comment);
    void getCommentFlags(vector<string> &flags);
    void setCommentFlags(const vector<string> &flags);
    int setValue(const string &section,const string &key,const string &value,const string &comment="");
	int setIntValue(const string &section, const string &key, const int &value, const string &comment = "");
	int setDoubleValue(const string &section, const string &key, const double &value, const string &comment = "");
    void deleteSection(const string &section);
    void deleteKey(const string &section,const string &key);
    void cleanSectionValue(const string &section);
private:
	IniSection_T *getSection(const string &section = "");
    void release();
    //int  getline(string &str);
    bool isComment(const string &str);
    bool parse(const string &content,string &key,string &value,string & comment, char c= '=');
    //for dubug
    void print();
    

    
private:

    int                      mdwIniType;

	map<string, IniSection_T *> sections_;
    string                   fname_;
    vector<string>           flags_;
};

}

#endif
