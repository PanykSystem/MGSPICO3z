/**
 * MGSPICO3z (RaspberryPiPico firmware)
 * Copyright (c) 2025 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

#include "for_debug.h"
#include <stdio.h>				// printf
#include <memory.h>
#include <stdint.h>
//#include <hardware/clocks.h>	 // set_sys_clock_khz()
#include <pico/rand.h> 

// for USB host
#include "tusb.h"
#include "bsp/board_api.h"

#ifdef FOR_DEBUG
#include <hardware/clocks.h>
#endif

#include "global.h"
#include "t_si5351.h"
#include "t_mmmspi.h"
#include "if_harz80.h"
#include "CHarz80Ctrl.h"
#include "harzlib.h"
#include "mgs_tools.h"
#include "sdfat.h"
#include "MusFiles.h"
#include "oled/oledssd1306.h"
#include "CMsCount.h"
#include "oled/SOUNDLOGO.h"

// -----------------------------------------------------------------------------
// VERSION
static const char *pFIRMVERSION = "v3.3.0";

// -----------------------------------------------------------------------------
static char tempWorkPath[255+1];
static const int  Z80_PAGE_SIZE = 16*1024;
static uint8_t g_WorkRam[Z80_PAGE_SIZE];
static char g_CurDir[LEN_FILE_PATH_MAX+1] = "\\";	// カレントディレクトリ

// -----------------------------------------------------------------------------
const uint32_t	ADDR_PLAYER 	= 0x4000;	// player
const uint32_t	ADDR_DRIVER 	= 0x6000;	// ドライバー(NDP以外)
const uint32_t	ADDR_DRIVER_NDP	= 0xC000;	// ドライバー(NDP)
const uint32_t	ADDR_MUSDT		= 0x8000;	// 楽曲データ(NDP以外)
const uint32_t	ADDR_MUSDT_NDP	= 0x4600;	// 楽曲データ(NDP)

// -----------------------------------------------------------------------------
struct INDICATOR
{
	static const int NUM_TRACKS = 17;
	static const int HEIGHT = 9;	// バーの高さ
	static const int FLAME_W = 7;	// 領域の幅
	static const int BAR_W = 5;		// バーの幅
	struct TRACK{
		int16_t OldCnt;
		int8_t DispLvl;
		int8_t OldDispLvl;
		uint32_t WaitC;
	};
	TRACK tk[NUM_TRACKS];
	void Setup()
	{
		for( int t = 0; t < NUM_TRACKS; ++t) {
			tk[t].DispLvl = 0;
			tk[t].OldDispLvl = HEIGHT;
			tk[t].WaitC = 0;
		}
	};
};
INDICATOR g_Indi;

// -----------------------------------------------------------------------------
struct LOOPTIMELOGWORK
{
public:
	static const int SIZE = 128;
	uint8_t *pLog;
	int WriteIndex;
	LOOPTIMELOGWORK()
	{
		WriteIndex = 0;
		pLog = GCC_NEW uint8_t[SIZE]();
	}
	void Set(const uint8_t v)
	{
		pLog[WriteIndex] = v;
		WriteIndex = (WriteIndex+1)%SIZE;
		return;
	}
};
LOOPTIMELOGWORK g_TimeLog;

// -----------------------------------------------------------------------------
struct MGSPICOWORKS
{
	bool bReadErrMGSDRV;
	bool bReadErrKINROU5;
	bool bReadErrNDP;
	int Volume;
	MGSPICOWORKS()
	{
		bReadErrMGSDRV = false;
		bReadErrKINROU5 = false;
		bReadErrNDP = false;
		Volume = 15;
		return;
	}
};
MGSPICOWORKS g_Works;


// -----------------------------------------------------------------------------
struct INITGPTABLE {
	int gpno;
	int direction;
	bool bPullup;
	int	init_value;
};
static const INITGPTABLE g_CartridgeMode_GpioTable[] = {
	{ HARZ80_GPIO_PIN_RESET_n,	GPIO_OUT,	false,	0,	},	// pull-up/down設定を行わないこと（回路でpull-downしている）
	{ MGSPICO_SW1,				GPIO_IN,	true,	0,	},
	{ MGSPICO_SW2,				GPIO_IN,	true,	0,	},
	{ MGSPICO_SW3,				GPIO_IN,	true,	0,	},
	{ GPIO_PICO_LED,			GPIO_OUT,	false,	0,	},
 	{ -1,						0,			false,	0,	},	// eot
};

static void setupGpio(const INITGPTABLE pTable[] )
{
	for( int t = 0; pTable[t].gpno != -1; ++t) {
		const int no = pTable[t].gpno;
		gpio_init(no);
		gpio_put(no, pTable[t].init_value);			// PIN方向を決める前に値をセットする
		gpio_set_dir(no, pTable[t].direction);
		if( pTable[t].bPullup)
		 	gpio_pull_up(no);
		else
		 	gpio_disable_pulls(no);
	}
	return;
}

/**
 *  タイマー割込み msカウンタ
*/
volatile static uint32_t timercnt = 0;
bool __time_critical_func(timerproc_fot_ff)(repeating_timer_t *rt)
{
	++timercnt;
	// disk_timerproc();
	return true;
}

uint32_t __time_critical_func(GetTimerCounterMS)()
{
	return timercnt;
}


static void waitForKeyRelease()
{
	uint32_t begin = GetTimerCounterMS();
	for(;;) {
		if( !gpio_get(MGSPICO_SW1) || !gpio_get(MGSPICO_SW2) || !gpio_get(MGSPICO_SW3) ){
			begin = GetTimerCounterMS();
		}
		else{
			busy_wait_ms(1);
			if( 10 <= (GetTimerCounterMS() - begin) )
				break;
		}
	}
	return;
}



static void uploadHarzBios(CHarz80Ctrl &harz)
{
	const harz80::memaddr_t topAddr = 0x0000;
	const uint8_t *pStart = _binary_harzbios_bin_start;
	const uint8_t *pEnd = _binary_harzbios_bin_end;
	const int SZ = (int)pEnd - (int)pStart;

#ifdef FOR_DEBUG
	printf("Upload HarzBIOS to %04Xh\n", topAddr);
#endif
	harz.WriteBlockMem(topAddr, pStart, SZ);
	return;
}

static void uploadPlayer(CHarz80Ctrl &harz)
{
	const uint32_t topAddr = ADDR_PLAYER;
	const uint8_t *pStart = _binary_hartplay_bin_start;
	const uint8_t *pEnd = _binary_hartplay_bin_end;
	const int SZ = (int)pEnd - (int)pStart;

#ifdef FOR_DEBUG
	printf("Upload Player(ctrldrv) to %04Xh\n", topAddr);
#endif
	harz.WriteBlockMem(topAddr, pStart, SZ);
	return;
}

