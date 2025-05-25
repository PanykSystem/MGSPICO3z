#pragma once

#include "MgspicoSettings.h"

// ファイル名の文字長
static const int LEN_FILE_SPEC = 8;
static const int LEN_FILE_EXT = 3;
static const int LEN_FILE_NAME = LEN_FILE_SPEC+1+LEN_FILE_EXT;


class MusFiles
{
public:
	struct FILESPEC{
		char name[LEN_FILE_NAME+1];
	};

private:
	static const int MAX_FILES = 1000;
	MgspicoSettings::MUSICDATA	m_Type;
	int m_NumFiles;
	FILESPEC m_Files[MAX_FILES];

public:
	MusFiles();
	virtual ~MusFiles();

private:
	void listupFiles(FILESPEC *pList, int *pNum, const char *pWild);

public:
	void ReadFileNames(const char *pWild);
	int GetNumFiles() const;
	bool IsEmpty() const;
	const FILESPEC *GetFileSpec(const int no) const;
	void SetMusicType(const MgspicoSettings::MUSICDATA type);
	MgspicoSettings::MUSICDATA GetMusicType() const;
};
