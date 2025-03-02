#include "CMsCount.h"

uint32_t GetTimerCounterMS();


CMsCount::CMsCount()
{
	m_Begin = GetTimerCounterMS();
	m_Timer = 0;
	return;
}

CMsCount::CMsCount(uint32_t t)
{
	m_Begin = GetTimerCounterMS();
	m_Timer = t;
	return;
}


CMsCount::~CMsCount()
{
	// do nothing
	return;
}

void CMsCount::Reset(uint32_t t)
{
	m_Begin = GetTimerCounterMS();
	m_Timer = t;
	return;
}

uint32_t CMsCount::GetTime()
{
	return GetTimerCounterMS() - m_Begin;
}

bool CMsCount::IsTimeOut(const bool bContinuous)
{
	if( m_Timer == 0 )
		return false;
	if( m_Timer <= (GetTimerCounterMS() - m_Begin))
	{
		if( bContinuous )
			m_Begin = GetTimerCounterMS();
		return true;
	}
	return false;
}

bool CMsCount::IsMidway()
{
	if( 0 < m_Timer ) {
		if( (GetTimerCounterMS() - m_Begin) < m_Timer ){
			return true;
		}
		else {
			m_Timer = 0;
		}
	}
	return false;
}

void CMsCount::Cancel()
{
	m_Timer = 0;
}

bool CMsCount::IsValid() const
{
	return (m_Timer == 0) ? false : true;
}
