`include "../defines.svh"

`default_nettype none
module FmBiosUnit (
	input	wire			i_CLK,
	input	wire			i_RST_n,
//
	input	wire			i_EN,			// セレクト信号
	input	wire [13:0]		i_MEM_ADDR14,	// アドレス(14bit)
	input	wire			i_MEM_RD8,		// メモリ読み込み
	output	wire [7:0]		o_MEM_DATA8,	// 読み込みデータ
	output	wire 			o_MEM_BUSY		// 処理中
);

//
reg [7:0]	resData8;
assign o_MEM_DATA8	= (i_EN&&i_MEM_RD8) ? resData8: 8'bz;
assign o_MEM_BUSY	= `LOW;

always @(posedge i_CLK) begin
	if(!i_RST_n) begin
		resData8 <= 8'b0;
	end
	else begin
		if( i_EN ) begin
			case (i_MEM_ADDR14)
				14'b00_0000_0001_1000:	resData8 <= 8'h41;	// A
				14'b00_0000_0001_1001:	resData8 <= 8'h50;	// P
				14'b00_0000_0001_1010:	resData8 <= 8'h52;	// R
				14'b00_0000_0001_1011:	resData8 <= 8'h4c;	// L
				14'b00_0000_0001_1100:	resData8 <= 8'h4f;	// O
				14'b00_0000_0001_1101:	resData8 <= 8'h50;	// P
				14'b00_0000_0001_1110:	resData8 <= 8'h4c;	// L
				14'b00_0000_0001_1111:	resData8 <= 8'h4c;	// L
				default:				resData8 <= 8'h00;
			endcase
		end
	end
end

endmodule
`default_nettype wire