static bool uploadMGSDRV(CHarz80Ctrl &harz)
{
	uint8_t *p = g_WorkRam;
	sprintf(tempWorkPath, "%s", pFN_MGSDRV);
	UINT readSize = 0;
	if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
#ifdef FOR_DEBUG
		printf( "Not found %s\n", tempWorkPath);
#endif
		return false;
	}
	else {
#ifdef FOR_DEBUG
		printf( "found %s(%d bytes)\n", tempWorkPath, readSize);
#endif
		const uint8_t *pBody;
		uint16_t bodySize;
		if( t_Mgs_GetPtrBodyAndSize(reinterpret_cast<const STR_MGSDRVCOM*>(p), &pBody, &bodySize) ) {
			const uint32_t topAddr = ADDR_DRIVER;
			for( int t = 0; t < (int)bodySize; ++t) {
				uint32_t addr = (uint32_t)(topAddr + t);
				uint8_t op = pBody[t];
				harz.WriteMem1(addr, op);
			}
			return true;
		}
	}
	return false;
}

static bool uploadKINROU5(CHarz80Ctrl &harz)
{
	uint8_t *p = g_WorkRam;
	sprintf(tempWorkPath, "%s", pFN_KINROU5);
	UINT readSize = 0;
	if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
#ifdef FOR_DEBUG
		printf( "Not found %s\n", tempWorkPath);
#endif
		return false;
	}
	else {
#ifdef FOR_DEBUG
		printf( "found %s(%d bytes)\n", tempWorkPath, readSize);
#endif
		const uint8_t *pBody = &p[7];
		uint16_t bodySize = readSize-7;
		const uint32_t topAddr = ADDR_DRIVER;
		for( int t = 0; t < (int)bodySize; ++t) {
			uint32_t addr = (uint32_t)(topAddr + t);
			uint8_t op = pBody[t];
			harz.WriteMem1(addr, op);
		}
		return true;
	}
	return false;
}

static bool uploadNDP(CHarz80Ctrl &harz)
{
	uint8_t *p = g_WorkRam;
	sprintf(tempWorkPath, "%s", pFN_NDP);
	UINT readSize = 0;
	if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
#ifdef FOR_DEBUG
		printf( "Not found %s\n", tempWorkPath);
#endif
		return false;
	}
	else {
#ifdef FOR_DEBUG
		printf( "found %s(%d bytes)\n", tempWorkPath, readSize);
#endif
		const uint8_t *pBody = &p[7];
		uint16_t bodySize = readSize-7;
		const uint32_t topAddr = ADDR_DRIVER_NDP;
		for( int t = 0; t < (int)bodySize; ++t) {
			uint32_t addr = (uint32_t)(topAddr + t);
			uint8_t op = pBody[t];
			harz.WriteMem1(addr, op);
		}
		return true;
	}
	return false;
}

static bool uploadMusicFileData(
	CHarz80Ctrl &harz,
	const char *pCurDir, const MusFiles &files, const int fileNo)
{
	auto *pFile = files.GetItemSpec(fileNo);
	sprintf(tempWorkPath, "%s\\%s", pCurDir, pFile->name);

	uint8_t *p = g_WorkRam;
	UINT readSize = 0;
	if(!sd_fatReadFileFrom(tempWorkPath, Z80_PAGE_SIZE, p, &readSize) ) {
		printf( "Not found %s\n", tempWorkPath);
	}
	else {
		printf( "found %s(%d bytes)\n", tempWorkPath, readSize);
		if( files.GetMusicType() == MgspicoSettings::MUSICDATA::NDP ) {
			const uint32_t topAddr = ADDR_MUSDT_NDP;
			for( int t = 0; t < (int)readSize-7; ++t) {
				uint32_t addr = (uint32_t)(topAddr + t);
				uint8_t op = p[t+7];
				harz.WriteMem1(addr, op);
			}
		}
		else
		{
			const uint32_t topAddr = ADDR_MUSDT;
			for( int t = 0; t < (int)readSize; ++t) {
				uint32_t addr = (uint32_t)(topAddr + t);
				uint8_t op = p[t];
				harz.WriteMem1(addr, op);
			}
		}
		return true;
	}
	return false;
}

static void setupHarz(CHarz80Ctrl harz, const MgspicoSettings stt)
{
	harz.SetBusak(0);
	harz.SetClkMode((uint8_t)stt.GetHarz80Clock());

	// ページnのRAMのスロットアドレス
	harz.OutputIo(0xA8, 0b11111111);
	harz.WriteMem1(0xF341, 0x03);	// Page.0
	harz.WriteMem1(0xF342, 0x03);	// Page.1
	harz.WriteMem1(0xF343, 0x03);	// Page.2
	harz.WriteMem1(0xF344, 0x03);	// Page.3
	// 拡張スロットの状態
	harz.WriteMem1(0xFCC1, 0x00);	// SLOT.0 + MAIN-ROMのスロットアドレス
	harz.WriteMem1(0xFCC2, 0x00);	// SLOT.1
	harz.WriteMem1(0xFCC3, 0x00);	// SLOT.2
	harz.WriteMem1(0xFCC4, 0x00);	// SLOT.3
	harz.WriteMem1(0xFFF7, harz.ReadMem1(0xFCC1));	// 0xFCC1と同一の内容を書く
	// FDCの作業領域らしいがMGSDRVがここ読み込んで何か書き換えを行っていたので、
	// DOS起動直後の値を再現しておく
	harz.WriteMem1(0xF349, 0xbb);
	harz.WriteMem1(0xF34A, 0xe7);
	//
	harz.WriteMem1(0xFF3E, 0xc9);	// H.NEWS
	harz.WriteMem1(0xFD9F, 0xc9);	// H.TIMI
	// 
	uploadHarzBios(harz);
	harz.WriteMem1(0x0200+0, 0x01);				// 0=Z80, 1=R800+ROM, 2=R800+DRAM
	//
	uploadPlayer(harz);	
	// 0x00=use MGSDRV、0x01=use KINROU5、0x04= use NDP
	harz.WriteMem1(0x4020, (uint8_t)stt.GetMusicType());

	// Sound driver の upload
	switch(stt.GetMusicType())
	{
		case MgspicoSettings::MUSICDATA::MGS:
			g_Works.bReadErrMGSDRV = !uploadMGSDRV(harz);
			break;
		case MgspicoSettings::MUSICDATA::KIN5:
			g_Works.bReadErrKINROU5 = !uploadKINROU5(harz);
			break;
		case MgspicoSettings::MUSICDATA::NDP:
			g_Works.bReadErrNDP = !uploadNDP(harz);
			break;
		default:
			break;
	}
	// 実行開始
	harz.ResetCpu();

	return;
}

// 起動画面（タイトル画面）
static void dislplayTitle(CSsd1306I2c &disp, const MgspicoSettings::MUSICDATA musType)
{
	disp.Clear();
	disp.FillBox(6, 0, 116, 33, true);
	disp.Strings8x16(3*8+0, 0*16, "MGSPICO 3z", true);
	disp.Strings8x16(8*8+0, 1*16, pFIRMVERSION, true);
	disp.Strings8x16(1*8+0, 2*16, "by harumakkin", false);
	// const char *pForDrv[] = {"for MGS", "for MuSICA", "for TGF", "for VGM", "for NDP"};
	// disp.Strings8x16(1*8+0, 3*16, pForDrv[(int)musType], false);
	disp.Strings8x16(1*8+0, 3*16, "MGS,MuSICA,NDP", false);
	disp.Present();
	return;
}


