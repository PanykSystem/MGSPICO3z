#include "CMsCount.h"

uint32_t GetTimerCounterMS();


CMsCount::CMsCount()
{
	m_Begin = GetTimerCounterMS();
	m_Timer = 0;
	m_bTimeout = false;
	return;
}

CMsCount::CMsCount(uint32_t t)
{
	m_Begin = GetTimerCounterMS();
	m_Timer = t;
	m_bTimeout = false;
	return;
}


CMsCount::~CMsCount()
{
	// do nothing
	return;
}

/**
 * タイマーをリセットし、指定時間で再スタートする
 * @param t トリガー時間[ms]（現在から何ms後を示す）
 */
void CMsCount::Reset(uint32_t t)
{
	m_Begin = GetTimerCounterMS();
	m_Timer = t;
	m_bTimeout = false;
	return;
}

/**
 * 設定されているトリガー時間を取得する
 * @return トリガー時間[ms]、設定されていない場合は0を返す
 */
uint32_t CMsCount::GetTriggerTimeMS() const
{
	return m_Timer;
}

/**
 * 開始からの経過時間を取得する
 * @return 経過時間[ms]
 */
uint32_t CMsCount::GetTime() const
{
	return GetTimerCounterMS() - m_Begin;
}

/**
 * タイムアウトしているかを調べる
 * @param bContinuous true:タイムアウトした場合、タイマーを再スタートする
 * @return true:タイムアウトしている、false:タイムアウトしていない
 */
bool CMsCount::IsTimeOut(const bool bContinuous)
{
	if( m_bTimeout )
		return true;
	if( m_Timer == 0 )
		return false;
	if( m_Timer <= (GetTimerCounterMS() - m_Begin))
	{
		if( bContinuous ){
			m_Begin = GetTimerCounterMS();
			m_bTimeout = false;
		}
		else {
			m_bTimeout = true;
		}
		return true;
	}
	return false;
}

// タイムアウトするまではtrueを返す。
// タイマーがスタートしていないもしくはタイムアウト後はfalseを返す
bool CMsCount::IsMidway()
{
	if( 0 < m_Timer ) {
		if( (GetTimerCounterMS() - m_Begin) < m_Timer ){
			return true;
		}
		else {
			m_Timer = 0;
			m_bTimeout = false;
		}
	}
	return false;
}

/**
 * タイマーをキャンセルする
 */
void CMsCount::Cancel()
{
	m_Timer = 0;
	m_bTimeout = false;
}

/**
 * タイマーが有効かを調べる
 * @return true:有効、false:無効
 */
bool CMsCount::IsValid() const
{
	return (m_Timer == 0) ? false : true;
}

/**
 * 進捗を取得する
 * @return 進捗(0.0f～1.0f)、タイマーが設定されていない場合は1.0fを返す
 */
float CMsCount::GetProgress() const
{
	if( m_Timer == 0 )
		return 1.0f;
	const uint32_t now = GetTime();
	const float prog = (float)now / (float)m_Timer;
	return (1.0f <= prog ) ? 1.0f : prog;
}