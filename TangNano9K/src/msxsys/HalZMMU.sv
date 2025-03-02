`include "../defines.svh"
`include "../if_harz80.svh"

`default_nettype none
module HarzMMU
(
	input	wire				i_CLK,
	input	wire				i_RST_n,
	//
	harzbus_if.host				bus_Harz,
	z80bus_if.peripheral		bus_Z80,
	pamux_if.client				bus_Pamux,
	txworkram_if.host			bus_TxWork,
	ccmnd_if.host				bus_CCmd,
	//
	// 音声出力
	soundbus_if.src				bus_Sound,
	//
	// TangNano9K OnBoard LEDx6, `LOW=Turn-On
	output	reg[5:0]			o_LED

);

reg [15:0]	harz_address;
reg [7:0]	harz_write_data;
reg 		harz_rd;
reg 		harz_wr;
reg 		harz_merq;
reg 		harz_iorq;
reg 		harz_busy;

// スロットバスへ
msxslotbus_if	bus_Slot();
assign bus_Slot.clock		= bus_Z80.clk;
assign bus_Slot.reset_n		= i_RST_n;
// assign bus_Slot.iorq		= (ff_Harz) ? harz_iorq		: ~bus_Z80.iorq_n;
// assign bus_Slot.merq		= (ff_Harz) ? harz_merq		: ~bus_Z80.mreq_n;
// assign bus_Slot.wr			= (ff_Harz) ? harz_wr		: ~bus_Z80.wr_n;
// assign bus_Slot.rd			= (ff_Harz) ? harz_rd		: ~bus_Z80.rd_n;
// assign bus_Slot.a			= (ff_Harz) ? harz_address	: bus_Z80.A;
// assign bus_Slot.write_d		= (ff_Harz) ? harz_write_data: bus_Z80.dout;

always_ff @(posedge i_CLK) begin
	bus_Slot.iorq		<= (ff_Harz) ? harz_iorq		: ~bus_Z80.iorq_n;
	bus_Slot.merq		<= (ff_Harz) ? harz_merq		: ~bus_Z80.mreq_n;
	bus_Slot.wr			<= (ff_Harz) ? harz_wr			: ~bus_Z80.wr_n;
	bus_Slot.rd			<= (ff_Harz) ? harz_rd			: ~bus_Z80.rd_n;
	bus_Slot.a			<= (ff_Harz) ? harz_address		: bus_Z80.A;
	bus_Slot.write_d	<= (ff_Harz) ? harz_write_data: bus_Z80.dout;
end


// CPUへ
assign bus_Z80.int_n		= `HIGH;
assign bus_Z80.nmi_n		= `HIGH;
assign bus_Z80.wait_n		= ~(bus_Slot.busy|m1_busy);
assign bus_Z80.di			= bus_Slot.read_d;
assign bus_Z80.busrq_n		= 1'bz;

//
//assign bus_Harz.read_data	= bus_Slot.read_d;
//assign bus_Harz.busy		= harz_busy;

// reg	[1:0]	z80_clk_sft;
// always_ff @(posedge i_CLK) begin
// 	if (!i_RST_n) begin
// 		z80_clk_sft <= 5'b0;
// 	end
// 	else begin
// 		z80_clk_sft <= {z80_clk_sft[0], bus_Z80.clk};
// 		if( z80_clk_sft == 2'b01 ) begin
// 			bus_Harz.read_data <= bus_Slot.read_d;
// 		end
// 	end
// end

//-----------------------------------------------------------------------
// @brief M1サイクル用WAIT信号の生成回路
// @note M1サイクル開始から clk_Z80CPUで2クロック分 wait_n(~m1_busy) をアサートする
//		これによりCPUは1クロック分、WAIT期間を挿入する
//		どうもtv80sはT2の立下り時ではなく、T3の立上り時にwait_nをみているようだ
//		tv80nを使用した場合はそのタイミングが異なるのだろうか、未調査である
reg [2:0]	m1_busy_sft;
reg 		m1_busy;
always_ff @(negedge bus_Z80.clk) begin	// ※ negedge
	if (!bus_Z80.reset_n) begin
		m1_busy_sft <= 3'b0;
		m1_busy <= 1'b0;
	end
	else begin
		m1_busy <= m1_busy_sft[1] & ~bus_Z80.m1_n;
		if( bus_Z80.m1_n ) begin
			m1_busy_sft <= 3'b111;
		end
		else begin
			m1_busy_sft <= {m1_busy_sft[1:0], 1'b0};
		end
	end
end

//-----------------------------------------------------------------------
// SLOTS&PAGES
BasicSlotUnit u_BasicSlotSystem
(
	.i_CLK		(i_CLK			),
	.i_RST_n	(i_RST_n		),
	//
	.bus_Slot	(bus_Slot		),
	.bus_Pamux	(bus_Pamux		),
	.bus_TxWork	(bus_TxWork		),
	.bus_Sound	(bus_Sound		),
	.bus_CCmd	(bus_CCmd		),
	.o_LED		(o_LED			)
);

//-----------------------------------------------------------------------
task conv(
	input harz_req_t	req,
	output	logic		iorq,
	output	logic		merq,
	output	logic		wr,
	output	logic		rd
);
	//
	logic io_W;
	logic io_R;
	logic mem_W;
	logic mem_R;
	io_W = (req==HARZ80_IO_WRITE);
	io_R = (req ==HARZ80_IO_READ);
	mem_W = (req==HARZ80_MEM_WRITE_1);
	mem_R = (req==HARZ80_MEM_READ_1);
	iorq = io_W|io_R;
	merq = mem_W|mem_R;
	wr = io_W|mem_W;
	rd = io_R|mem_R;
endtask
//
typedef enum logic [2:0]
{
	ST_IDLE,
	ST_WAIT,
	ST_DELAY,
	ST_FINISH
} state_t;
state_t state;

reg ff_Harz;

always_ff @(posedge i_CLK) begin
	if (!i_RST_n) begin
		harz_busy	<= `LOW;
		harz_rd		<= `LOW;
		harz_wr		<= `LOW;
		harz_merq	<= `LOW;
		harz_iorq	<= `LOW;
		ff_Harz 	<= `LOW;
		state <= ST_IDLE;
	end
	else begin
		if( state == ST_IDLE ) begin 
			if (bus_Harz.request != HARZ80_NONE) begin
				ff_Harz <= `HIGH;
				harz_write_data <= bus_Harz.write_data;
				harz_address <= bus_Harz.address;
				conv(bus_Harz.request, harz_iorq, harz_merq, harz_wr, harz_rd);
				bus_Harz.busy <= `HIGH;
				state <= ST_WAIT;
			end
		end
		else if( state==ST_WAIT ) begin
			state <= ST_DELAY;
		end
		else if( state==ST_DELAY ) begin	
			if( !bus_Slot.busy ) begin
				harz_wr <= `LOW;
				harz_rd <= `LOW;
				bus_Harz.read_data <= bus_Slot.read_d;
				bus_Harz.busy <= `LOW;
				state <= ST_FINISH;
			end
		end
		else if( state==ST_FINISH ) begin
			ff_Harz <= `LOW;
			harz_iorq <= `LOW;
			harz_merq <= `LOW;
			state <= ST_IDLE;
		end
	end
end


endmodule
`default_nettype wire

