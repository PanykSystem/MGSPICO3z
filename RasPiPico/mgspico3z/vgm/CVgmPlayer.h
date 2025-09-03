#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "../CReadFileStream.h"
#include "../IStreamPlayer.h"
#include "vgm.h"

class CHarz80Ctrl;

class CVgmPlayer : public IStreamPlayer
{
private:
	//
	vgm::HEADER m_VgmHeader;
	//
	CReadFileStream *m_pStrm;
	bool m_bKick;
	bool m_bFileIsOK;
	bool m_bPlay;
	int m_RepeatCount;
	int m_CurStepCount;
	int m_TotalStepCount;
	uint32_t m_VsyncCount;
	uint32_t m_CompletedNotes;
	bool m_bFirst;
	CHarz80Ctrl *m_pHarz;
	//
	uint32_t	m_WaitSamples;
	uint64_t	m_StartTime;
	uint64_t	m_PlayTime;
	//
	int m_Volume;	// 0-15
	uint8_t m_ReqVolume[2];

private:
	typedef  bool (CVgmPlayer::*VGM_PROC_OP)(const uint8_t cmd, CReadFileStream *pStrm);
	VGM_PROC_OP m_ProcTable[256];

public:
	CVgmPlayer(CHarz80Ctrl *pHarz);
	virtual ~CVgmPlayer();
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
	bool store(CReadFileStream *pStrm, uint8_t *pDt, const int size);
	void mute();
	void setupSCC();
	void setupSCCP();
private:
	bool vgmPSG(const uint8_t cmd, CReadFileStream *pStrm);		// SN76489/SN76496
	bool vgmYM2413(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmSCC(const uint8_t cmd, CReadFileStream *pStrm);		// K051649(SCC), K052539(SCCI)
private:
	bool vgmOneOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmTwoOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmThreeOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmFourOp(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWaitNNNN(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait735(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait882(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmEnd(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmDataBlocks(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmPcmData(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait7n(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmWait8n(const uint8_t cmd, CReadFileStream *pStrm);
	bool vgmDACStreamControlWrite(const uint8_t cmd, CReadFileStream *pStrm);
};

