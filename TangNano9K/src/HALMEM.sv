`include "defines.svh"
`include "if_harz80.svh"
`include "psram/PsramAccessMUX.svh"
`include "psram/PsramController.svh"

`default_nettype none
module halmem (
	input	wire		i_RST_n,
	input	wire		i_CLK_3M579,
	input	wire		i_CLK_3M072,		// for SPDIF 3.072MHz
	output	wire		o_CLK_71M7,
// SPI(Pico -> TangNano9K)
	input	wire		i_SPI_CS_n,
	input	wire		i_SPI_CLK,
	input	wire		i_SPI_MOSI,
	output	wire		o_SPI_MISO,
	output	wire		o_WARN_FULLY,
// SPDIF
	output	wire		o_SPDIF,			// S/PIDF
// DAC
	output	wire		o_DAC_WS,			// DAC // 0=Right-Ch, 1=Left-Ch
	output	wire		o_DAC_CLK,			// 
	output	wire		o_DAC1_L_R,			// I2S(16bit+16bit) L:PSG, Rt:SCC
	output	wire		o_DAC2_L_R,			// I2S(16bit+16bit) L:OPLL, R:OPLL+SCC+PSG
// onbord LED
	output	reg[5:0]	o_LED,				// TangNano9K OnBoard LEDx6, `LOW=Turn-On
// PSRAM(TangNano9K)
	output	wire [1:0] 	O_psram_ck,       // Magic ports for PSRAM to be inferred
	output	wire [1:0] 	O_psram_ck_n,
	inout	wire [1:0] 	IO_psram_rwds,
	inout	wire [15:0] IO_psram_dq,
	output	wire [1:0] 	O_psram_reset_n,
	output	wire [1:0] 	O_psram_cs_n
);
 
