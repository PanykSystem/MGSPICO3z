#pragma once
#include <stdint.h>
#include "ff/ff.h"

class MgspicoSettings
{
public:
	struct ITEM
	{
		const char *pName;
		const int num;
		const char *pChoices[4];
	};
	const static int NUM_MENUITEMS = 4;
	enum class RPxxxxCLOCK : uint8_t {CLK125MHZ, CLK240MHZ};
	enum class MUSICDATA : uint8_t	{MGS=0, KIN5=1, TGF=2, VGM=3};
	enum class SCCMODULE : uint8_t {IKASCC=0, HRASCC=1};

private:
#pragma pack(push,1)
	// この構造体はそのままファイル保存されるので一度決めた位置・サイズは変更しない。
	struct SETTINGDATA {
		MUSICDATA	MusicType;
		RPxxxxCLOCK	RpCpuClock;
		uint8_t		AutoRun;			// != 0 : 自動的に演奏を開始する
		uint8_t		RandomPlay;			// != 0 : ランダムの曲順で再生する
		uint8_t		EnforceOPLL;		// != 0 : OPLLの存在模倣する
		SCCMODULE	SccModule;
		uint8_t		Padding[122];		// (構造体サイズを128byteに保つこと)
	};
#pragma pack(pop)

private:
	FATFS m_fs;
	SETTINGDATA m_Setting;

public:
	MgspicoSettings();
	virtual ~MgspicoSettings();
private:
	void setDefault(SETTINGDATA *p);

public:
	int	GetNumItems() const;
	const ITEM *GetItem(const int index ) const;
	void SetChioce(const int indexItem, const int no);
	int GetChioce(const int indexItem) const;

	bool ReadSettingFrom(const char *pFilePath);
	bool WriteSettingTo(const char *pFilePath);
public:
	MUSICDATA GetMusicType() const;
	void SetMusicType(const MUSICDATA type);

	bool Is240MHz() const;
	RPxxxxCLOCK GetRp2040Clock() const;
	void SetRp2040Clock(const RPxxxxCLOCK clk);

	bool GetAutoRun() const;
	void SetAutoRun(const bool bAuto);

	bool GetRandomPlay() const;
	void SetRandomPlay(const bool bRnd);

	bool GetEnforceOPLL() const;
	void SetEnforceOPLL(const bool bEnforce);

	SCCMODULE GetSccModule() const;
	void SetSccModule(const SCCMODULE mod);

};