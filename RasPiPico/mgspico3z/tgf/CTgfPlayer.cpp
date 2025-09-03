#include "../stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include "CTgfPlayer.h"
#include "CHarz80Ctrl.h"

CTgfPlayer::CTgfPlayer(CHarz80Ctrl *pHarz)
{
	m_pStrm = GCC_NEW CReadFileStream();
	m_bFileIsOK = false;
	m_bPlay = false;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_TotalStepCount = 0;
	m_pHarz = pHarz;
	m_Volume = 15;
	m_ReqVolume[0] = m_ReqVolume[1] = 0;
	return;
}
CTgfPlayer::~CTgfPlayer()
{
	NULL_DELETE(m_pStrm);
	return;
}

bool CTgfPlayer::SetTargetFile(const char *pFname)
{
	m_pStrm->SetTargetFileName(pFname);
	auto fsize = m_pStrm->GetFileSize();
	if( 0 < fsize && (fsize%sizeof(tgf::ATOM)) == 0 ) {
		m_bFileIsOK = true;
		m_TotalStepCount = fsize / sizeof(tgf::ATOM);
	}
	else{
		m_TotalStepCount = 0;
	}
	//printf("TGF file %s, size=%d,%d\n", pFname, fsize, (fsize%sizeof(tgf::ATOM)) );

	return m_bFileIsOK;
}

int CTgfPlayer::GetTotalStepCount() const
{
	return m_TotalStepCount;
}

int CTgfPlayer::GetCurStepCount() const
{
	return m_CurStepCount;
}

int CTgfPlayer::GetRepeatCount() const
{
	return m_RepeatCount;
}

uint32_t CTgfPlayer::GetPlayTime() const
{
	return (m_VsyncCount * 166) / 10;
}

void CTgfPlayer::Start()
{
	m_bPlay = true;
	g_Mtc.Reset(0);
	m_bFirst = true;
	m_RepeatCount = 0;
	m_CurStepCount = 0;
	m_VsyncCount = 0;
	return;
}

void CTgfPlayer::Stop()
{
	m_bPlay = false;
}

/**
 * ファイルからデータを読み込む
 * @return true ディスクアクセスあり
 */
bool CTgfPlayer::FetchFile()
{
	return m_pStrm->FetchFile();
}

void CTgfPlayer::PlayLoop()
{
	if( m_ReqVolume[1] != m_ReqVolume[0] ) {
		m_ReqVolume[1] = m_ReqVolume[0];
		m_pHarz->OutputIo((harz80::ioaddr_t)0x06, (uint8_t)m_Volume);
	}

	if( !m_bFileIsOK || !m_bPlay){
		mute();
		return;
	}

	tgf::ATOM atom;
	if( !m_pStrm->Store(reinterpret_cast<uint8_t*>(&atom), sizeof(atom)) )
		return;
	switch(atom.mark)
	{
		case tgf::M_OPLL:
			m_pHarz->OutOPLL(atom.data1, atom.data2);
			break;
		case tgf::M_PSG:
			m_pHarz->OutPSG(atom.data1, atom.data2);
			break;
		case tgf::M_SCC:
			m_pHarz->OutSCC(atom.data1, atom.data2);
			break;
		case tgf::M_TC:
		{
			auto base = static_cast<tgf::timecode_t>((atom.data1<<16)|atom.data2);
			if( m_bFirst ){
				// 最初のtcは初期値として取り込む
				m_bFirst = false;
				m_padding = base;
			}else {
				if( base < m_oldBase )
					g_Mtc.Reset(0);
			}
			tgf::timecode_t tc = 0;
			while( ((m_padding+tc) < base) && m_bPlay){
				tc = static_cast<tgf::timecode_t>((((uint64_t)g_Mtc.GetTime())*1000)/16600);	// 16.6ms
			}
			m_oldBase = base;
			++m_VsyncCount;
			break;
		}
		case tgf::M_NOP:
		case tgf::M_SYSINFO:
		case tgf::M_WAIT:
		default:
			// do nothing
			break;
	}
	if( ++m_CurStepCount == m_TotalStepCount){
		m_CurStepCount = 0;
		++m_RepeatCount;
	}

	return;
}


void CTgfPlayer::SetHarzVolume(const int v)
{
	m_Volume = v;
	++m_ReqVolume[0];
	return;
}

bool CTgfPlayer::IsPlaying() const
{
	return m_bPlay;
}


void CTgfPlayer::mute()
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
	m_pHarz->OutPSG(0x08, 0x00);
	m_pHarz->OutPSG(0x09, 0x00);
	m_pHarz->OutPSG(0x0A, 0x00);

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