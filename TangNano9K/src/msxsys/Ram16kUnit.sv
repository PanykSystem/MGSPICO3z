`include "../defines.svh"

`default_nettype none
module Ram16kUnit #(
    parameter [7:0] TOP_RAM_ADDR = 8'b0000_0000
) (
	input	wire			i_CLK,
	input	wire			i_RST_n,
//
	input	wire			i_EN,			// セレクト信号
	input	wire [13:0]		i_MEM_ADDR14,	// アドレス(14bit)
	input	wire			i_MEM_WR8,		// メモリ書き込み
	input	wire			i_MEM_RD8,		// メモリ読み込み
	input	wire [7:0]		i_MEM_DATA8,	// 書き込みデータ
	output	wire [7:0]		o_MEM_DATA8,	// 読み込みデータ
	output	wire 			o_MEM_BUSY,		// 処理中
//
	pamux_if.client			bus_Pamux
);

typedef enum logic [2:0]
{
	STATE_IDLE,
	STATE_WAIT_FOR_WR,
	STATE_WAIT_FOR_WR_DONE,
	STATE_WAIT_FOR_RD,
	STATE_WAIT_FOR_RD_DONE,
	STATE_NEXT_STANDBY
} state_t;
state_t state;

//
logic [7:0]	resData8;
reg resBusy;
assign o_MEM_DATA8	= (i_EN&&i_MEM_RD8) ? resData8: 8'bz;
assign o_MEM_BUSY	= resBusy;
//
//
logic [21:0]memCtrlAddr22;
logic [7:0]	memCtrlWrData8;
logic		memCtrlWR8;
logic		memCtrlRD8;
logic 		gateMemCtrl;
assign bus_Pamux.address	= (gateMemCtrl) ? memCtrlAddr22 	: 22'bz;
assign bus_Pamux.write 		= (gateMemCtrl) ? memCtrlWR8 		: 1'bz;
assign bus_Pamux.read		= (gateMemCtrl) ? memCtrlRD8		: 1'bz;
assign bus_Pamux.write_data	= (gateMemCtrl) ? memCtrlWrData8	: 8'bz;

always @(posedge i_CLK) begin
	if(!i_RST_n) begin
		gateMemCtrl <= `LOW;
		resData8 <= 8'hCC;
		memCtrlWR8 <= `LOW;
		memCtrlRD8 <= `LOW;
		state <= STATE_IDLE;
		resBusy <= `LOW;
	end
	else begin
		if( !i_EN ) begin
			state <= STATE_IDLE;
			gateMemCtrl <= `LOW;
			resBusy <= `LOW;
		end
		else begin
			case (state) 
				STATE_IDLE: begin
					if( i_MEM_WR8 ) begin
						gateMemCtrl <= `HIGH;
						memCtrlAddr22 <= {TOP_RAM_ADDR, i_MEM_ADDR14};
						memCtrlWrData8 <= i_MEM_DATA8;
						memCtrlWR8 <= `HIGH;
						resBusy <= `HIGH;
						state <= STATE_WAIT_FOR_WR;
					end
					else if( i_MEM_RD8 ) begin
						gateMemCtrl <= `HIGH;
						memCtrlAddr22 <= {TOP_RAM_ADDR, i_MEM_ADDR14};
						memCtrlRD8 <= `HIGH;
						resBusy <= `HIGH;
						state <= STATE_WAIT_FOR_RD;
					end
				end
				STATE_WAIT_FOR_WR: begin
					memCtrlWR8 <= `LOW;
					state <= STATE_WAIT_FOR_WR_DONE;
				end
				STATE_WAIT_FOR_WR_DONE: begin
					if( !bus_Pamux.busy ) begin
						resBusy <= `LOW;
						gateMemCtrl <= `LOW;
						state <= STATE_NEXT_STANDBY;
					end
				end
				STATE_WAIT_FOR_RD: begin
					memCtrlRD8 <= `LOW;
					state <= STATE_WAIT_FOR_RD_DONE;
				end
				STATE_WAIT_FOR_RD_DONE: begin
					if( !bus_Pamux.busy ) begin
						resData8 <= bus_Pamux.read_data;
						resBusy <= `LOW;
						gateMemCtrl <= `LOW;
						state <= STATE_NEXT_STANDBY;
					end
				end
				STATE_NEXT_STANDBY: begin
					// (!i_EN)へ遷移するのを待つ
				end
				default: begin
					// do nothing
				end
			endcase
		end
	end
end

endmodule
`default_nettype wire

