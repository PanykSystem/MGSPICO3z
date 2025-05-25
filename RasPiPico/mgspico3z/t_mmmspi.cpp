#include "stdafx.h"
#include <stdio.h>		// printf
#include "t_mmmspi.h"

namespace mmmspi
{
static const uint16_t LEN_BUFF = 1024;
#pragma pack(push,1)
static uint32_t g_Buff[LEN_BUFF];
#pragma pack(pop)
static uint16_t g_RecordNum;		// 格納数
static uint16_t g_ReadIndex;		// 読込みインデックス
static uint16_t g_WriteIndex;	// 書込みインデックス

bool Init()
{
	g_RecordNum = 0;
	g_ReadIndex = 0;
	g_WriteIndex = 0;
	//
	spi_init( HARZ80_SPI_DEV, (uint)6600000 ); 	
//	uint spd = spi_init( HARZ80_SPI_DEV, (uint)6600000 ); 	
//	uint spd = spi_init( HARZ80_SPI_DEV, (uint)500000 );
	sleep_ms(1000);
//	printf("SPI speed=%d\n", spd);
    gpio_set_function( HARZ80_SPI_PIN_TX, GPIO_FUNC_SPI );
    gpio_set_function( HARZ80_SPI_PIN_RX, GPIO_FUNC_SPI );
    gpio_set_function( HARZ80_SPI_PIN_SCK, GPIO_FUNC_SPI );

	// gpio_pull_up(HARZ80_SPI_PIN_TX);
	// gpio_pull_up(HARZ80_SPI_PIN_RX);
	// gpio_pull_up(HARZ80_SPI_PIN_SCK);

	gpio_disable_pulls(HARZ80_SPI_PIN_TX);
    gpio_disable_pulls(HARZ80_SPI_PIN_RX);
    gpio_disable_pulls(HARZ80_SPI_PIN_SCK);

    /* CS# */
    gpio_init( HARZ80_SPI_PIN_CS_n );
    gpio_set_dir( HARZ80_SPI_PIN_CS_n, GPIO_OUT);
	//
    gpio_put( HARZ80_SPI_PIN_CS_n, 1 );	/* Set CS# high */
	busy_wait_ms(10);

	return true;
}

void ClearBUff()
{
	g_RecordNum = 0;
	return;
}

RAM_FUNC void PushBuff(const CMD cmd, const uint32_t addr, const uint32_t data)
{
	if( LEN_BUFF <= g_RecordNum )
		return;
	// [5:CMD][11:ADDR][8:DATA]
	const uint32_t rec = (((uint32_t)cmd)<<19) | (addr<<8) | data;
	g_Buff[g_WriteIndex++] = rec;
	g_WriteIndex &= 0x03ff;	// 1024で0に戻す
	g_RecordNum++;
	return;
}

RAM_FUNC void Present()
{
	const int num = g_RecordNum;
	for( int t = 0; t < num; ++t)
	{
		const uint32_t rec = g_Buff[g_ReadIndex++];
		g_ReadIndex &= 0x03ff;	// 1024で0に戻す
		uint8_t temp[3] = {
			static_cast<uint8_t>(rec>>16),
			static_cast<uint8_t>((rec>>8)&0xff),
			static_cast<uint8_t>(rec&0xff)};

	    gpio_put( HARZ80_SPI_PIN_CS_n, 0 );	/* Set CS# low */
		spi_write_blocking( HARZ80_SPI_DEV, temp, 3);
	    gpio_put( HARZ80_SPI_PIN_CS_n, 1 );	/* Set CS# high */
	}
	g_RecordNum = 0;
	return;
}

}; // namespace mmmspi