static void init()
{
	static repeating_timer_t tim;
	add_repeating_timer_ms (1/*ms*/, timerproc_fot_ff, nullptr, &tim);

	setupGpio(g_CartridgeMode_GpioTable);

#ifdef FOR_DEBUG
    // UART1の初期化方法
	// UART0を使用する場合は、
	//		stdio_init_all();を呼び出すだけでよい
	// UART1を使用する場合は、
	//		stdio_uart_init_fullを使用し、使用GPIOピンも指定する
	// 		また、CMakeLists.txt 内で、pico_enable_stdio_uart(${BinName} 1) を記述すること
	stdio_uart_init_full(
		PICO_UART1_DEV, CFG_BOARD_UART_BAUDRATE, PICO_UART1_TX, PICO_UART1_RX);
	// for first call printf.
	busy_wait_ms(1500);
#endif

#ifdef FOR_DEBUG
	printf("MGSPICO3z by harumakkin. 2025 -------------- \n");
#endif

	// clock
	t_SetupSi5351();

	// RESET信号を解除
	busy_wait_ms(100);
	gpio_put(HARZ80_GPIO_PIN_RESET_n, 1);

	// setup for filesystem
	disk_initialize(0);

	// setup for TinyUSB
	tuh_init(BOARD_TUH_RHPORT);
	if( board_init_after_tusb) {
		board_init_after_tusb();
	}

	return;
}

static void listupMusicFiles(MusFiles *pFiles)
{
	const char *pWild = nullptr;
	switch(pFiles->GetMusicType())
	{
		case MgspicoSettings::MUSICDATA::MGS:	pWild = "*.MGS";	break;
		case MgspicoSettings::MUSICDATA::KIN5:	pWild = "*.BGM";	break;
		case MgspicoSettings::MUSICDATA::NDP:	pWild = "*.NDP";	break;
		case MgspicoSettings::MUSICDATA::VGM:	pWild = "*.VGM";	break;
		case MgspicoSettings::MUSICDATA::TGF:	pWild = "*.TGF";	break;
		default:													break;
	}
	if( pWild == nullptr ) {
		pFiles->ClearList();
	}
	else{
		pFiles->ReadFileNames(pWild, g_CurDir);
	}
	return;
}


enum KEYSTATUS : int 
{
	KEYSTS_NOCHANGE	= 0,
	KEYSTS_PUSH		= 1,
	KEYSTS_RELEASE	= 2,
};
enum KEYCODE : int 
{
	KEY_APPLY	= 0,	// [●]
	KEY_DOWN	= 1,	// [▼]
	KEY_UP		= 2,	// [▲]
	KEY_NUM,
};
enum EVENT_TYPE 
{
	EVENT_NONE,
	EVENT_KEY,						// KEY-SWの押下／解放
	EVENT_KB_INPUT,					// USBキーボード
	EVENT_LOOPCT,					// 演奏ループ回数
	EVENT_ENDMUSIC,					// 演奏が終了した
	EVENT_REQ_FLIST_DOWN_CURSOR,	// ファイルリスト画面のカーソルを一行下へ移動するよう指示された
	EVENT_REQ_FLIST_UP_CURSOR,		// ファイルリスト画面のカーソルを一行上へ移動するよう指示された
	EVENT_REQ_PLAY,					// 再生開始を指示された
	EVENT_REQ_STOP,					// 再生停止を指示された
	EVENT_REQ_STT_DOWN_CURSOR,
	EVENT_REQ_STT_UP_CURSOR,
	EVENT_REQ_STT_APPLY,
	EVENT_REQ_SET_VOLUME,
	EVENT_REQ_CHDIR,
};

struct EVENT_DT
{
	EVENT_TYPE	MsgType;
	int			Action;
	int			KeyCode;
	int			Value;
	EVENT_DT(){
		MsgType = EVENT_NONE;
		Action = 0;
		KeyCode = 0;
		Value = 0;
	}
};

static const int EVENT_DT_BUFF_SIZE = 10;
class EVENT_DT_BUFF
{
private:
	EVENT_DT	Buff[EVENT_DT_BUFF_SIZE];
	int			Num;
	int			ReadIndex;
	int			WriteIndex;

public:
	EVENT_DT_BUFF() :
		Num(0), ReadIndex(0), WriteIndex(0)
	{
		return;
	}
	~EVENT_DT_BUFF()
	{
		return;
	}

private:
	bool set(EVENT_DT &dt)
	{
		if( Num == EVENT_DT_BUFF_SIZE )
			return false;
		++Num;
		Buff[WriteIndex] = dt;
		WriteIndex = (WriteIndex+1) % EVENT_DT_BUFF_SIZE;
		return true;
	}
	bool get(EVENT_DT *pDt)
	{
		if( Num == 0 )
			return false;
		--Num;
		*pDt = Buff[ReadIndex];
		ReadIndex = (ReadIndex+1) % EVENT_DT_BUFF_SIZE;
		return true;
	}

public:
	void Clear()
	{
		Num = 0;
		ReadIndex = 0;
		WriteIndex = 0;
		return;
	}
	bool Receive(EVENT_DT *pDt)
	{
		return get(pDt);
	}

	void Post(const EVENT_TYPE type)
	{
		EVENT_DT dt;
		dt.MsgType = type;
		set(dt);
		return;
	}
	void Post(const EVENT_TYPE type, const int value)
	{
		EVENT_DT dt;
		dt.MsgType = type;
		dt.Value = value;
		set(dt);
		return;
	}
	void Post(const EVENT_TYPE type, const int action, const int keyCode)
	{
		EVENT_DT dt;
		dt.MsgType = type;
		dt.Action = action;
		dt.KeyCode = keyCode;
		set(dt);
		return;
	}
};
static EVENT_DT_BUFF g_Events;

// チャタリングを除去しつつSWの状態を作成する
static void checkSw(const uint32_t nowTime, KEYSTATUS swSts[KEY_NUM])
{
	static const uint32_t swtable[KEY_NUM] = {MGSPICO_SW1, MGSPICO_SW2, MGSPICO_SW3};
	static uint32_t swStsCandi[KEY_NUM] = {1, 1, 1};
	static uint32_t timerCnts[KEY_NUM] = {0,0,0};

	for( int t = 0; t < KEY_NUM; ++t){
		const uint32_t sts = gpio_get(swtable[t]);
		KEYSTATUS ksts = KEYSTS_NOCHANGE;
		if( timerCnts[t] == 0 ) {
			if( swStsCandi[t] != sts ) {
				swStsCandi[t] = sts;
				timerCnts[t] = nowTime;
			}
		}
		else if( 15 < (nowTime - timerCnts[t]) ) {
			if( swStsCandi[t] == sts ) {
				// 確定状態
				ksts = (sts==0) ? KEYSTS_PUSH : KEYSTS_RELEASE;
			}
			swStsCandi[t] = sts;
			timerCnts[t] = 0;
		}
		swSts[t] = ksts;
	}
	return;
}

