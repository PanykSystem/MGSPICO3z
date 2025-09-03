#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "../CMsCount.h"
#include "../CReadFileStream.h"
#include "tgf.h"
#include "../IStreamPlayer.h"

class CHarz80Ctrl;

class CTgfPlayer : public IStreamPlayer
{
private:
	CReadFileStream *m_pStrm;
	bool m_bFileIsOK;
	bool m_bPlay;
	int m_RepeatCount;
	int m_CurStepCount;
	int m_TotalStepCount;
	uint32_t m_VsyncCount;
	CMsCount g_Mtc;
	bool m_bFirst;
	tgf::timecode_t m_padding;
	tgf::timecode_t m_oldBase;
	CHarz80Ctrl *m_pHarz;
	int m_Volume;	// 0-15
	uint8_t m_ReqVolume[2];

public:
	explicit CTgfPlayer(CHarz80Ctrl *pHarz);
	virtual ~CTgfPlayer();

// member of IStreamPlayer
public:
	bool SetTargetFile(const char *pFname);
	int GetTotalStepCount() const;
	int GetCurStepCount() const;
	int GetRepeatCount() const;
	uint32_t GetPlayTime() const;
	void Start();
	void Stop();
	bool FetchFile();
	void PlayLoop();
	void SetHarzVolume(const int v);
	bool IsPlaying() const;

private:
	void mute();
};

