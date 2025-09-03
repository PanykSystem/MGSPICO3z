#pragma once
#include <stdlib.h>
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "pico/multicore.h"
#include "../global.h"
#include "../sdfat.h"

class CReadFileStream
{
private:
	// ファイル名の文字長
	static const int LEN_FILE_SPEC = 8+1+3;
private:
	static const int SIZE_SEGMEMT = 4*1024;
	static const int NUM_SEGMEMTS = 8;	// 4K*8 = 32K
	uint8_t *m_pBuff32k;				// バッファへのポインタ(32KBytes）
	struct SEGMENTS
	{
		int Size[NUM_SEGMEMTS];
		int	ValidSegmentNum;
		int	WriteSegmentIndex;
		int	ReadSegmentIndex;
		int	ReadIndexInSegment;
	};
	SEGMENTS m_segs;
	//
	char m_filepath[256+1];
	uint32_t m_totalFileSize;			// ファイル総サイズ
	uint32_t m_loadedFileSize;			// ファイルサイズに対する読込済みサイズ
	uint32_t m_offset;					// 読み込み対象外にする先頭サイズ
	uint32_t m_effectiveFileSize;		// 対象外を除いたファイルサイズ
	mutable semaphore_t m_sem;

public:
	CReadFileStream();
	virtual ~CReadFileStream();
private:
	void init(const uint32_t offset);
	void reset();
public:
	void SetTargetFileName(const char *pFName, const uint32_t offset = 0);
	uint32_t GetFileSize() const;
	uint32_t GetEffectiveFileSize() const;
	uint32_t GetLeftInSegment() const;
	bool FetchFile();
	bool Store(uint8_t *pDt, const int size);
};