static bool displayPlayFileName(
	CSsd1306I2c &disp,
	const MusFiles &files,
	const int pageTopNo, const int seleFileNo)
{
	if( files.IsEmpty() ) {
		const static char *pNo = " No Music file";
		disp.Strings8x16(0, 1*16, pNo, true);
	}
	else {
		// ファイル名リスト
		for( int t = 0; t < 3; ++t) {
			int no = pageTopNo + t;
			auto *pF = files.GetItemSpec(no);
			if( pF == nullptr )
				continue;
			if( pF->no == 0 ){
				sprintf(tempWorkPath, "/%-*s", LEN_FILE_NAME, pF->name);
			}
			else{
				sprintf(tempWorkPath, "%03d:%-*s", pF->no, LEN_FILE_NAME, pF->name);
			}
			disp.Strings8x16(0, (1+t)*16, tempWorkPath, (seleFileNo==no)?true:false);
		}
	}
	return true;
}

static bool changeCurPos(
	const int num, int *pPageTopNo, int *pCurNo,
	const int step, const bool bRot = false)
{
	int old = *pCurNo;
	*pCurNo += step;
	if( *pCurNo <= 0 ) {
		*pCurNo = (bRot)?num:1;
	}
	else if( num < *pCurNo ) {
		*pCurNo = (bRot)?1:num;
	}

	if( *pCurNo == 0 || old == *pCurNo )
		return false;

	if( *pCurNo < *pPageTopNo )
		*pPageTopNo = *pCurNo;
	if( *pPageTopNo+2 < *pCurNo )
		*pPageTopNo = *pCurNo-2;

	return (old != *pCurNo);
}

static bool nextFile(
	MusFiles &files, int *pPageTopNo, int *pCurNo)
{
	const int numFiles = files.GetNumFiles();
	if( numFiles <= 1 )
		return false;

	bool bRetc = false;
	const int numItems = files.GetNumItems();
	for( int t = 0; t < numItems; ++t){
		changeCurPos(numFiles, pPageTopNo, pCurNo, +1, true);
		if( files.GetItemSpec(*pCurNo)->no != 0 ){
			bRetc = true;
			break;
		}
	}
	return bRetc;
}


static bool playMusic(
	CSsd1306I2c &disp, CHarz80Ctrl &harz, const char *pCurDir,
	MusFiles &files, const int selectFileNo)
{
	harz.SetCCmd(harz80::CCMD_STOP);
	// NOTE: 
	// ここで完全に曲が停止している（DRVの処理ルーチンを抜け楽曲データを参照していないといえる状態）
	// になっていることを知る方法はないか？
	// DRVがデータを参照している状態で曲データを入れ替えてしまうとハングアップしてしまう
	// 現状は、暫定手的に32msのウェイトを入れている
	busy_wait_ms(32);

	// CPUにバスアクセスを止めさせ、直接メモリに曲データを書き込む
	harz.SetBusak(0);
	busy_wait_ms(10);
	harz.OutputIo(0xA8, 0b11111111);
	bool bRetc = uploadMusicFileData(harz, pCurDir, files, selectFileNo);
	if( bRetc ){
		harz.SetBusak(1);
	}
	harz.SetCCmd(harz80::CCMD_PLAY);

	// sd アクセス後は表示器と共用しているI2Cを再設定すること
	disp.ResetI2C();
	return bRetc;
}

static void stopMusic(CHarz80Ctrl &harz)
{
	harz.SetCCmd(harz80::CCMD_STOP);
	return;
}

static void fadeoutMusic(CHarz80Ctrl &harz)
{
	harz.SetCCmd(harz80::CCMD_FADEOUT);
	return;
}

// v = 0:minimum, 15:maximum,
static void  setVolumeMusic(CHarz80Ctrl &harz, const int v)
{
	harz.SetCCmdData((uint8_t)(15-v));
	harz.SetCCmd(harz80::CCMD_VOLUME);
	return;
}

static bool downloadStatus(CHarz80Ctrl &harz, harz80::WRWKRAM *pWk)
{
	static uint8_t oldCnt = 0;
	harz.ReadStatus(reinterpret_cast<uint8_t*>(pWk), sizeof(*pWk));
	bool bUpdate = false;
	if( oldCnt != pWk->update_counter )	{
		oldCnt = pWk->update_counter;
		bUpdate = true;
#define FOR_DEBUG_PRINT_STATUS
#ifdef FOR_DEBUG_PRINT_STATUS
		static uint16_t index = 0;
		if( 160 < pWk->loop_time )
			printf("%05d %03d %03d +lt:%-3d pt:%-3d ", ++index, oldCnt, pWk->ccmd_cnt, pWk->loop_time, pWk->play_time*166/10000);
		else
			printf("%05d %03d %03d  lt:%-3d pt:%-3d ", ++index, oldCnt, pWk->ccmd_cnt, pWk->loop_time, pWk->play_time*166/10000);
		for(int t = 0; t < (47-7); t++)
			printf("%02x ", (reinterpret_cast<uint8_t*>(pWk))[t+7]);
		printf("\n");
#endif
	}
	return bUpdate;
}

static bool makeSoundIndicator(
	const uint32_t nowTime, harz80::WRWKRAM &wk,
	const MgspicoSettings::MUSICDATA musicType,
	INDICATOR *pIndi)
{
	bool bUpdated = false;
	const int num_trks = pIndi->NUM_TRACKS;
	for( int trk = 0; trk < num_trks; ++trk ) { 
		int16_t cnt = 0;
		if( musicType == MgspicoSettings::MUSICDATA::MGS ) {
			cnt = wk.info.mgs.GATETIME[trk];					// PSGx3,SCCx5,FMx9 の順のまま採用
		}
		else if( musicType == MgspicoSettings::MUSICDATA::KIN5 ) {
			cnt = wk.info.mgs.GATETIME[(trk+9)%num_trks];		// FMx9,PSGx3,SCCx5を、PSGx3,SCCx5,FMx9 の順になるように読みだす
		}
		else if( musicType == MgspicoSettings::MUSICDATA::NDP ) {
			cnt = (trk < 4) ? wk.info.mgs.GATETIME[trk] : 0;	// PSGx3,RHx1
		}
		INDICATOR::TRACK &tk = pIndi->tk[trk];
		auto oldLvl = tk.DispLvl;
		// key-on/offを判断してレベル値を決める
		if( tk.OldCnt < cnt ) {
			tk.DispLvl = pIndi->HEIGHT;	// key-on
		}
		else if( cnt <= 0 ) {
			tk.DispLvl = 0;	// key-off
		}
		tk.OldCnt = cnt;
		if( 0 < tk.DispLvl && 24 < (nowTime-tk.WaitC)) {
			// レベル値を下降させる
			tk.DispLvl--;
			tk.WaitC = nowTime;
		}
		bUpdated |= (tk.DispLvl != oldLvl)?true:false;
	}
	return bUpdated;
}

