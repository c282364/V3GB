#include "GB28181StreamInstance.h"


CGB28181StreamInstance::CGB28181StreamInstance()
{
	m_cid = 0;
	m_did = 0;
	m_bAutoRelease = false;
}

CGB28181StreamInstance::~CGB28181StreamInstance()
{

}

int CGB28181StreamInstance::releaseInstance()
{
	return 0;
}


int CGB28181StreamInstance::getCID()
{
	return m_cid;
}

int CGB28181StreamInstance::getDID()
{
	return m_did;
}

std::string CGB28181StreamInstance::getStreamId()
{
	return m_strStreamId;
}

bool CGB28181StreamInstance::getAutoRelease()
{
	return m_bAutoRelease;
}

void CGB28181StreamInstance::setCID(int cid)
{
	m_cid = cid;
}

void CGB28181StreamInstance::setDID(int did)
{
	m_did = did;
}


void CGB28181StreamInstance::setStreamId(const char *pszStreamId)
{
	m_strStreamId = pszStreamId;
}

void CGB28181StreamInstance::setAutoRelease(bool bVal)
{
	m_bAutoRelease = bVal;
}