assign o_WARN_FULLY = `LOW;

psram_if bus_Psram();
assign 	O_psram_ck		= bus_Psram.O_psram_ck;
assign 	O_psram_ck_n	= bus_Psram.O_psram_ck_n;
assign 	IO_psram_rwds	= bus_Psram.IO_psram_rwds;
assign 	IO_psram_dq		= bus_Psram.IO_psram_dq;
assign 	O_psram_reset_n	= bus_Psram.O_psram_reset_n;
assign 	O_psram_cs_n	= bus_Psram.O_psram_cs_n;

//-----------------------------------------------------------------------
// define
//-----------------------------------------------------------------------
localparam FREQ = 71_590_900;
localparam LATENCY = 3;

//-----------------------------------------------------------------------
// CLOCK
//-----------------------------------------------------------------------
/// 17.9MHz
wire clk_17M9;
Gowin_OSC u_osc (
	.oscout		(clk_17M9)
);
/// 71.7MHz
wire clk_71M7;
wire clk_71M7_p;
Gowin_rPLL_x20p u_rPLL_x20(
	.clkout		(clk_71M7),
	.clkoutp	(clk_71M7_p),
	.clkin		(i_CLK_3M579)
);
///
wire clk_SPIRX		= clk_71M7;
wire clk_PSRAM		= clk_71M7;
wire clk_PSRAM_p	= clk_71M7_p;
wire clk_CONTROL	= clk_71M7;
wire clk_SND		= i_CLK_3M579;
wire clk_Z80CPU		= i_CLK_3M579;
wire clk_SPDIF		= i_CLK_3M072;		// for S/PIDIF 48KHz(32bits x2) (by External)
wire clk_DAC		= clk_17M9;
//
assign o_CLK_71M7	= clk_71M7;

//-----------------------------------------------------------------------
// MIXER
//-----------------------------------------------------------------------
wire signed [10:0] 	snd_WTS_w		= (11'h400 <= bus_Sound.WTS) ? bus_Sound.WTS-11'h400 : bus_Sound.WTS+11'h400;
wire signed [15:0]	snd_WTS_x		= {{5{snd_WTS_w[10]}}, snd_WTS_w} * 6'sd8;
wire signed [15:0]	snd_IKASCC_x	= {{5{bus_Sound.IKASCC[10]}}, bus_Sound.IKASCC} * 6'sd8;
wire signed [15:0]	snd_PSG_x		= bus_Sound.PSG / 2'sd2;
wire signed [15:0]	SOUND_OPLL_x	= bus_Sound.OPLL;
wire signed [15:0]	SOUND_SCC_y		= 16'sd0;//{bus_Sound.IKASCC, 5'b0};
wire signed [15:0]	SOUND_PSG_y	 	= 16'sd0;//{bus_Sound.PSG, 2'b0};
wire signed [15:0]	SOUND_OPLL_y	= 16'sd0;// bus_Sound.OPLL;
wire signed [15:0]	SOUND_ALL_y		= SOUND_OPLL_x + snd_IKASCC_x + snd_PSG_x /*+ snd_WTS_x*/;

//-----------------------------------------------------------------------
// DAC
//-----------------------------------------------------------------------
wire signed [15:0]	SOUND_DAC_z;

MMP_cdc_L2F u_DacCdc (
	.i_RST_n	(i_RST_n		),
	.i_CLK_A	(clk_SND		),
	.i_DATA_A	(SOUND_ALL_y	),
	.i_CLK_B	(clk_DAC		),
	.o_DATA_B	(SOUND_DAC_z	)
);

MMP_dac u_Dac (
	.i_RST_n	(i_RST_n		),
	.i_CLK		(clk_DAC		),
	.i_SCC		(SOUND_SCC_y	),
	.i_PSG		(SOUND_PSG_y	),
	.i_OPLL		(SOUND_OPLL_y	),
	.i_ALL		(SOUND_DAC_z	),
	.o_DAC_WS	(o_DAC_WS		),
	.o_DAC_CLK	(o_DAC_CLK		),
	.o_DAC1_L_R	(o_DAC1_L_R		),
	.o_DAC2_L_R	(o_DAC2_L_R		)
);

//-----------------------------------------------------------------------
// S/PDIF Transmitter
//-----------------------------------------------------------------------
wire signed [15:0]	SOUND_SPDIF_z;
MMP_cdc_F2L u_SpdifCdc (
	.i_RST_n	(i_RST_n		),
	.i_CLK_A	(clk_SND		),
	.i_DATA_A	(SOUND_ALL_y	),
	.i_CLK_B	(clk_SPDIF		),
	.o_DATA_B	(SOUND_SPDIF_z	)
);

MMP_spdif u_spdif (
	.i_RST_n		(i_RST_n		),
	.i_CLK_SPDIF	(clk_SPDIF		),
	.i_SOUND		(SOUND_SPDIF_z	),
	.o_SPDIF		(o_SPDIF		)
);


//-----------------------------------------------------------------------
// PSRAM device controller
//-----------------------------------------------------------------------
psram_ctrl_if	bus_PsramCtrl();
PsramController #(
	.FREQ(FREQ),
	.LATENCY(LATENCY)
) u_PsramCtrl(
    .i_CLK				(clk_PSRAM		),
    .i_CLK_p			(clk_PSRAM_p	),
    .i_RST_n			(i_RST_n		),
	//
	.bus_PsramCtrl		(bus_PsramCtrl	),
	.bus_Psram			(bus_Psram		)
);

//-----------------------------------------------------------------------
// PsramAccessMUX
//-----------------------------------------------------------------------
pamux_if bus_Pamux_pri();
pamux_if bus_Pamux_sec();
PsramAccessMUX u_PsramAccessMUX
(
	.bus_pri	(bus_Pamux_pri	),	// from spitx
	.bus_sec	(bus_Pamux_sec	),	// from u_HarzMMU
	//
	.bus_Psram	(bus_PsramCtrl	)	// to u_PsramCtrl.
);

//-----------------------------------------------------------------------
// HarzMMU
//-----------------------------------------------------------------------
soundbus_if		bus_Sound();
harzbus_if		bus_Harz();
txworkram_if	bus_TxWork();
ccmnd_if		bus_CCmd();

HarzMMU u_HarzMMU
(
	.i_CLK				(clk_CONTROL	),
	.i_RST_n			(i_RST_n		),
	.bus_Harz			(bus_Harz		),
	.bus_Z80			(bus_Z80		),
	.bus_Pamux			(bus_Pamux_sec	),
	.bus_TxWork			(bus_TxWork		),
	.bus_CCmd			(bus_CCmd		),
	.bus_Sound			(bus_Sound		),
	.o_LED				(o_LED			)
);


//-----------------------------------------------------------------------
// TV80
//-----------------------------------------------------------------------
wire clk_TV80CPU;
Gowin_rPLLx2 u_Gowin_rPLLx2(clk_TV80CPU, i_CLK_3M579);
assign bus_Z80.clk = clk_TV80CPU;
//assign bus_Z80.clk = clk_Z80CPU;

assign bus_Z80.bus_clk = i_CLK_3M579;
z80bus_if bus_Z80();
tv80s u_tv80s(
	.reset_n			(bus_Z80.reset_n	),
	.clk				(bus_Z80.clk		),
	// input
	.wait_n				(bus_Z80.wait_n		),
	.int_n				(bus_Z80.int_n		),
	.nmi_n				(bus_Z80.nmi_n		),
	.busrq_n			(bus_Z80.busrq_n	),
	.di					(bus_Z80.di			),
	// output
	.m1_n				(bus_Z80.m1_n		),
	.mreq_n				(bus_Z80.mreq_n		),
	.iorq_n				(bus_Z80.iorq_n		),
	.rd_n				(bus_Z80.rd_n		),
	.wr_n				(bus_Z80.wr_n		),
	.rfsh_n				(bus_Z80.rfsh_n		),
	.halt_n				(bus_Z80.halt_n		),
	.busak_n			(bus_Z80.busak_n	),
	.A					(bus_Z80.A		)	,
	.dout				(bus_Z80.dout		)
);

//-----------------------------------------------------------------------
// SPI RX
//-----------------------------------------------------------------------
// SPIモード2(アイドル時CLK=H、立ち上がりエッジでサンプリング）
// 40bitを一単位として受信を行います。
// また、送信側は40bit毎に、CS(i_SPI_CS_n)を制御する必要があります
reg	[2:0]	spirx_buff_clk;
reg	[2:0]	spirx_buff_ena;
reg	[2:0]	spirx_buff_dat;
reg	[63:0]	spirx_rsv_data;		// cmd(8)+addr(24)+data(32)
reg [63:0]	spirx_sxv_data;
wire		spirx_clk_rise;
wire		spirx_ena_rise;
wire		spirx_ena_fall;
wire		spirx_shift;

// メタステーブル対策のための入力バッファ
always @(posedge clk_SPIRX) begin
	if (!i_RST_n) begin
		spirx_buff_clk <= 3'b111;
		spirx_buff_ena <= 3'b111;
		spirx_buff_dat <= 3'b000;
	end
	else begin
		spirx_buff_clk <= {spirx_buff_clk[1:0], i_SPI_CLK};
		spirx_buff_ena <= {spirx_buff_ena[1:0], i_SPI_CS_n};
		spirx_buff_dat <= {spirx_buff_dat[1:0], i_SPI_MOSI};
	end
end

// エッジ検出
assign spirx_clk_rise = (spirx_buff_clk[2:1] == 2'b01) ? `HIGH : `LOW;
assign spirx_shift = (spirx_clk_rise && !spirx_buff_ena[2]) ? `HIGH : `LOW;
assign spirx_ena_rise = (spirx_buff_ena[2:1] == 2'b01) ? `HIGH : `LOW;
assign spirx_ena_fall = (spirx_buff_ena[2:1] == 2'b10) ? `HIGH : `LOW;

assign o_SPI_MISO = (i_SPI_CS_n) ? 1'bz : spirx_sxv_data[63];

reg [21:0]	membus_adder;
reg [31:0]	membus_data;
reg [7:0]	spirx_bitcnt;

wire spirx_psram_wr_1	= (spirx_bitcnt == 7'd40 && spirx_rsv_data[39:32] == TXCMD_PSRAM_WR_8	);
wire spirx_psram_rd_1	= (spirx_bitcnt == 7'd32 && spirx_rsv_data[31:24] == TXCMD_PSRAM_RD_8	);
wire spirx_z80io_wr		= (spirx_bitcnt == 7'd24 && spirx_rsv_data[23:16] == TXCMD_Z80IO_WR		);
wire spirx_z80io_rd		= (spirx_bitcnt == 7'd16 && spirx_rsv_data[15:8] == TXCMD_Z80IO_RD		);
wire spirx_z80mem_wr_1	= (spirx_bitcnt == 7'd32 && spirx_rsv_data[31:24] == TXCMD_Z80MEM_WR_1	);
wire spirx_z80mem_rd_1	= (spirx_bitcnt == 7'd24 && spirx_rsv_data[23:16] == TXCMD_Z80MEM_RD_1	);
//
wire spirx_harz_reset	= (spirx_bitcnt == 7'd08 && spirx_rsv_data[7:0] == TXCMD_HARZ_RESET		);
wire spirx_harz_run		= (spirx_bitcnt == 7'd08 && spirx_rsv_data[7:0] == TXCMD_HARZ_RUN		);
wire spirx_harz_stop	= (spirx_bitcnt == 7'd08 && spirx_rsv_data[7:0] == TXCMD_HARZ_STOP		);
//
wire spirx_harz_getsts	= (spirx_bitcnt == 7'd08 && spirx_rsv_data[7:0] == TXCMD_HARZ_GETSTS	);
wire spirx_harz_setcmd	= (spirx_bitcnt == 7'd16 && spirx_rsv_data[15:8] == TXCMD_HARZ_SETCMD	);


reg [6:0] spirx_bytecnt;


reg [8:0] counter_for_cpureset;

typedef enum logic [4:0]
{
	MEMBUS_IDLE,
	// PSRAM 
	MEMBUS_PSRAM_WR1_1,		// Write 1 byte
	MEMBUS_PSRAM_WR1_2,
	MEMBUS_PSRAM_WR1_3,
	MEMBUS_PSRAM_RD1_1,		// Raed 1 byte
	MEMBUS_PSRAM_RD1_2,
	MEMBUS_PSRAM_RD1_3,
	// harz80 MMU(I/O)
	MEMBUS_Z80IO_WR_1,		// Write 1 byte
	MEMBUS_Z80IO_WR_2,
	MEMBUS_Z80IO_WR_3,
	MEMBUS_Z80IO_RD_1,		// Read 1 byte
	MEMBUS_Z80IO_RD_2,
	MEMBUS_Z80IO_RD_3,
	// harz80 MMU(Mmeory)
	MEMBUS_Z80MEM_WR1_1,	// Write 1 byte
	MEMBUS_Z80MEM_WR1_2,
	MEMBUS_Z80MEM_WR1_3,
	MEMBUS_Z80MEM_RD1_1,	// Raed 1 byte
	MEMBUS_Z80MEM_RD1_2,
	MEMBUS_Z80MEM_RD1_3,
	//
	MEMBUS_CPU_RESET,
	MEMBUS_WAIT_CPU_BUSAK_ON,	// Z80 
	MEMBUS_WAIT_CPU_BUSAK_OFF,	// Z80 
	//
	MEMBUS_HARZ_GETSTS,
	MEMBUS_HARZ_GETSTS_2,
	MEMBUS_HARZ_GETSTS_3,
	MEMBUS_HARZ_GETSTS_4,
	//
	MEMBUS_HARZ_SETCMD,
	MEMBUS_HARZ_SETCMD_2,
	MEMBUS_HARZ_SETCMD_3,
	//
	MEMBUS_FINISH
} membus_sts_t;
membus_sts_t	membus_sts;


always @(posedge clk_SPIRX) begin
	if (!i_RST_n) begin
		spirx_sxv_data <= 64'h0;
		spirx_rsv_data <= 64'h0;
		spirx_bitcnt <= 7'd0;
		//
		bus_Pamux_pri.write <= `LOW;
		bus_Pamux_pri.read <= `LOW;
		//
		bus_Harz.address <= 32'd0;
		bus_Harz.write_data <= 32'd0;
		//
		membus_sts <= MEMBUS_IDLE;
		membus_adder <= 22'h0;
		membus_data <= 32'h0;
		//
		bus_Z80.reset_n <= `LOW;
		bus_Z80.busrq_n <= `LOW;		//cpu を沈黙させる
		counter_for_cpureset <= 9'd0;
	end
	else begin
		if (/*spirx_ena_rise||*/spirx_ena_fall) begin
			spirx_bitcnt <= 7'd0;
			spirx_rsv_data <= 64'h0;
			membus_sts <= MEMBUS_IDLE;
		end
		if (spirx_shift) begin
			spirx_bitcnt <= spirx_bitcnt + 7'd1;
			spirx_rsv_data <= {spirx_rsv_data[62:0], spirx_buff_dat[2]};
			spirx_sxv_data <= {spirx_sxv_data[62:0], 1'b0};
		end
		case (membus_sts) 
			//  ------------------------------
			MEMBUS_IDLE: begin
				// PSRAM ------------------------------------------------
				if( spirx_psram_wr_1 ) begin
					membus_sts <= MEMBUS_PSRAM_WR1_1;
					membus_adder <= spirx_rsv_data[29:8];	// 4MB space
					membus_data[7:0] <= spirx_rsv_data[7:0];
				end
				if( spirx_psram_rd_1 ) begin
					membus_sts <= MEMBUS_PSRAM_RD1_1;
					membus_adder <= spirx_rsv_data[21:0];	// 4MB space
				end
				// I/O --------------------------------------------------
				if( spirx_z80io_wr ) begin
					membus_sts <= MEMBUS_Z80IO_WR_1;
					membus_adder <= spirx_rsv_data[15:8];	// 256 space
					membus_data[7:0] <= spirx_rsv_data[7:0];
				end
				if( spirx_z80io_rd ) begin
					membus_sts <= MEMBUS_Z80IO_RD_1;
					membus_adder <= spirx_rsv_data[7:0];	// 256 space
				end
				// MEM 1 ------------------------------------------------
				if( spirx_z80mem_wr_1 ) begin
					membus_sts <= MEMBUS_Z80MEM_WR1_1;
					membus_adder <= spirx_rsv_data[23:8];	// 64KB space
					membus_data[7:0] <= spirx_rsv_data[7:0];
				end
				if( spirx_z80mem_rd_1 ) begin
					membus_sts <= MEMBUS_Z80MEM_RD1_1;
					membus_adder <= spirx_rsv_data[15:0];	// 64KB space
				end
				// -----------------------------------------------------
				if( spirx_harz_reset ) begin
					bus_Z80.reset_n <= `LOW;
					bus_Z80.busrq_n <= `HIGH;
					counter_for_cpureset <= 9'd0;
					membus_sts <= MEMBUS_CPU_RESET;
				end
				if( spirx_harz_run ) begin
					bus_Z80.busrq_n <= `HIGH;	//cpu を動作させる
					membus_sts <= MEMBUS_WAIT_CPU_BUSAK_OFF;
				end
				if( spirx_harz_stop ) begin
					bus_Z80.busrq_n <= `LOW;	//cpu を沈黙させる
					membus_sts <= MEMBUS_WAIT_CPU_BUSAK_ON;
				end
				if( spirx_harz_getsts ) begin
					membus_sts <= MEMBUS_HARZ_GETSTS;
					spirx_bytecnt <= 7'd0;
				end
				// -----------------------------------------------------
				if( spirx_harz_setcmd ) begin
					membus_sts <= MEMBUS_HARZ_SETCMD;
					membus_data[7:0] <= spirx_rsv_data[7:0];
				end

			end
			// -----------------------
			MEMBUS_CPU_RESET:begin
				counter_for_cpureset <= counter_for_cpureset + 9'd1;
				if( counter_for_cpureset == 9'd120 /* 6 clock */ ) begin
					bus_Z80.reset_n <= `HIGH;
					membus_sts <= MEMBUS_WAIT_CPU_BUSAK_OFF;
				end
			end
			// -----------------------
			MEMBUS_WAIT_CPU_BUSAK_ON: begin
				// CPUがバスを開放するのを待つ（CPUが停止する野を待つのが目的）
				if( bus_Z80.busak_n == `LOW ) begin
					membus_sts <= MEMBUS_FINISH;
				end
			end
			MEMBUS_WAIT_CPU_BUSAK_OFF: begin
				// CPUが動作するのを待つ
				if( bus_Z80.busak_n == `HIGH ) begin
					membus_sts <= MEMBUS_FINISH;
				end
			end
			// -----------------------
			MEMBUS_HARZ_GETSTS: begin
				if( spirx_bitcnt[2:0] == 3'b0 ) begin
					bus_TxWork.read <= `HIGH;
					bus_TxWork.read_index <= spirx_bytecnt;
					membus_sts <= MEMBUS_HARZ_GETSTS_2;
					spirx_bytecnt <= spirx_bytecnt + 7'd1;
				end
			end
			MEMBUS_HARZ_GETSTS_2: begin
				membus_sts <= MEMBUS_HARZ_GETSTS_3;
			end
			MEMBUS_HARZ_GETSTS_3: begin
				bus_TxWork.read <= `LOW;
				spirx_sxv_data[63:56] <= bus_TxWork.read_data;
				if( spirx_bytecnt == 7'd46 )
					membus_sts <= MEMBUS_FINISH;
				else
					membus_sts <= MEMBUS_HARZ_GETSTS_4;
			end
			MEMBUS_HARZ_GETSTS_4: begin
				if( spirx_bitcnt[2:0] != 3'b0 )
					membus_sts <= MEMBUS_HARZ_GETSTS;
			end
			// -----------------------
			MEMBUS_HARZ_SETCMD: begin
				bus_CCmd.write_data <= membus_data[7:0];
				membus_sts <= MEMBUS_HARZ_SETCMD_2;
			end
			MEMBUS_HARZ_SETCMD_2: begin
				bus_CCmd.write <= `HIGH;
				membus_sts <= MEMBUS_HARZ_SETCMD_3;
			end
			MEMBUS_HARZ_SETCMD_3: begin
				bus_CCmd.write <= `LOW;
				membus_sts <= MEMBUS_FINISH;
			end
			// PSRAM書込み(1Byte)-----------------
			MEMBUS_PSRAM_WR1_1: begin
				bus_Pamux_pri.address <= membus_adder;
				bus_Pamux_pri.write <= `HIGH;
				bus_Pamux_pri.write_data[7:0] <= membus_data[7:0];
				membus_sts <= MEMBUS_PSRAM_WR1_2;
			end
			MEMBUS_PSRAM_WR1_2: begin
				membus_sts <= MEMBUS_PSRAM_WR1_3;
				bus_Pamux_pri.write <= `LOW;
			end
			MEMBUS_PSRAM_WR1_3: begin
				if( !bus_Pamux_pri.busy ) begin
					membus_sts <= MEMBUS_FINISH;
				end
			end
			// PSRAM読込み(1Byte) -------------------
			MEMBUS_PSRAM_RD1_1: begin
				bus_Pamux_pri.address <= membus_adder;
				bus_Pamux_pri.read <= `HIGH;
				membus_sts <= MEMBUS_PSRAM_RD1_2;
			end
			MEMBUS_PSRAM_RD1_2: begin
				bus_Pamux_pri.read <= `LOW;
				membus_sts <= MEMBUS_PSRAM_RD1_3;
			end
			MEMBUS_PSRAM_RD1_3: begin
				if(!bus_Pamux_pri.busy ) begin
					membus_sts <= MEMBUS_FINISH;
					spirx_sxv_data[63:56] <= bus_Pamux_pri.read_data[7:0];
				end
			end
			// Z80 IO書込み -----------------
			MEMBUS_Z80IO_WR_1: begin
				bus_Harz.request <= HARZ80_IO_WRITE;
				bus_Harz.address[7:0] <= membus_adder[7:0];		// I/O 8bits
				bus_Harz.write_data[7:0] <= membus_data[7:0];
				membus_sts <= MEMBUS_Z80IO_WR_2;
			end
			MEMBUS_Z80IO_WR_2: begin
				bus_Harz.request <= HARZ80_NONE;
				membus_sts <= MEMBUS_Z80IO_WR_3;
			end
			MEMBUS_Z80IO_WR_3: begin
				if( !bus_Harz.busy ) begin
					membus_sts <= MEMBUS_FINISH;
				end
			end
			// Z80 IO読込み -----------------
			MEMBUS_Z80IO_RD_1: begin
				bus_Harz.request <= HARZ80_IO_READ;
				bus_Harz.address[7:0] <= membus_adder[7:0];
				membus_sts <= MEMBUS_Z80IO_RD_2;
			end
			MEMBUS_Z80IO_RD_2: begin
				bus_Harz.request <= HARZ80_NONE;
				membus_sts <= MEMBUS_Z80IO_RD_3;
			end
			MEMBUS_Z80IO_RD_3: begin
				if( !bus_Harz.busy ) begin
					membus_sts <= MEMBUS_FINISH;
//					spirx_sxv_data[63:56] <= bus_Harz.read_data[7:0];
				end
			end
			// Z80MEM書込み(1Byte) ------------
			MEMBUS_Z80MEM_WR1_1: begin
				bus_Harz.request <= HARZ80_MEM_WRITE_1;
				bus_Harz.address <= membus_adder[15:0];
				bus_Harz.write_data[7:0] <= membus_data[7:0];
				membus_sts <= MEMBUS_Z80MEM_WR1_2;
			end
			MEMBUS_Z80MEM_WR1_2: begin
				bus_Harz.request <= HARZ80_NONE;
				membus_sts <= MEMBUS_Z80MEM_WR1_3;
			end
			MEMBUS_Z80MEM_WR1_3: begin
				if( !bus_Harz.busy ) begin
					membus_sts <= MEMBUS_FINISH;
				end
			end
			// Z80MEM読込み(1Byte) -----------------
			MEMBUS_Z80MEM_RD1_1: begin
				bus_Harz.request <= HARZ80_MEM_READ_1;
				bus_Harz.address <= membus_adder[15:0];
				membus_sts <= MEMBUS_Z80MEM_RD1_2;
			end
			MEMBUS_Z80MEM_RD1_2: begin
				bus_Harz.request <= HARZ80_NONE;
				membus_sts <= MEMBUS_Z80MEM_RD1_3;
			end
			MEMBUS_Z80MEM_RD1_3: begin
				if( !bus_Harz.busy ) begin
					membus_sts <= MEMBUS_FINISH;
//					spirx_sxv_data[63:56] <= bus_Harz.read_data[7:0];
				end
			end
			// -------------------------------
			default: begin
				// do nothing
				// if( spirx_buff_ena[0] == `HIGH ) begin
				// spirx_bitcnt <= 7'd0;
				// spirx_rsv_data <= 64'h0;
				// membus_sts <= MEMBUS_IDLE;
				// end
			end
		endcase
	end
end


endmodule
`default_nettype wire