static bool drawSoundIndicator(
	CSsd1306I2c &disp, INDICATOR *p, bool bForce,
	MgspicoSettings::MUSICDATA musicType)
{
	const int numTracks =
		(musicType==MgspicoSettings::MUSICDATA::NDP) ? 4 : p->NUM_TRACKS;
	bool bDrew = false;
	for( int trk = 0; trk < numTracks; ++trk ) { 
		INDICATOR::TRACK &tk = p->tk[trk];
		int offsetX = 0;
		if( 8 <= trk )		// SCCとFMの隙間
			offsetX = 6;
		else if( 3 <= trk )	// PSGとSCCの隙間
			offsetX = 3;
		// レベル値が変化していたらバーを描画する
		if( tk.OldDispLvl != tk.DispLvl || bForce){
			tk.OldDispLvl = tk.DispLvl;
			auto x = trk * p->FLAME_W + offsetX;
			int y;
			for(y = 0; y < p->HEIGHT; ++y ){
				disp.Line(x, y, x+p->BAR_W, y, ((p->HEIGHT-y)<=tk.DispLvl)?true:false);
			}
			disp.Line(x, y, x+p->BAR_W, y, true);
			bDrew = true;
		}
	}
	// PSG3 トラックの下線
	disp.Line(0, p->HEIGHT+2, 0+p->FLAME_W*3-2, p->HEIGHT+2, true);
	if( musicType==MgspicoSettings::MUSICDATA::NDP ){
		// NDP リズム トラックの下線
		disp.Line(p->FLAME_W*3+3, p->HEIGHT+2, 0+p->FLAME_W*3+3+p->FLAME_W*1-2, p->HEIGHT+2, true);
	}
	else{
		// SCC トラックの下線
		disp.Line(p->FLAME_W*3+3, p->HEIGHT+2, 0+p->FLAME_W*3+3+p->FLAME_W*5-2, p->HEIGHT+2, true);
		// FM トラックの下線
		disp.Line(p->FLAME_W*8+6, p->HEIGHT+2, p->FLAME_W*8+6+p->FLAME_W*9-2, p->HEIGHT+2, true);
	}
	return bDrew;
}

static bool displayPlayTime(
	CSsd1306I2c &disp,
	const MusFiles &files,
	const uint32_t timev,
	const int playingFileNo,
	const bool bForce )
{
	static uint32_t oldTime = 0;
	bool bUpdated = false;
	const uint32_t maxv = (99*60+59)*1000;
	const uint32_t pt = ((maxv < timev) ? maxv : timev ) / 1000;
	if( oldTime != pt || bForce ){
		oldTime = pt;
		const int s = pt % 60;
		const int m = pt / 60;
		sprintf(tempWorkPath, "%02d:%02d", m, s);
		disp.Strings16x16(40, 5*8, tempWorkPath);
		bUpdated = true;
	}
	if( bUpdated && !files.IsEmpty() ){
		const int index = playingFileNo;
		auto *pF = files.GetItemSpec(index);
		sprintf(tempWorkPath, "%03d:%*s", pF->no, LEN_FILE_NAME, pF->name);
		disp.Strings8x16(0, 1*16, tempWorkPath, false);
	}
	return bUpdated;
}

static void displayVolume(
	CSsd1306I2c &disp,
	const int v )
{
	static int old_v = -1;
	static char temp[1+6+1+1];		// "vol.##"
	if( old_v != v ){
		old_v = v;
		sprintf(temp, " vol.%2d ", v);
	}
	// 画面横幅は16文字分。8文字をセンターに表示するには、
	// CX=4の位置から表示する
	disp.Strings8x16(8*4, 0*16, temp, true);
	return;
}

static void drawLoadLog(CSsd1306I2c &disp, const LOOPTIMELOGWORK &timlog)
{
	const int H = 53;
	for( int x = 0; x < timlog.SIZE; ++x ){
		const int index = (timlog.WriteIndex + x) % timlog.SIZE;
		const int v = timlog.pLog[index];
		const int gurgeH = (int)(v / 255.f * H);
		disp.Line(x, 63-gurgeH, x, 63, true);
	}
	const int borderY = (int)(63-(166/255.f * H));
	disp.Line(0, borderY, 127, borderY, true);
	disp.Line(0, borderY+1, 127, borderY+1, false);
	return;
}

static void  displayFps(CSsd1306I2c &disp, const int fps)
{
	sprintf(tempWorkPath, "%d", fps);
	disp.Strings8x16(4, 2*8, tempWorkPath);
	return;
}

inline void ledLamp(uint32_t sts)
{
	gpio_put(GPIO_PICO_LED, sts);
	return;
}

inline void aliveLamp()
{
	static CMsCount tim(25);
	static uint8_t lamp = 0;
	if( tim.IsTimeOut(true) ){
		++lamp;
	}
	gpio_put(GPIO_PICO_LED, (lamp&0x01));
	return;
}

// -----------------------------------------------------------------------------------------------
#define MAX_REPORT  4
static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static void process_kbd_report(const hid_keyboard_report_t *pKbRep)
{
	for( int t = 0; t < 6; ++t ){
		if( pKbRep->keycode[t] ){
			g_Events.Post(EVENT_KB_INPUT, pKbRep->keycode[t]);
		}
	}
	return;
}

static void process_mouse_report(const hid_mouse_report_t *pMusRep)
{
	// do nothing
	return;
}

