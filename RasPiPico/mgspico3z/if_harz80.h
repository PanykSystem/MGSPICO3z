#pragma once
#include <stdint.h>

namespace harz80 {

enum TXCMD : uint8_t
{
	TXCMD_NOP			= 0x00,
	//
	TXCMD_PSRAM_WR_8	= 0x01,
	TXCMD_PSRAM_RD_8	= 0x02,
	//
	TXCMD_Z80MEM_WR_1	= 0x03,		// 1byte
	TXCMD_Z80MEM_RD_1	= 0x04,
	TXCMD_Z80MEM_WR_2	= 0x05,		// 2byte(WORD)
	TXCMD_Z80MEM_RD_2	= 0x06,
	TXCMD_Z80MEM_WR_4	= 0x07,		// 4byte(DWORD)
	TXCMD_Z80MEM_RD_4	= 0x08,
	//
	TXCMD_Z80IO_WR		= 0x09,
	TXCMD_Z80IO_RD		= 0x0a,
	//
	TXCMD_HARZ_RESET	= 0x10,
	TXCMD_HARZ_RUN		= 0x11,
	TXCMD_HARZ_STOP		= 0x12,
	TXCMD_padding1		= 0x13,		// (空き)
	//
	TXCMD_HARZ_GETSTS	= 0x14,		// 動作状態を返す
	TXCMD_HARZ_SETCMD	= 0x15,
	TXCMD_HARZ_CLKMODE	= 0x16,		// Z80速度 0=3.58MHz, 1=7.16Mhz
};

enum CCMD : uint8_t
{
	CCMD_NOP			= 0x00,
	CCMD_PLAY			= 0x01,
	CCMD_STOP			= 0x02,
	CCMD_FADEOUT		= 0x03,
};

#pragma pack(push, 1)

struct MGSINFO
{
	uint8_t	FM_SLT;			// MSX-MUSICのスロット番号、無しならFFH
	uint8_t	SC_SLT;			// SCC音源のスロット番号、無しならFFH
	uint8_t MAXCHN;			// 15 or 17
	uint8_t LOOPCT;			// ループ回数(演奏開始直後は0)
	uint8_t	PLAYFG;			// 現在演奏中のトラック数
	int16_t	GATETIME[17];	// GATEタイムカウンタ(signed int16)
};

// WRWKRAMの構造 47 bytes
struct WRWKRAM
{
 	uint8_t	update_counter;		// 更新される度にインクリメントされる
	uint8_t ccmd_cnt;			// CCMDを受領した回数。リセット直後は0であるが、CCMD受領のたびにインクリメントされ255の次は0ではなく1に循環する
 	uint8_t	loop_time;			// メインループの所要時間[100us]単位。ただし上限25.5[ms]
	uint32_t play_time;			// 再生経過時間 16.6ms毎にインクリメントされる32ビット整数、再生していないときは0
	uint8_t type;				// 0:MGS
	union {
		struct MGSINFO	mgs;
	} info;
};


#pragma pack(pop)


typedef uint16_t memaddr_t;
typedef uint8_t ioaddr_t;


} /* namespace harz80 */
