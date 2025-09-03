#include "../stdafx.h"
#include <stdio.h>		// printf
#include <memory.h>
#include <string.h>
#include "for_debug.h"
#include "CVgmPlayer.h"
#include "CHarz80Ctrl.h"

CVgmPlayer::CVgmPlayer(CHarz80Ctrl *pHarz)
{
	m_pStrm = GCC_NEW CReadFileStream();
	m_bKick = false;
	m_bFileIsOK = false;
	m_bPlay = false;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_TotalStepCount = 0;
	m_PlayTime = 0;
	m_pHarz = pHarz;
	m_Volume = 15;
	m_ReqVolume[0] = m_ReqVolume[1] = 0;
	m_CompletedNotes = 0;

	for(int t = 0x00; t < 0x2f; ++t)
		m_ProcTable[t] = nullptr;

	for(int t = 0x30; t < 0x3f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmOneOp;

	m_ProcTable[0x40] = &CVgmPlayer::vgmTwoOp;

	for(int t = 0x41; t < 0x4e; ++t)
		m_ProcTable[t] = nullptr;

	m_ProcTable[0x4f] = &CVgmPlayer::vgmTwoOp;
	m_ProcTable[0x50] = &CVgmPlayer::vgmPSG;			// PSG ?
	m_ProcTable[0x51] = &CVgmPlayer::vgmYM2413;			// OPLL

	for(int t = 0x52; t < 0x5f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmTwoOp;

	m_ProcTable[0x61] = &CVgmPlayer::vgmWaitNNNN;
	m_ProcTable[0x62] = &CVgmPlayer::vgmWait735;
	m_ProcTable[0x63] = &CVgmPlayer::vgmWait882;
	m_ProcTable[0x66] = &CVgmPlayer::vgmEnd;
	m_ProcTable[0x67] = &CVgmPlayer::vgmDataBlocks;
	m_ProcTable[0x68] = &CVgmPlayer::vgmPcmData;

	for(int t = 0x70; t <= 0x7f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmWait7n;

	for(int t = 0x80; t <= 0x8f; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmWait8n;

	for(int t = 0x90; t <= 0x95; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmDACStreamControlWrite;

	m_ProcTable[0xa0] = &CVgmPlayer::vgmPSG;			// PSG

	for(int t = 0xb0; t <= 0xbf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmTwoOp;

	for(int t = 0xc0; t <= 0xc8; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	for(int t = 0xd0; t <= 0xd1; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	m_ProcTable[0xd2] = &CVgmPlayer::vgmSCC;			// SCC

	for(int t = 0xd3; t <= 0xd6; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;

	m_ProcTable[0xe0] = &CVgmPlayer::vgmFourOp;
	m_ProcTable[0xe1] = &CVgmPlayer::vgmFourOp;

	// RESERVED AREA
	for(int t = 0xc9; t <= 0xcf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;
	for(int t = 0xd7; t <= 0xdf; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmThreeOp;
	for(int t = 0xe2; t <= 0xff; ++t)
		m_ProcTable[t] = &CVgmPlayer::vgmFourOp;

	//
	return;
}
CVgmPlayer::~CVgmPlayer()
{
	NULL_DELETE(m_pStrm);
	return;
}

bool CVgmPlayer::SetTargetFile(const char *pFname)
{
	m_TotalStepCount = 0;
	const UINT HEADSIZE = sizeof(m_VgmHeader);
	UINT readSize;
	if( !sd_fatReadFileFrom(pFname, (int)HEADSIZE, (uint8_t*)&m_VgmHeader, &readSize) )
		return false;
	if( readSize < 64 )	
	 	return false;
	const uint8_t IDENT[4] = {0x56,0x67,0x6d,0x20,};			// "Vgm "
	if( memcmp(m_VgmHeader.ident, IDENT, sizeof(IDENT)) != 0 )
		return false;
	//
	const uint32_t dataOffset =
		(m_VgmHeader.Version < 0x150)
		? (0x40)
		: (0x34 + m_VgmHeader.VGM_data_offset);
	m_pStrm->SetTargetFileName(pFname, dataOffset);
	m_bFileIsOK = true;
	m_TotalStepCount = m_VgmHeader.Total_Number_samples;

	// // setup SCC
	// if( 0x161 <= m_VgmHeader.Version && m_VgmHeader.K051649_clock != 0) {
	// 	if( m_VgmHeader.K051649_clock & 0x80000000 )
	// 		setupSCCP();	// K052539(SCC-I,SCC+)
	// 	else 
	// 		setupSCC();		// K051649
	// }

	return m_bFileIsOK;
}

int CVgmPlayer::GetTotalStepCount() const
{
	return m_TotalStepCount;
}

int CVgmPlayer::GetCurStepCount() const
{
	return m_CurStepCount;
}

int CVgmPlayer::GetRepeatCount() const
{
	return m_RepeatCount;
}

uint32_t CVgmPlayer::GetPlayTime() const
{
	return (uint32_t)(m_PlayTime / 1000);	// ms
}

void CVgmPlayer::Start()
{
	m_bKick = true;
	m_bPlay = true;
	m_bFirst = true;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_WaitSamples = 0;
	m_PlayTime = 0;
	m_CompletedNotes = 0;
	return;
}

void CVgmPlayer::Stop()
{
	m_bPlay = false;
	m_bKick = false;
	return;
}

// @return true ディスクアクセスあり
bool CVgmPlayer::FetchFile()
{
	return m_pStrm->FetchFile();
}

void CVgmPlayer::PlayLoop()
{
	if( m_ReqVolume[1] != m_ReqVolume[0] ) {
		m_ReqVolume[1] = m_ReqVolume[0];
		m_pHarz->OutputIo((harz80::ioaddr_t)0x06, (uint8_t)m_Volume);
	}

	if( !m_bFileIsOK || !m_bPlay){
		mute();
		return;
	}
	if( m_bKick ){
		if( m_pStrm->GetLeftInSegment() == 0)
			return;
		m_bKick = false;
		m_StartTime = time_us_64();
	}

	uint8_t cmd;
	if( !store(m_pStrm, &cmd, sizeof(cmd)) )
		return;

#ifdef FOR_DEBUG
	printf("cmd:%02x\n", cmd);
#endif

	// comannd
	VGM_PROC_OP pProc = m_ProcTable[cmd];
	(this->*pProc)(cmd, m_pStrm);

	// wait
	static uint64_t oldSam = 0;
	const uint64_t w = (uint64_t)(m_WaitSamples*23);
	volatile uint64_t nowSam = m_StartTime + w;
	volatile uint32_t defTime = (uint32_t)(nowSam - oldSam);
	if( 1 < defTime ){
		busy_wait_until(nowSam);
		oldSam = nowSam;
		m_PlayTime += defTime;
	}

	m_CurStepCount = m_WaitSamples;
	return;
}

void CVgmPlayer::SetHarzVolume(const int v)
{
	m_Volume = v;
	++m_ReqVolume[0];
	return;
}

bool CVgmPlayer::IsPlaying() const
{
	return m_bPlay;
}

bool CVgmPlayer::store(CReadFileStream *pStrm, uint8_t *pDt, const int size)
{
	const bool b = pStrm->Store(pDt, size);
	m_CompletedNotes += size;
	return b;
}


void CVgmPlayer::mute()
{
	// 音量を0にする
	//		VOLのレジスタを0x0fにするだけでは完全に音は消えない
	//		F-Numを0にする必要あり
	// OPLL
	m_pHarz->OutOPLL(0x0E, 0x00);
	for(int i = 0x10; i <= 0x28; ++i)
		m_pHarz->OutOPLL((uint8_t)i, 0x00);
	for(int i = 0x30; i <= 0x38; ++i)
		m_pHarz->OutOPLL((uint8_t)i, 0x0F);

	// PSG
	for(int i = 0x00; i <= 0x06; ++i)
		m_pHarz->OutPSG((uint8_t)i, 0x00);
	m_pHarz->OutPSG(0x07, 0x3F);
	for(int i = 0x08; i <= 0x0A; ++i)
		m_pHarz->OutPSG((uint8_t)i, 0x00);
	m_pHarz->OutPSG(0x0B, 0x00);
	m_pHarz->OutPSG(0x0C, 0x00);
	m_pHarz->OutPSG(0x0D, 0x00);

	// // SCC+
	// m_pHarz->OutSCC(0xbffe, 0x20);
	// m_pHarz->OutSCC(0xb000, 0x80);
	// // 音量 vol=0
	// for( harz80::memaddr_t addr = 0xb8aa; addr <= 0xb8ae; ++addr)
	// 	m_pHarz->OutSCC(addr, 0x00);
	// // チャンネルイネーブルビット
	// m_pHarz->OutSCC(0xb8af, 0x00);	// turn off, CH.A-E
	// // wave table data 再生速度
	// for( harz80::memaddr_t addr = 0xb8a0; addr <= 0xb8a8; ++addr)
	// 	m_pHarz->OutSCC(addr, 0x00);
	// // wave table data A,B,C,D/E
	// for( harz80::memaddr_t addr = 0xb800; addr <= 0xb8bf; ++addr)
	// 	m_pHarz->OutSCC(addr, 0x00);

	// SCC
	m_pHarz->OutSCC(0xbffe, 0x00);
	m_pHarz->OutSCC(0x9000, 0x3f);
	// 音量 vol=0
	for( harz80::memaddr_t addr = 0x988a; addr <= 0x988e; ++addr)
		m_pHarz->OutSCC(addr, 0x00);
	// チャンネルイネーブルビット
	m_pHarz->OutSCC(0x988f, 0x00);	// turn off, CH.A-E
	// wave table data 再生速度
	for( harz80::memaddr_t addr = 0x9880; addr <= 0x9889; ++addr)
		m_pHarz->OutSCC(addr, 0x00);
	// wave table data A,B,C,D/E
	for( harz80::memaddr_t addr = 0x9800; addr <= 0x987f; ++addr)
		m_pHarz->OutSCC(addr, 0x00);
	return;
}

void CVgmPlayer::setupSCC()
{
	// SCC動作
	m_pHarz->OutSCC(0xBFFE, 0x00);
	m_pHarz->OutSCC(0x9000, 0x3F);
	return;
}

void CVgmPlayer::setupSCCP()
{
	// SCC+動作
	m_pHarz->OutSCC(0xBFFE, 0x20);
	m_pHarz->OutSCC(0xB000, 0x80);
	return;
}

bool CVgmPlayer::vgmPSG(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint8_t dt[2];
	store(pStrm, dt, sizeof(dt));
	m_pHarz->OutPSG(dt[0], dt[1]);
	return true;
}

bool CVgmPlayer::vgmYM2413(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint8_t dt[2];
	store(pStrm, dt, sizeof(dt));
	m_pHarz->OutOPLL(dt[0], dt[1]);
	return true;
}

bool CVgmPlayer::vgmSCC(const uint8_t cmd, CReadFileStream *pStrm)
{
	const harz80::memaddr_t base[] = {
		0x9800,	// waveform
		0x9880,	// frequency
		0x988A,	// volume
		0x988F,	// key on/off
		0x9800,	// waveform(SCC+)
		0x98C0,	// test register
	};
	uint8_t dt[3];
	store(pStrm, dt, sizeof(dt));
	const harz80::memaddr_t addr = base[dt[0]] + dt[1];
	m_pHarz->OutSCC(addr, dt[2]);
	return true;
}

bool CVgmPlayer::vgmOneOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	store(pStrm, nullptr, 1);
	return true;
}

bool CVgmPlayer::vgmTwoOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	store(pStrm, nullptr, 2);
	return true;
}

bool CVgmPlayer::vgmThreeOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	store(pStrm, nullptr, 3);
	return true;
}

bool CVgmPlayer::vgmFourOp(const uint8_t cmd, CReadFileStream *pStrm)
{
	store(pStrm, nullptr, 4);
	return true;
}

bool CVgmPlayer::vgmWaitNNNN(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint16_t w;
	store(pStrm, reinterpret_cast<uint8_t *>(&w), 2);
	m_WaitSamples += w;
	return true;
}

bool CVgmPlayer::vgmWait735(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += 735;
	return true;
}

bool CVgmPlayer::vgmWait882(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += 882;
	return true;
}
bool CVgmPlayer::vgmEnd(const uint8_t cmd, CReadFileStream *pStrm)
{
	const uint32_t waste = pStrm->GetEffectiveFileSize() - m_CompletedNotes;
	store(pStrm, nullptr, waste);	// endコード以降にデータがある場合はそれを読み捨てる
	m_CompletedNotes = 0;
	m_WaitSamples = 0;
	++m_RepeatCount;
	m_StartTime = time_us_64();
	return true;
}

bool CVgmPlayer::vgmDataBlocks(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint32_t dataSize;
	store(pStrm, nullptr, 1);			// 0x6?
	store(pStrm, nullptr, 1);			// data type
	store(pStrm, reinterpret_cast<uint8_t *>(&dataSize), 4);
	store(pStrm, nullptr, dataSize);	// 全て読み捨て
	return true;
}

bool CVgmPlayer::vgmPcmData(const uint8_t cmd, CReadFileStream *pStrm)
{
	uint32_t dataSize;
	store(pStrm, nullptr, 1);			// 0x66
	store(pStrm, nullptr, 1);			// chip type
	store(pStrm, nullptr, 3);			// offset 1
	store(pStrm, nullptr, 3);			// offset 2
	store(pStrm, reinterpret_cast<uint8_t *>(&dataSize), 3);
	dataSize >>= 8;
	store(pStrm, nullptr, dataSize);	// 全て読み捨て
	return true;
}

bool CVgmPlayer::vgmWait7n(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += cmd - 0x70 + 1;
	return true;
}

bool CVgmPlayer::vgmWait8n(const uint8_t cmd, CReadFileStream *pStrm)
{
	m_WaitSamples += cmd - 0x80 + 0;	// +1ではない。
	return true;
}

bool CVgmPlayer::vgmDACStreamControlWrite(const uint8_t cmd, CReadFileStream *pStrm)
{
	store(pStrm, nullptr, 4);
	return true;
}