static void process_generic_report(uint8_t dev_addr, uint8_t instance, const uint8_t *pReport, uint16_t len)
{
	const uint8_t rpt_count = hid_info[instance].report_count;
	tuh_hid_report_info_t *rpt_info_arr = hid_info[instance].report_info;
	tuh_hid_report_info_t *rpt_info = NULL;

	if( rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
		// Simple report without report ID as 1st byte
		rpt_info = &rpt_info_arr[0];
	}
	else {
		// Composite report, 1st byte is report ID, data starts from 2nd byte
		const uint8_t rpt_id = pReport[0];
		// Find report id in the array
		for( uint8_t i = 0; i < rpt_count; i++ ){
			if( rpt_id == rpt_info_arr[i].report_id ){
				rpt_info = &rpt_info_arr[i];
				break;
			}
		}
		pReport++;
		len--;
	}

	if( !rpt_info ) {
		//printf("Couldn't find report info !\r\n");
		return;
	}

	// For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
	// - Keyboard                     : Desktop, Keyboard
	// - Mouse                        : Desktop, Mouse
	// - Gamepad                      : Desktop, Gamepad
	// - Consumer Control (Media Key) : Consumer, Consumer Control
	// - System Control (Power key)   : Desktop, System Control
	// - Generic (vendor)             : 0xFFxx, xx
	if( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP ){
		switch( rpt_info->usage )
		{
			case HID_USAGE_DESKTOP_KEYBOARD:
				// Assume keyboard follow boot report layout
				process_kbd_report((hid_keyboard_report_t const *) pReport);
				break;
			case HID_USAGE_DESKTOP_MOUSE:
				// Assume mouse follow boot report layout
				process_mouse_report((hid_mouse_report_t const *) pReport);
				break;
			default:
				break;
		}
	}
	return;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
	static int c = 0;
	++c;

	const uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
#ifdef FOR_DEBUG
	printf("tuh_hid_mount_cb(), dev_addr=%d, instance=%d\n", dev_addr, instance);
	printf("  itf_protocol %d\n", itf_protocol);
#endif
	if( itf_protocol == HID_ITF_PROTOCOL_NONE) {
		hid_info[instance].report_count =
			tuh_hid_parse_report_descriptor(
				hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
#ifdef FOR_DEBUG
		printf("  hid_info[instance].report_count=%d\n", hid_info[instance].report_count);
#endif
	}
	else{
		// itf_protocol != HID_ITF_PROTOCOL_NONE のみ、tuh_hid_receive_report()を行う
		// HID_ITF_PROTOCOL_NONEの時にtuh_hid_receive_reportを呼び出すと、
		// 約40秒後に、"*** PANIC ***　ep 00 was already available" が起こって停止してしまう
		tuh_hid_receive_report(dev_addr, instance);
	}
#ifdef FOR_DEBUG
	printf("\n");
#endif
	return;
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
#ifdef FOR_DEBUG
	printf("tuh_hid_mount_cb(), dev_addr=%d, instance=%d\n", dev_addr, instance);
#endif
	return;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *pReport, uint16_t len)
{
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
	switch (itf_protocol)
	{
		case HID_ITF_PROTOCOL_KEYBOARD:
			process_kbd_report((const hid_keyboard_report_t *)pReport);
			break;
		case HID_ITF_PROTOCOL_MOUSE:
			process_mouse_report((const hid_mouse_report_t *)pReport);
			break;
    	default:
			process_generic_report(dev_addr, instance, pReport, len);
			break;
	}
	tuh_hid_receive_report(dev_addr, instance);
	return;
}

// -----------------------------------------------------------------------------------------------
static void task_sw(const uint32_t nowTime)
{
	static int repCnts[KEY_NUM];
	static uint32_t timerCnts[KEY_NUM] = {0,0,0,};
	KEYSTATUS sts[KEY_NUM];
	checkSw(nowTime, sts);
	for( int t = 0; t < KEY_NUM; ++t ){
		switch(sts[t])
		{
			case KEYSTS_PUSH:
			{
				g_Events.Post(EVENT_KEY, KEYSTS_PUSH, (KEYCODE)t);
				repCnts[t] = 1;
				timerCnts[t] = nowTime;
				break;
			}
			case KEYSTS_RELEASE:
			{
				g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, (KEYCODE)t);
				repCnts[t] = 0;
				timerCnts[t] = 0;
				break;
			}
			case KEYSTS_NOCHANGE:
			{
				if( t == KEY_APPLY )
					break;
				static const uint32_t waits[] = {0, 300, 50};
				auto diff = nowTime - timerCnts[t];
				auto &rep = repCnts[t];
				if( 0 < rep && waits[rep] < diff ) {
					timerCnts[t] = nowTime;
					if( rep < 2 )
						rep++;
					g_Events.Post(EVENT_KEY, KEYSTS_PUSH, (KEYCODE)t);
				}
			}
		}
	}

	// TinyUSB device task
	static CMsCount usbTim(8);
	if( usbTim.IsTimeOut(true) ){
		tuh_task();
	}
	return;
}

static void tasks(
	const uint32_t nowTime, CHarz80Ctrl &harz, harz80::WRWKRAM *pWkram,
	CMsCount *pIgnoringTim)
{
	// SWの押下状態をイベントに変換する
	task_sw(nowTime);

	// Harzから受信した再生情報や時間経過をもとにIndicator描画用の情報を作成する
	bool bUpdate = downloadStatus(harz, pWkram);
	makeSoundIndicator(nowTime, *pWkram, g_Setting.GetMusicType(), &g_Indi);
	// 
	if( bUpdate ){
		static uint8_t oldLoopCnt = 0;
		//printf( "PLAYFG=%d\n", pWkram->info.mgs.PLAYFG);
		if( pIgnoringTim->IsTimeOut() && pWkram->info.mgs.PLAYFG == 0 ){
			// 再生が終了したらEVENT_ENDMUSICを発報する（ただし再生開始直後から100msが経過するまでは何もしない）
			pIgnoringTim->Cancel();
			g_Events.Post(EVENT_ENDMUSIC);
		}
		if( oldLoopCnt != pWkram->info.mgs.LOOPCT ){
			// 再生回数が変化したらEVENT_LOOPCTを発報する
			oldLoopCnt = pWkram->info.mgs.LOOPCT;
			g_Events.Post(EVENT_LOOPCT, (int)pWkram->info.mgs.LOOPCT);
		}
		// CPU負荷を可視化するためのメインループ一周の時間を記録する
		g_TimeLog.Set(pWkram->loop_time);
	}
	return;
}

static bool getRandPlayNo(const MusFiles &files, int *pNewNo)
{
	const int num = files.GetNumItems();
	for(int t = 0; t < num; ++t){
		const int cno = (get_rand_32() % num) + 1;
		const auto *pF = files.GetItemSpec(cno);
		if( pF != nullptr && pF->no != 0 ){
			*pNewNo = cno;
			return true;
		}
	}
	return false;
}

