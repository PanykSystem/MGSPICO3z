`default_nettype none
`include "if_harz80.svh"

module TxWorkRam
(
	input	wire				i_CLK,
	input	wire				i_RST_n,
	//
	input	wire				i_R1_WR,			// in CPU clk
	input	wire [7:0]			i_R1_WR_DATA,
	input	wire				i_COPY,
	//
	input	wire				i_R2_RD,			// in i_CLK
	input	wire [6:0]			i_R2_RD_INDEX,
	output	wire [7:0]			o_R2_OUT_DATA
);

parameter [6:0] RAMSIZE = 47;
reg [7:0]	ram [1:0][RAMSIZE-1:0];
reg 		bank_R1;
reg 		bank_R2;
reg [2:0]	wr_sft;
reg [2:0]	copy_sft;
reg [6:0]	index_wr_ram1;
reg [7:0]	out_data;
reg 		req_swap;
assign o_R2_OUT_DATA = out_data;

always_ff@(posedge i_CLK) begin
	integer i1;
	if (!i_RST_n) begin
		wr_sft <= 3'b0;
		copy_sft <= 3'b0;
		index_wr_ram1 <= 7'd0;
		out_data <= 8'd0;
		req_swap <= `LOW;
		bank_R1 <= 1'b0;
		bank_R2 <= 1'b1;
	end
	else begin
		// RAM1への書き込み(i_WRの立ち上がりエッジのみ)
		wr_sft <= {wr_sft[1:0], i_R1_WR};
		if (wr_sft == 3'b011) begin
			ram[bank_R1][index_wr_ram1] <= i_R1_WR_DATA;
			index_wr_ram1 <= index_wr_ram1 + 7'd1;
		end
		// RAM2からの読み込み
		if (i_R2_RD) begin
			out_data <= ram[bank_R2][i_R2_RD_INDEX];
		end
		//
		copy_sft <= {copy_sft[1:0], i_COPY};
		if (copy_sft == 3'b011) begin
			index_wr_ram1 <= 7'd0;
			req_swap <= `HIGH;

		end
		else if (req_swap && !i_R2_RD) begin
			// 読み／書き領域の入れ替え
			req_swap <= `LOW;
			bank_R1 <= ~bank_R1;
			bank_R2 <= ~bank_R2;
		end
	end
end

endmodule
`default_nettype wire

// // WRWKRAMの構造 46 bytes
// struct WRWKRAM
// {
//  	uint8_t	update_counter;		// 更新される度にインクリメントされる
//  	uint8_t ccmd_cnt;			// CCMDを受領した回数。リセット直後は0であるが、CCMD受領のたびにインクリメントされ255の次は0ではなく1に循環する
//  	uint8_t	loop_time;			// メインループの所要時間[100us]単位。ただし上限25.5[ms]
//		uint32_t play_time;			// 再生経過時間 16.6ms毎にインクリメントされる32ビット整数、再生していないときは0
//		uint8_t type;				// 0:MGS
// 	struct MGSINFO
// 	{
// 		uint8_t	FM_SLT;			// MSX-MUSICのスロット番号、無しならFFH
// 		uint8_t	SC_SLT;			// SCC音源のスロット番号、無しならFFH
// 		uint8_t MAXCHN;			// 15 or 17
// 		uint8_t LOOPCT;			// ループ回数(演奏開始直後は0)
// 		uint8_t	PLAYFG;			// 現在演奏中のトラック数
// 		int16_t	GATETIME[17];	// GATEタイムカウンタ(signed int16)
// 	};
// };


