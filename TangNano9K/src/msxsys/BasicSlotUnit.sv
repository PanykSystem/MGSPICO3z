`include "../defines.svh"
`include "../if_harz80.svh"

`default_nettype none
module BasicSlotUnit
(
	input	wire				i_CLK,
	input	wire				i_RST_n,
	//
	msxslotbus_if.host			bus_Slot,
	pamux_if.client				bus_Pamux,
	txworkram_if.host			bus_TxWork,
	ccmnd_if.host				bus_CCmd,
	//
	soundbus_if.src				bus_Sound,
	//
	input	wire [4:0]			i_masicn_IKASCC,
	//
	output	wire [5:0]			o_LED
);

assign bus_Slot.busy = mem_busy|io_busy;
wire mem_busy = busy_s3_p0|busy_s3_p1|busy_s3_p2|busy_s3_p3|busy_s1_p1|busy_s1_p2;
assign bus_Sound.WTS = 11'sd0;

// ==========================================================================
// LED x6 (I/O 0x00)
// ==========================================================================
assign o_LED = ~led_sts;
reg [5:0] led_sts;
always_ff @(posedge i_CLK) begin
	if( !i_RST_n ) begin
		led_sts <= 6'b0;
	end 
	else begin
		if( bus_Slot.iorq && bus_Slot.wr && bus_Slot.a[7:0]==8'h00 ) begin
			led_sts <= bus_Slot.write_d[5:0];
		end
	end
end

// ==========================================================================
// 100us counter (I/O 0x01)
// ==========================================================================
wire io_acc_100us = ((bus_Slot.a[7:0]==8'h01)&&bus_Slot.iorq);
assign bus_Slot.read_d = (io_acc_100us && bus_Slot.rd) ? cntusec : 8'bz;

reg [15:0] clkcnt71M7;
reg [7:0] cntusec;
reg [1:0] ff_accwr;
always_ff @(posedge i_CLK) begin
	if( !i_RST_n ) begin
		clkcnt71M7 <= 15'd0;
		cntusec <= 8'h00;
		ff_accwr <= 2'b0;
	end 
	else begin
		ff_accwr <= {ff_accwr[0], (io_acc_100us&bus_Slot.wr)};
		if( ff_accwr == 2'b01 ) begin
			cntusec <= 8'h00;
			clkcnt71M7 <= 15'd0;
		end
		else begin 
			if( clkcnt71M7 == 15'd7158 ) begin
				if( cntusec != 8'hff )					// 上限 ffhで止める
					cntusec <= cntusec + 8'h01;
				clkcnt71M7 <= 15'd0;
			end
			else begin
				clkcnt71M7 <= clkcnt71M7 + 15'd1;
			end
		end
	end
end

// ==========================================================================
// TxWorkRam (I/O 0x02, 0x03)
// ==========================================================================
wire io_acc_txworkram_wr = ((bus_Slot.a[7:0]==8'h02)&&bus_Slot.iorq&&bus_Slot.wr);
wire io_acc_txworkram_ed = ((bus_Slot.a[7:0]==8'h03)&&bus_Slot.iorq&&bus_Slot.wr);

TxWorkRam u_txworkram(
	.i_CLK			(i_CLK					),
	.i_RST_n		(i_RST_n				),
	//
	.i_R1_WR		(io_acc_txworkram_wr	),
	.i_R1_WR_DATA	(bus_Slot.write_d		),
	.i_COPY			(io_acc_txworkram_ed	),
	//
	.i_R2_RD		(bus_TxWork.read		),	// in i_CLK
	.i_R2_RD_INDEX	(bus_TxWork.read_index	),
	.o_R2_OUT_DATA	(bus_TxWork.read_data	)
);

// ==========================================================================
// CCMD_RD (I/O 0x04)
// ==========================================================================
wire io_acc_ccmd = ((bus_Slot.a[7:0]==8'h04)&&bus_Slot.iorq);
assign bus_Slot.read_d = (io_acc_ccmd && bus_Slot.rd) ? ff_ccmd_data_out : 8'bz;
reg [7:0] ff_ccmd_data;
reg [7:0] ff_ccmd_data_out;
reg [1:0] ff_ccmd_rd_sht;

always_ff@(posedge i_CLK ) begin
 	if( !i_RST_n ) begin
		ff_ccmd_data <= 8'h00;
	end
	else begin
		ff_ccmd_rd_sht <= {ff_ccmd_rd_sht[0], bus_Slot.rd};
		if( bus_CCmd.write ) begin
			ff_ccmd_data <= bus_CCmd.write_data;
		end
		else if( io_acc_ccmd && ff_ccmd_rd_sht == 2'b01 ) begin
			ff_ccmd_data <= 8'h00;
			ff_ccmd_data_out <= ff_ccmd_data;
		end
	end
end


// ==========================================================================
// Timre RD (I/O 0xE6,0xE7)
// ==========================================================================
wire clk_895K;
wire clk_255K69;
Gowin_CLKDIV_4 uGowin_CLKDIV_4(clk_895K, i_CLK, i_RST_n);
Gowin_CLKDIV_35 uGowin_CLKDIV_35(clk_255K69, clk_895K, i_RST_n);
reg [15:0] ff_CounterTimer;

assign bus_Slot.read_d = ((bus_Slot.a[7:0]==8'he6)&&bus_Slot.iorq&&bus_Slot.rd) ? ff_CounterTimer[7:0] : 8'bz;
assign bus_Slot.read_d = ((bus_Slot.a[7:0]==8'he7)&&bus_Slot.iorq&&bus_Slot.rd) ? ff_CounterTimer[15:8] : 8'bz;

always_ff@(posedge clk_255K69 ) begin
 	if( !i_RST_n ) begin
		ff_CounterTimer <= 16'd0;
	end
	else begin
		ff_CounterTimer <= ff_CounterTimer + 16'd1;
	end
end

// ==========================================================================
// PSG(ym2149_audio)
// ==========================================================================
wire bus_PSG_CS = (bus_Slot.iorq&&bus_Slot.wr&&((bus_Slot.a[7:0]==8'hA0)||(bus_Slot.a[7:0]==8'hA1)));
wire w_BC_PSG	= (bus_PSG_CS) & (!bus_Slot.a[0]); 
wire w_BDIR_PSG	= (bus_PSG_CS);
reg ff_BC_PSG;
reg ff_BDIR_PSG;
reg [7:0] ff_psg_data;

always_ff@(posedge i_CLK ) begin
 	if( !i_RST_n ) begin
		ff_BC_PSG <= 1'b0;
		ff_BDIR_PSG <= 1'b0;
		ff_psg_data <= 8'b0;
	end
	else begin
		// CPUバスの内容をi_CLKの1サイクル分遅らせてPSGに伝える
		// PSGクロックの立ち上がりぴったりにff_BC_PSG/ff_BDIR_PSGを変化させると、
		// PSGがそれを拾ってくれないため。
		ff_BC_PSG <= w_BC_PSG;
		ff_BDIR_PSG <= w_BDIR_PSG;
		ff_psg_data <= bus_Slot.write_d;
	end
end

ym2149_audio u_Ym2149Audio (
	.clk_i			(bus_Slot.clock		),	//  in     std_logic -- system clock
	.en_clk_psg_i	(1'b1				),	//  in     std_logic -- PSG clock enable
	.sel_n_i		(1'b0				),	//  in     std_logic -- divide select, 0=clock-enable/2
	.reset_n_i		(bus_Slot.reset_n	),	//  in     std_logic -- active low
	.data_i			(ff_psg_data		),	//  in     std_logic_vector(7 downto 0)
	.bc_i			(ff_BC_PSG			),	//  in     std_logic -- bus control
	.bdir_i			(ff_BDIR_PSG		),	//  in     std_logic -- bus direction
	.data_r_o		(),						//  out    std_logic_vector(7 downto 0) -- registered output data
	.ch_a_o			(),						//  out    unsigned(11 downto 0)
	.ch_b_o			(),						//  out    unsigned(11 downto 0)
	.ch_c_o			(),						//  out    unsigned(11 downto 0)
	.mix_audio_o	(),						//  out    unsigned(13 downto 0)
	.pcm14s_o		(bus_Sound.PSG		)	//  out    unsigned(13 downto 0)
);

// PSG I/O アクセス時に CPUクロックで1サイクルのWAIT期間を追加する回路
reg [6:0] psg_forbusy_cnt;
reg psg_busy;
reg psg_nobusy;
always_ff @(posedge i_CLK) begin
	if (!i_RST_n) begin
		psg_forbusy_cnt <= 7'd0;
		psg_busy <= `LOW;
		psg_nobusy <= `LOW;
	end
	else begin
		if( !psg_nobusy && !psg_busy && w_BC_PSG ) begin
			psg_busy <= `HIGH;
			psg_forbusy_cnt <= 7'd58;	// WAITをつくり出すための期間
		end
		else begin
			if( psg_busy ) begin
				psg_forbusy_cnt <= psg_forbusy_cnt - 7'd1;
				if( psg_forbusy_cnt == 7'd0 ) begin
					psg_busy <= `LOW;
					psg_forbusy_cnt <= 7'd26;
					psg_nobusy <= `HIGH;
				end
			end
			if( psg_nobusy ) begin
				psg_forbusy_cnt <= psg_forbusy_cnt - 7'd1;
				if( psg_forbusy_cnt == 7'd0 ) begin
					psg_nobusy <= `LOW;
				end
			end
		end
	end
end
wire io_busy = psg_busy;

//-----------------------------------------------------------------------
// OPLL(IKAOPLL)
//-----------------------------------------------------------------------
reg		signed [4:0]	opll_movol;
reg		signed [4:0]	opll_rovol;
wire w_OPLL_CS = (bus_Slot.iorq&&bus_Slot.wr&&((bus_Slot.a[7:0]==8'h7C)||(bus_Slot.a[7:0]==8'h7D)));
wire w_opll_A0 = bus_Slot.a[0];	// 0=adress, 1=data
reg ff_opll_A0;
reg [7:0] ff_opll_data;
reg [5:0] ff_OPLL_CS_cnt;
reg ff_OPLL_CS;
reg [5:0] ff_OPLL_WR_cnt;
reg ff_OPLL_WR;
reg [1:0] w_OPLL_CS_sft;

always_ff@(posedge i_CLK ) begin
	if( !i_RST_n ) begin
		ff_OPLL_WR_cnt <= 6'd0;
		ff_OPLL_WR <= `LOW;
		ff_OPLL_CS_cnt <= 6'd0;
		ff_OPLL_CS <= `LOW;
		ff_opll_A0 <= 1'b0;
		w_OPLL_CS_sft <= 2'b0;
	end
	else begin
		w_OPLL_CS_sft <= {w_OPLL_CS_sft[0], w_OPLL_CS};
		if( w_OPLL_CS_sft == 2'b01 ) begin
			ff_opll_A0 <= w_opll_A0;
			ff_opll_data <= bus_Slot.write_d;
			ff_OPLL_WR_cnt <= 6'd0;
			ff_OPLL_WR <= `HIGH;
			ff_OPLL_CS_cnt <= 6'd0;
			ff_OPLL_CS <= w_OPLL_CS;
		end
		else begin
			ff_OPLL_WR_cnt <= ff_OPLL_WR_cnt + 6'd1;
			if( ff_OPLL_WR_cnt == 6'd36 )
				ff_OPLL_WR <= `LOW;
			ff_OPLL_CS_cnt <= ff_OPLL_CS_cnt + 6'd1;
			if( ff_OPLL_CS_cnt == 6'd38 )
				ff_OPLL_CS <= `LOW;
		end
	end
end


IKAOPLL #(
    .FULLY_SYNCHRONOUS          (1 ),
    .FAST_RESET                 (1 ),
    .ALTPATCH_CONFIG_MODE       (0 ),
    .USE_PIPELINED_MULTIPLIER   (1 )
) u_IKAOPLL (
	.i_XIN_EMUCLK			(bus_Slot.clock		), //emulator master clock, same as XIN
	.o_XOUT					(),
	.i_phiM_PCEN_n			(`LOW				), //phiM positive edge clock enable(negative logic)
	.i_IC_n					(bus_Slot.reset_n	),
	.i_ALTPATCH_EN			(`LOW				),
	.i_CS_n					(~ff_OPLL_CS		),
	.i_WR_n					(~ff_OPLL_WR		),
	.i_A0					(ff_opll_A0			),
	.i_D					(ff_opll_data		),
	.o_D					(), 				//YM2413 uses only two LSBs
	.o_D_OE					(),
	.o_DAC_EN_MO			(),
	.o_DAC_EN_RO			(),
	.o_IMP_NOFLUC_SIGN		(),
	.o_IMP_NOFLUC_MAG		(),
	.o_IMP_FLUC_SIGNED_MO	(),					// signed [8:0]
	.o_IMP_FLUC_SIGNED_RO	(),					// signed [8:0]
    .i_ACC_SIGNED_MOVOL		(opll_movol		),
    .i_ACC_SIGNED_ROVOL		(opll_rovol		),
	.o_ACC_SIGNED_STRB		(opll_STRB		),
	.o_ACC_SIGNED			(opll_OUT		)	// signed [15:0]
);

wire opll_STRB;
reg [1:0] opll_STRB_sft;
reg signed	[15:0]	opll_OUT;

always_ff @(posedge i_CLK) begin
	if(!i_RST_n) begin
		opll_movol <= 5'sd4;
		opll_rovol <= -5'sd6;
		opll_STRB_sft <= 2'b0;
	end
	else begin
		opll_STRB_sft <= {opll_STRB_sft[0], opll_STRB};
		if( opll_STRB_sft == 2'b01 )
			bus_Sound.OPLL <= opll_OUT;
	end
end


//-----------------------------------------------------------------------
// SCC(IKASCC)
//-----------------------------------------------------------------------
wire busy_s1_p2 = `LOW;
wire w_IKASCC_CS_n = ~sel_s1_p2;
reg ff_IKASCC_CS_n;
reg ff_IKASCC_RD_n;
reg ff_IKASCC_WR_n;
wire [7:0] w_IKASCC_ABLO = bus_Slot.a[7:0];
wire [4:0] w_IKASCC_ABHI = bus_Slot.a[15:11];
wire [7:0] w_IKASCC_RD_DATA;
reg [7:0] ff_IKASCC_WR_DATA;
reg [3:0] ff_IKASCC_RISE_CNT;
reg		  	scc_wr_delay_on;
reg	[4:0]	z80_f_cnt;
wire		w_IKASCC_DB_OE;

assign bus_Slot.read_d	= (w_IKASCC_DB_OE) ? w_IKASCC_RD_DATA: 8'bz;

always_ff@(posedge i_CLK ) begin
	if( !i_RST_n ) begin
		ff_IKASCC_CS_n <= `HIGH;
		ff_IKASCC_RD_n <= `HIGH;
		ff_IKASCC_WR_n <= `HIGH;
		ff_IKASCC_RISE_CNT <= 4'b000;
		scc_wr_delay_on <= `LOW;
		z80_f_cnt <= 5'd0;
	end
	else begin
		if( scc_wr_delay_on ) begin		// WRを、CSより1cyc遅れてアサートさせる
			scc_wr_delay_on <= `LOW;
			ff_IKASCC_WR_n <= `LOW;
		end
		if( ff_IKASCC_CS_n ) begin
			if( !w_IKASCC_CS_n ) begin
				ff_IKASCC_CS_n <= w_IKASCC_CS_n;
				ff_IKASCC_RISE_CNT <= 4'b001;
				ff_IKASCC_RD_n <= ~bus_Slot.rd;
				if( bus_Slot.wr ) begin
				 	scc_wr_delay_on <= `HIGH;
					ff_IKASCC_WR_DATA <= bus_Slot.write_d;
				 end
			end
			else begin
				ff_IKASCC_CS_n <= ff_IKASCC_CS_n;
			end;
		end
		else begin
			z80_f_cnt <= z80_f_cnt + 5'd01;
			//if( z80_f_cnt == 5'd19 ) begin	// for Z80 3.58MHz
			//if( z80_f_cnt == 5'd9 ) begin		// for Z80 7.16MHz
			if( z80_f_cnt == i_masicn_IKASCC ) begin
				z80_f_cnt <= 5'd0;
				ff_IKASCC_RISE_CNT <= {ff_IKASCC_RISE_CNT[2:0], 1'b0};
				if( ff_IKASCC_RISE_CNT[0] ) begin
					ff_IKASCC_WR_n <= `HIGH;
					ff_IKASCC_RD_n <= `HIGH;
				end
				if( ff_IKASCC_RISE_CNT[1] ) begin
					ff_IKASCC_CS_n <= `HIGH;
				end
			end
		end
	end
end

IKASCC #(
	.IMPL_TYPE(2),	// 2 : async,3.58MHz  
	.RAM_BLOCK(1)
) u_IKASCC (
	.i_EMUCLK		(bus_Slot.clock		),
	.i_MCLK_PCEN_n	(1'b0				),
	.i_RST_n		(bus_Slot.reset_n	),
	.i_CS_n			(ff_IKASCC_CS_n		),
	.i_RD_n			(ff_IKASCC_RD_n		),
	.i_WR_n			(ff_IKASCC_WR_n		),
	.i_ABLO			(w_IKASCC_ABLO		),
	.i_ABHI			(w_IKASCC_ABHI		),
	.i_DB			(ff_IKASCC_WR_DATA	),
	.o_DB			(w_IKASCC_RD_DATA	),
	.o_DB_OE		(w_IKASCC_DB_OE		),
	.o_ROMCS_n		(					),
	.o_ROMADDR		(					),
	.o_SOUND		(bus_Sound.IKASCC	),	// signed[10:0]
	.o_TEST			(					)
);

// ==========================================================================
// 基本スロットのページ選択
// ==========================================================================
wire io_acc = ((bus_Slot.a[7:0]==8'hA8)&&bus_Slot.iorq);
reg [7:0] resIoData8;
assign bus_Slot.read_d = (io_acc && bus_Slot.rd) ? resIoData8 : 8'bz;

// RegBaseSlot[0]: PageNo(0-3) of Slot-0
// RegBaseSlot[1]: PageNo(0-3) of Slot-1
// RegBaseSlot[2]: PageNo(0-3) of Slot-2
// RegBaseSlot[3]: PageNo(0-3) of Slot-3
reg	[1:0] RegBaseSlot[3:0];	

always_ff @(posedge i_CLK) begin
	if(!i_RST_n) begin
		resIoData8 <= 8'h00;
		{RegBaseSlot[3],RegBaseSlot[2],RegBaseSlot[1],RegBaseSlot[0]} <= 8'b00_00_00_00;	// デフォルト
	end
	else begin
		// 基本スロット選択レジスタ(A8h)へのアクセス
		if( io_acc ) begin
			if( bus_Slot.wr) begin
				{RegBaseSlot[3],RegBaseSlot[2],RegBaseSlot[1],RegBaseSlot[0]} <= bus_Slot.write_d;
			end
			else if( bus_Slot.rd ) begin
				resIoData8 <= {RegBaseSlot[3],RegBaseSlot[2],RegBaseSlot[1],RegBaseSlot[0]};
			end
		end
	end
end

// ==========================================================================
// Slot #3 は、16KBx4 RAM
// ==========================================================================
wire [13:0]	addrInPage = bus_Slot.a[13:0];
wire [1:0]	pgNo = bus_Slot.a[15:14];
wire sel_s3_p0 = (bus_Slot.merq && RegBaseSlot[0] == 2'd3 && pgNo == 0 );
wire sel_s3_p1 = (bus_Slot.merq && RegBaseSlot[1] == 2'd3 && pgNo == 1 );
wire sel_s3_p2 = (bus_Slot.merq && RegBaseSlot[2] == 2'd3 && pgNo == 2 );
wire sel_s3_p3 = (bus_Slot.merq && RegBaseSlot[3] == 2'd3 && pgNo == 3 );
//
wire sel_s1_p1 = (bus_Slot.merq && RegBaseSlot[1] == 2'd1 && pgNo == 1 );	// FM-BIOS
wire sel_s1_p2 = (bus_Slot.merq && RegBaseSlot[2] == 2'd1 && pgNo == 2 );	// IKASCC
//
wire busy_s3_p0;
wire busy_s3_p1;
wire busy_s3_p2;
wire busy_s3_p3;
wire busy_s1_p1;

// --------------------------------------------------------------------------
Ram16kUnit #(
    .TOP_RAM_ADDR(8'b0000_0000)	// page-0用のトップメモリアドレス
) u_Ram16k_s3_p0
(
	.i_CLK			(i_CLK				),
	.i_RST_n		(i_RST_n			),
//
	.i_EN			(sel_s3_p0			),	// セレクト信号
	.i_MEM_ADDR14	(addrInPage			),	// アドレス(14bit)
	.i_MEM_WR8		(bus_Slot.wr		),	// メモリ書き込み
	.i_MEM_RD8		(bus_Slot.rd		),	// メモリ読み込み
	.i_MEM_DATA8	(bus_Slot.write_d	),	// 書き込みデータ
	.o_MEM_DATA8	(bus_Slot.read_d	),	// 読み込みデータ
	.o_MEM_BUSY		(busy_s3_p0			),	// 処理中
//
	.bus_Pamux		(bus_Pamux	)
);

// --------------------------------------------------------------------------
Ram16kUnit #(
    .TOP_RAM_ADDR(8'b0000_0001)	// page-1用のトップメモリアドレス
) u_Ram16k_s3_p1
(
	.i_CLK			(i_CLK				),
	.i_RST_n		(i_RST_n			),
//
	.i_EN			(sel_s3_p1			),	// セレクト信号
	.i_MEM_ADDR14	(addrInPage			),	// アドレス(14bit)
	.i_MEM_WR8		(bus_Slot.wr		),	// メモリ書き込み
	.i_MEM_RD8		(bus_Slot.rd		),	// メモリ読み込み
	.i_MEM_DATA8	(bus_Slot.write_d	),	// 書き込みデータ
	.o_MEM_DATA8	(bus_Slot.read_d	),	// 読み込みデータ
	.o_MEM_BUSY		(busy_s3_p1			),	// 処理中
//
	.bus_Pamux		(bus_Pamux	)
);

// --------------------------------------------------------------------------
Ram16kUnit #(
    .TOP_RAM_ADDR(8'b0000_0010)	// page-2用のトップメモリアドレス
) u_Ram16k_s3_p2
(
	.i_CLK			(i_CLK				),
	.i_RST_n		(i_RST_n			),
//
	.i_EN			(sel_s3_p2			),	// セレクト信号
	.i_MEM_ADDR14	(addrInPage			),	// アドレス(14bit)
	.i_MEM_WR8		(bus_Slot.wr		),	// メモリ書き込み
	.i_MEM_RD8		(bus_Slot.rd		),	// メモリ読み込み
	.i_MEM_DATA8	(bus_Slot.write_d	),	// 書き込みデータ
	.o_MEM_DATA8	(bus_Slot.read_d	),	// 読み込みデータ
	.o_MEM_BUSY		(busy_s3_p2			),	// 処理中
//
	.bus_Pamux		(bus_Pamux	)
);

// --------------------------------------------------------------------------
Ram16kUnit #(
    .TOP_RAM_ADDR(8'b0000_0011)	// page-2用のトップメモリアドレス
) u_Ram16k_s3_p3
(
	.i_CLK			(i_CLK				),
	.i_RST_n		(i_RST_n			),
//
	.i_EN			(sel_s3_p3			),	// セレクト信号
	.i_MEM_ADDR14	(addrInPage			),	// アドレス(14bit)
	.i_MEM_WR8		(bus_Slot.wr		),	// メモリ書き込み
	.i_MEM_RD8		(bus_Slot.rd		),	// メモリ読み込み
	.i_MEM_DATA8	(bus_Slot.write_d	),	// 書き込みデータ
	.o_MEM_DATA8	(bus_Slot.read_d	),	// 読み込みデータ
	.o_MEM_BUSY		(busy_s3_p3			),	// 処理中
//
	.bus_Pamux		(bus_Pamux	)
);

// --------------------------------------------------------------------------
FmBiosUnit u_FmBios_s1_p2 (
	.i_CLK			(i_CLK				),
	.i_RST_n		(i_RST_n			),
//
	.i_EN			(sel_s1_p1			),	// セレクト信号
	.i_MEM_ADDR14	(addrInPage			),	// アドレス(14bit)
	.i_MEM_RD8		(bus_Slot.rd		),	// メモリ読み込み
	.o_MEM_DATA8	(bus_Slot.read_d	),	// 読み込みデータ
	.o_MEM_BUSY		(busy_s1_p1			)	// 処理中
);

endmodule
`default_nettype wire