static void dispPlayer(CSsd1306I2c &disp, CHarz80Ctrl &harz, MusFiles &files)
{
	waitForKeyRelease();
	g_Indi.Setup();
	bool bRandomPlay = g_Setting.GetRandomPlay();
	bool bDispLoadGraph = false;

	int topFileNo = 1;
	int curNo = 1;
	int frameCnt = 0;
	int fps = 0;

	harz80::WRWKRAM wkram;
	int playNo = 0;	
	CMsCount updDispTim(16/*ms*/);
	CMsCount fpsTim(1000/*ms*/);
	CMsCount volumeDispTim;

	static const uint32_t DISPLIST_TIME = 1000; // ms
	CMsCount dispListTim;
	static const uint32_t IGNORING_TIME = 100; // ms
	CMsCount ignoringTim;

	enum SHIFTKEY {SHIFT_NONE, SHIFT_APPLY, SHIFT_DOWN, SHIFT_UP};
	SHIFTKEY shift = SHIFT_NONE;

	if( bRandomPlay)
		getRandPlayNo(files, &curNo);

	// 先頭のファイルの自動再生
	if( g_Setting.GetAutoRun() ){
		int topNo = (bRandomPlay) ? curNo : files.GetTopFileNo();
		if( topNo != 0 ){
			curNo = topNo;
			g_Events.Post(EVENT_REQ_PLAY, curNo);
		}
	}



	ledLamp(0);

	for(;;) {
		// 動作LED
		aliveLamp();
		//
		const uint32_t nowTime = GetTimerCounterMS();
		tasks(nowTime, harz, &wkram, &ignoringTim);
		EVENT_DT dt;
		if( g_Events.Receive(&dt) ){
			switch(dt.MsgType)
			{
				case EVENT_KB_INPUT:
				{
					switch((uint8_t)dt.Value)
					{
//						case 30:	/*KEY 左*/	g_Works.Volume += (g_Works.Volume<15)?1:0;		break;
//						case 31:	/*KEY 中*/	g_Works.Volume += (0<g_Works.Volume)?-1:0;		break;
						case 30:	/*KEY 左*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_UP);		g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_UP);		break;
						case 31:	/*KEY 中*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_DOWN);	g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_DOWN);		break;
						case 32:	/*KEY 右*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_APPLY);	g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_APPLY);	break;
//						case 33:	/*回転 左*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_DOWN);	g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_DOWN);		break;
// /*ロータリー押下*/	case 34:	/*回転 押*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_APPLY);	g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_APPLY);	break;
//						case 35:	/*回転 右*/	g_Events.Post(EVENT_KEY, KEYSTS_PUSH, KEY_UP);		g_Events.Post(EVENT_KEY, KEYSTS_RELEASE, KEY_UP);		break;
						case 33:	/*回転 反時計*/	g_Works.Volume += (0<g_Works.Volume)?-1:0;		break;
						case 35:	/*回転 時計*/	g_Works.Volume += (g_Works.Volume<15)?1:0;		break;
					}
					break;
				}
				// キー操作が行われた
				case EVENT_KEY:
				{
					switch(dt.KeyCode)
					{
						case KEY_APPLY:
						{
							if( dt.Action == KEYSTS_PUSH )
								shift = SHIFT_APPLY;
							else {
								if( shift == SHIFT_APPLY ){
									if( playNo != 0 && curNo==playNo ){
										g_Events.Post(EVENT_REQ_STOP);
									}
									else {
										const char *pDir = files.GetDirName(curNo);
										if( pDir != nullptr ){
											g_Events.Post(EVENT_REQ_CHDIR, curNo);
										}
										else {
											g_Events.Post(EVENT_REQ_PLAY, curNo);
										}
									}
								}
								shift = SHIFT_NONE;
							}
							break;
						}
						case KEY_DOWN:
						{
							if( dt.Action == KEYSTS_PUSH ){
								if( shift == SHIFT_APPLY || shift == SHIFT_DOWN){
									shift = SHIFT_DOWN;
									bRandomPlay = !bRandomPlay;
								}
								else {
									g_Events.Post(EVENT_REQ_FLIST_DOWN_CURSOR);
								}
							}
							break;
						}
						case KEY_UP:
						{
							if( dt.Action == KEYSTS_PUSH ){
								if( shift == SHIFT_APPLY || shift == SHIFT_UP ){
									shift = SHIFT_UP;
									bDispLoadGraph = !bDispLoadGraph;
								}
								else {
									g_Events.Post(EVENT_REQ_FLIST_UP_CURSOR);
								}
							}
							break;
						}
						default:
							break;
					}
					break;
				}
				// 再生開始を指示されたので再生を実行する（dt.Value=曲番号）
				case EVENT_REQ_PLAY:
				{
					const int no = dt.Value;
					const char *pDir = files.GetDirName(no);
					if( pDir != nullptr )
						break;
					if( playMusic(disp, harz, g_CurDir, files, no) ){
						ignoringTim.Reset(IGNORING_TIME);
						playNo = dt.Value;
						dispListTim.Cancel();
					}
					break;
				}
				// 再生停止を指示されたので停止を実行する
				case EVENT_REQ_STOP:
				{
					stopMusic(harz);
					playNo = 0;
					break;
				}
				// カーソルを一行下へ移動するよう指示された
				case EVENT_REQ_FLIST_DOWN_CURSOR:
				{
					changeCurPos(files.GetNumItems(), &topFileNo, &curNo, +1, true);
					// ファイルリストを1秒間は表示する
					dispListTim.Reset(DISPLIST_TIME);
					break;
				}
				// カーソルを一行上へ移動するよう指示された
				case EVENT_REQ_FLIST_UP_CURSOR:
				{
					changeCurPos(files.GetNumItems(), &topFileNo, &curNo, -1, true);
					// ファイルリストを1秒間は表示する
					dispListTim.Reset(DISPLIST_TIME);
					break;
				}
				// 下した（dt.Valueに回数が隠されている。演奏開始直後は0）
				case EVENT_LOOPCT:
				{
					const int cnt = g_Setting.GetLoopCnt();
					// cnt回ループしていたら、４秒フェードアウト後に演奏を停止させる
					if( cnt != 0 && dt.Value == cnt ){
						fadeoutMusic(harz);
					}
					break;
				}
				// 演奏が終了した
				case EVENT_ENDMUSIC:
				{
					if( playNo == 0 )
						break;
					const int cnt = g_Setting.GetLoopCnt();
					if( cnt != 0 ){
						// 次の曲を再生開始
						if( bRandomPlay ){
							// 次の曲を乱数で決める
							getRandPlayNo(files, &playNo);
						}
						else {
							nextFile(files, &topFileNo, &playNo);
						}
					}
					if( playMusic(disp, harz, g_CurDir, files, playNo) ){
						ignoringTim.Reset(IGNORING_TIME);
						curNo = playNo;
						dispListTim.Cancel();
					}
					break;
				}
				case EVENT_REQ_SET_VOLUME:
				{
					setVolumeMusic(harz, g_Works.Volume);
					break;
				}
				case EVENT_REQ_CHDIR:
				{
					// ディレクトリを変更し、ファイルリストを更新する
					// NOTE: 指定ディレクトリが無い場合は、ルートディレクトリに移動する
					const char *pDirName = files.GetDirName(dt.Value);
					if( pDirName != nullptr ){
						// 変更先のディレク織パスを作成する -> g_CurDir
						if( strcmp(pDirName, "..") == 0 ){
							MusFiles::DeleteTermPath(g_CurDir);
						}
						else{
							sprintf(tempWorkPath, "%s\\%s", g_CurDir, pDirName);
							strcpy(g_CurDir, tempWorkPath);
						}
						// パスが存在しない場合、ルートディレクトリとする
						if( !MusFiles::IsExistDir(g_CurDir) )
							strcpy(g_CurDir, "\\");
						// ファイルリストを更新する
						listupMusicFiles(&files);
						topFileNo = 1;
						curNo = 1;
						dispListTim.Reset(DISPLIST_TIME);
						// sd アクセス後は表示器と共用しているI2Cを再設定すること
						disp.ResetI2C();
					}
					break;
				}
				default:
				{
					break;
				}
			}
		}

		// 再生していないとき、ファイルリスト画面を表示する
		// あるいは、一定時間、ファイルリスト画面を表示する
		if( updDispTim.IsTimeOut(true) ){
			disp.Clear();
			drawSoundIndicator(disp, &g_Indi, true, g_Setting.GetMusicType());

			bool bDispList = (playNo == 0) || dispListTim.IsMidway();
			if( bDispList ){
				displayPlayFileName(disp, files, topFileNo, curNo);
			}
			else if( bDispLoadGraph ){
				drawLoadLog(disp, g_TimeLog);
				displayFps(disp, fps);
			}
			else {
				// 再生時間の表示
				const uint32_t timev = (wkram.play_time*166) / 10;	// ms
				displayPlayTime(disp, files, timev, playNo, true);
				// 再生マークの表示
				if( playNo != 0 ){
					disp.Bitmap(24, 5*8, PLAY_8x16_BITMAP, PLAY_LX, PLAY_LY);
				}
				// シャッフル再生
				if( bRandomPlay ){
					disp.Bitmap(4, 5*8, RANDOM_13x16_BITMAP, RANDOM_LX,RANDOM_LY);
				}
			}

			// 音量表示
			static int oldVolume = g_Works.Volume;
			if( oldVolume != g_Works.Volume ){
				oldVolume = g_Works.Volume;
				g_Events.Post(EVENT_REQ_SET_VOLUME);
				volumeDispTim.Reset(1000/*ms*/);		// 1秒間、音量値を表示する
			}
			if( volumeDispTim.IsMidway() )
				displayVolume(disp, g_Works.Volume);
			if( volumeDispTim.IsTimeOut() )
				volumeDispTim.Cancel();

			// 描画内容をOLEDへ転送する
			disp.Present();
			++frameCnt;
		}

		if( fpsTim.IsTimeOut(true) ){
			fps = frameCnt;
			frameCnt = 0;
		}
	}
	return;
}


