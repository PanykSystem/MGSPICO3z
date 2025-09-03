#include "stdafx.h"
#include <string.h>
#include "CReadFileStream.h"

#include <stdio.h>

//static uint8_t m_StrmBuff[32*1024];	// RAM容量チェック用

CReadFileStream::CReadFileStream()
{
	m_pBuff32k = GCC_NEW uint8_t[SIZE_SEGMEMT*NUM_SEGMEMTS];
	sem_init(&m_sem, 1, 1);
	init(0);
	reset();
	return;
}

CReadFileStream::~CReadFileStream()
{
	NULL_DELETEARRAY(m_pBuff32k);
	// do nothing
	return;
}

void CReadFileStream::init(const uint32_t offset)
{
	m_totalFileSize = 0;
	m_offset = offset;
	return;
}

void CReadFileStream::reset()
{
	for( int t = 0; t < NUM_SEGMEMTS; ++t){
		m_segs.Size[t] = 0;
	}
	m_segs.ValidSegmentNum = 0;
	m_segs.WriteSegmentIndex = 0;
	m_segs.ReadSegmentIndex = 0;
	m_segs.ReadIndexInSegment = 0;
	m_loadedFileSize = 0;
	return;
}

void CReadFileStream::SetTargetFileName(const char *pFName, const uint32_t offset)
{
	init(offset);
	reset();
	strcpy(m_filepath, pFName);
	m_totalFileSize = sd_fatGetFileSize(m_filepath);
	m_effectiveFileSize = m_totalFileSize - m_offset;
	return;
}

uint32_t CReadFileStream::GetFileSize() const
{
	return m_totalFileSize;
}

uint32_t CReadFileStream::GetEffectiveFileSize() const
{
	return m_effectiveFileSize;
}

uint32_t CReadFileStream::GetLeftInSegment() const
{
	sem_acquire_blocking(&m_sem);
	const uint32_t s = m_segs.Size[m_segs.ReadSegmentIndex] - m_segs.ReadIndexInSegment;
	sem_release(&m_sem);
	return s;
}

// @return true ディスクアクセスあり
bool CReadFileStream::FetchFile()
{
	bool bDiskAcc = false;
	int validNum = m_segs.ValidSegmentNum;
	const int n = NUM_SEGMEMTS - validNum;
	for(int t = 0; t < n; ++t) {
		uint32_t est = m_effectiveFileSize - m_loadedFileSize;
		if( SIZE_SEGMEMT < est )
			est = SIZE_SEGMEMT;
		auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT*m_segs.WriteSegmentIndex];
		UINT readSize;
		bDiskAcc = true;
		//printf("FetchFile: off=%d, load=%d, est=%d\n", m_offset+m_loadedFileSize, m_loadedFileSize, est);
		if( sd_fatReadFileFromOffset( m_filepath, m_offset+m_loadedFileSize, est, pReadPos, &readSize)) {
			//printf("read %d-%d\n", m_offset+m_loadedFileSize, m_offset+m_loadedFileSize+readSize);
			m_segs.Size[m_segs.WriteSegmentIndex] = readSize;
			m_loadedFileSize += readSize;
			m_segs.WriteSegmentIndex = (m_segs.WriteSegmentIndex +1) % NUM_SEGMEMTS;
			if( m_effectiveFileSize <= m_loadedFileSize ) {
				m_loadedFileSize = 0;
			}
			sem_acquire_blocking(&m_sem);
			++m_segs.ValidSegmentNum;
			sem_release(&m_sem);
		}
	}
	return bDiskAcc;
}

bool CReadFileStream::Store(uint8_t *pDt, const int size)
{
	if( m_totalFileSize == 0 )
		return false;
	int s = size;
	int destIndex = 0;
	bool bRetc = false;
	while( 0 < s ) {
		int validNum = m_segs.ValidSegmentNum;
		if( validNum == 0 ){
			sem_release(&m_sem);
			break;
		}
		int sp = m_segs.Size[m_segs.ReadSegmentIndex] - m_segs.ReadIndexInSegment;
		if( s < sp )
			sp = s;
		auto *pReadPos = &m_pBuff32k[SIZE_SEGMEMT * m_segs.ReadSegmentIndex];
		if( pDt != nullptr ) {
			memcpy(&pDt[destIndex], &pReadPos[m_segs.ReadIndexInSegment], sp);
		}
		// Update ReadIndex
		auto &sz = m_segs.Size[m_segs.ReadSegmentIndex];
		m_segs.ReadIndexInSegment += sp;
		if( sz <= m_segs.ReadIndexInSegment ){
			m_segs.ReadIndexInSegment = 0;
			m_segs.ReadSegmentIndex = (m_segs.ReadSegmentIndex +1) % NUM_SEGMEMTS;
			sz = 0;
			sem_acquire_blocking(&m_sem);
			--(m_segs.ValidSegmentNum);
			sem_release(&m_sem);
		}
		//
		s -= sp;
		destIndex += sp;
		bRetc = true;
	}
	return bRetc;
}