// 画面内に一度に表示できるメニュー項目数数
const int NUM_DISP_MENUITEMS = 3;

static void dsipSttMenu(
	CSsd1306I2c &disp, const int y, const MgspicoSettings::ITEM &item,
	const int seleChoice, const bool bCursor)
{
	disp.Strings8x16(0, y, item.pName);
	disp.Strings8x16(7*10, y, item.pChoices[seleChoice], bCursor);
	if( bCursor )
		disp.Line(0, y+13, 80, y+13, true);
	return;
}

static void dispSettingMenuTitle(CSsd1306I2c &disp)
{
	disp.Clear();
	disp.Strings8x16(2, 0, "SETTINGS");
	disp.Box(0, 0, 2+8*8, 13, true);
	return;
}

static void dispSettingMenu(
	CSsd1306I2c &disp, const int topNo, const int seleNo,
	const MgspicoSettings &stt)
{
	for( int t = 0; t < NUM_DISP_MENUITEMS; ++t) {
		const int index = topNo-1+t;
		auto &item = *stt.GetItem(index);
		dsipSttMenu(disp, 16+16*t, item, stt.GetChioce(index), (index==(seleNo-1)));
	}
	return;
}

static void dispSettingMenu(
	CSsd1306I2c &disp,
	const MgspicoSettings &stt)
{
	int topNo = 1;
	int seleNo = 1;
	dispSettingMenuTitle(disp);
	disp.Present();
	waitForKeyRelease();

	dispSettingMenu(disp, topNo, seleNo, stt);
	disp.Present();

	CMsCount updDispTim(16/*ms*/);
	CMsCount pressApllyTim;

	bool bExit = false;
	while(!bExit) {
		aliveLamp();
		const uint32_t nowTime = GetTimerCounterMS();
		// SWの押下状態をイベントに変換する
		task_sw(nowTime);
		EVENT_DT dt;
		if( g_Events.Receive(&dt) ){
			switch(dt.MsgType)
			{
				// ------------------------------------------------------------
				// キー操作が行われた
				case EVENT_KEY:
				{
					switch(dt.KeyCode)
					{
						case KEY_APPLY:
							if( dt.Action == KEYSTS_PUSH ){
								pressApllyTim.Reset(700);
							}
							else {
								if( pressApllyTim.IsTimeOut() ){
									bExit = true;
								}
								else{
									pressApllyTim.Cancel();
									g_Events.Post(EVENT_REQ_STT_APPLY);
								}
								break;
							}
							break;
						case KEY_DOWN:
							if( dt.Action == KEYSTS_PUSH ){
								g_Events.Post(EVENT_REQ_STT_DOWN_CURSOR);
							}
							break;
						case KEY_UP:
							if( dt.Action == KEYSTS_PUSH ){
								g_Events.Post(EVENT_REQ_STT_UP_CURSOR);
							}
							break;
						default:
							break;
					}
					break;
				}
				// ------------------------------------------------------------
				case EVENT_REQ_STT_APPLY:
				{
					int index = seleNo-1;
					auto &item = *g_Setting.GetItem(index);
					int n = g_Setting.GetChioce(index);
					const int temp = n;
					++n;
					if( item.num <= n )
						n = 0;
					if( temp != n ) {
						gpio_put(GPIO_PICO_LED, 1);
						g_Setting.SetChioce(index, n);
						g_Setting.WriteSettingTo(pFN_MGSPICO_DAT);
						busy_wait_ms(200);
						disp.Setup();
						gpio_put(GPIO_PICO_LED, 0);
					}
					break;
				}
				case EVENT_REQ_STT_DOWN_CURSOR:
				{
					changeCurPos(g_Setting.NUM_MENUITEMS, &topNo, &seleNo, +1);
					break;
				}
				case EVENT_REQ_STT_UP_CURSOR:
				{
					changeCurPos(g_Setting.NUM_MENUITEMS, &topNo, &seleNo, -1);
					break;
				}
				default:
				{
					break;
				}
			}
		}

		if( updDispTim.IsTimeOut(true) ){
			disp.Clear();
			// pressApllyTimタイムアウトしたら画面が真っ暗になるようにする
			if( !pressApllyTim.IsTimeOut() ){
				dispSettingMenuTitle(disp);
				dispSettingMenu(disp, topNo, seleNo, stt);
			}
			disp.Present();
		}
	}
	return;
}

static void displayNotFound(CSsd1306I2c &oled, const char *pPathName)
{
	oled.Clear();
	oled.Strings8x16(0*8, 1*16, "Not found", false);
	oled.Strings8x16(0*8, 2*16, pPathName, false);
	oled.Present();
	#ifdef FOR_DEBUG
		printf("not found %s\n", pPathName);
	#endif
	return;
}

static void dispError(CSsd1306I2c &disp, MGSPICOWORKS &wk)
{
	if( g_Works.bReadErrMGSDRV )
		displayNotFound(disp, pFN_MGSDRV);
	else if( g_Works.bReadErrKINROU5 )
		displayNotFound(disp, pFN_KINROU5);
	else if( g_Works.bReadErrNDP )
		displayNotFound(disp, pFN_NDP);
	return;
}

/**
 * エントリ
 */
int main()
{
	auto *pDisp = GCC_NEW CSsd1306I2c();
	auto *pHarz = GCC_NEW CHarz80Ctrl();
	auto *pFiles = GCC_NEW MusFiles();

	init();

	// OLEDディスプレイの初期化とタイトル画面の表示
	pDisp->Setup();
	dislplayTitle(*pDisp, g_Setting.GetMusicType());

	bool bSettingMenu = !gpio_get(MGSPICO_SW1);
	busy_wait_ms(1000);		// title 画面を表示する時間
	bSettingMenu &= !gpio_get(MGSPICO_SW1);

	// 動作設定ファイルを読み込む
	g_Setting.ReadSettingFrom(pFN_MGSPICO_DAT);
	if( bSettingMenu ){
		pDisp->Setup();
		dispSettingMenu(*pDisp, g_Setting);
	}

	// HARZ80制御オブジェクト
	pHarz->Setup();
	setupHarz(*pHarz, g_Setting);

	// 音源ドライバの種類
	pFiles->SetMusicType(g_Setting.GetMusicType());

	// 
	if( g_Works.bReadErrMGSDRV || g_Works.bReadErrKINROU5 || g_Works.bReadErrNDP ){
		pDisp->Setup();
		dispError(*pDisp, g_Works);
	}
	else{
		listupMusicFiles(pFiles);
		pDisp->Setup();
		dispPlayer(*pDisp, *pHarz, *pFiles);
	}
	return 0;
}


