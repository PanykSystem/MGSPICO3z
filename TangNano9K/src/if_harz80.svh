`ifndef IF_HARZ80_SVH
`define IF_HARZ80_SVH

`default_nettype none

/** Pico-TangNano間の通信コマンド
*/
typedef enum logic [7:0]
{
	TXCMD_NOP			= 8'h00,
	// PSRAM直接アクセス(4MB)
	TXCMD_PSRAM_WR_8	= 8'h01,
	TXCMD_PSRAM_RD_8	= 8'h02,
	// HalzMMUを介したメモリアクセス(64KB)
	TXCMD_Z80MEM_WR_1	= 8'h03,
	TXCMD_Z80MEM_RD_1	= 8'h04,
	// TXCMD_Z80MEM_WR_2	= 8'h05,
	// TXCMD_Z80MEM_RD_2	= 8'h06,
	// TXCMD_Z80MEM_WR_4	= 8'h07,
	// TXCMD_Z80MEM_RD_4	= 8'h08,
	TXCMD_Z80IO_WR		= 8'h09,
	TXCMD_Z80IO_RD		= 8'h0a,
	// Halzの制御
	TXCMD_HARZ_RESET	= 8'h10,
	TXCMD_HARZ_RUN		= 8'h11,
	TXCMD_HARZ_STOP		= 8'h12,
	TXCMD_HARZ_RESUME	= 8'h13,
	//
	TXCMD_HARZ_GETSTS	= 8'h14,		// 動作状態を返す
	TXCMD_HARZ_SETCMD	= 8'h15,
	//
	TXCMD_HARZ_EOT		= 8'hff
} txcmd_t;

typedef enum logic [3:0]
{
	HARZ80_NONE,
	HARZ80_IO_WRITE,			// I/O 書込み
	HARZ80_IO_READ,				// I/O 読込み
	HARZ80_MEM_WRITE_1,
	HARZ80_MEM_READ_1
} harz_req_t;

interface harzbus_if;
	harz_req_t		request;
	logic [15:0]	address;
	logic [7:0]		write_data;
	logic [7:0]		read_data;
	logic 			busy;
	modport client(
		output	request,
		output	address,
		output	write_data,
		input	read_data,
		input	busy
	);
	modport host(
		input	request,
		input	address,
		input	write_data,
		output	read_data,
		output	busy
	);
endinterface


/** スロット・バス
* カードエッジコネクタの制御線に似せている
*/
interface msxslotbus_if;
	logic			clock;
	logic			reset_n;
//	logic [3:0]		sltsl;
	logic			iorq;
	logic			merq;
	logic			wr;
	logic			rd;
	logic [15:0]	a;
	logic [7:0]		write_d;
	logic [7:0]		read_d;
	logic			busy;
	modport client(
		output	clock,
		output	reset_n,
//		output	sltsl,
		output	iorq,
		output	merq,
		output	wr,
		output	rd,
		output	a,
		output	write_d,
		input	read_d,
		input	busy
	);
	modport host(
		input	clock,
		input	reset_n,
//		input	sltsl,
		input	iorq,
		input	merq,
		input	wr,
		input	rd,
		input	a,
		input	write_d,
		output	read_d,
		output	busy
	);
endinterface

interface z80bus_if;
	logic			clk;
	logic			bus_clk;
	logic			reset_n;
	logic			wait_n;
	logic			int_n;
	logic			nmi_n;
	logic			busrq_n;
	logic	[7:0]	di;
	logic			m1_n;
	logic			mreq_n;
	logic			iorq_n;
	logic			rd_n;
	logic			wr_n;
	logic			rfsh_n;
	logic			halt_n;
	logic			busak_n;
	logic	[15:0]	A;
	logic	[7:0]	dout;
	modport cpu(
		input	clk,
		input	bus_clk,
		input	reset_n,
		input	wait_n,
		input	int_n,
		input	nmi_n,
		input	busrq_n,
		input	di,
		output	m1_n,
		output	mreq_n,
		output	iorq_n,
		output	rd_n,
		output	wr_n,
		output	rfsh_n,
		output	halt_n,
		output	busak_n,
		output	A,
		output	dout
	);
	modport peripheral(
		input	clk,
		input	bus_clk,
		input	reset_n,
		output	wait_n,
		output	int_n,
		output	nmi_n,
		output	busrq_n,
		output	di,
		input	m1_n,
		input	mreq_n,
		input	iorq_n,
		input	rd_n,
		input	wr_n,
		input	rfsh_n,
		input	halt_n,
		input	busak_n,
		input	A,
		input	dout
	);
endinterface

interface soundbus_if;
	logic signed [13:0]	PSG;
	logic signed [15:0]	OPLL;
	logic signed [10:0]	IKASCC;		// IKASCC by イカビク(@RCAVictorCo)
	logic signed [10:0]	WTS;		// WaveTableSound by HRA!(@thara1129)
	modport src(
		output	PSG,
		output	OPLL,
		output	IKASCC,
		output	WTS
	);
	modport dest(
		input	PSG,
		input	OPLL,
		input	IKASCC,
		input	WTS
	);

endinterface

interface txworkram_if;
	logic 		read;
	logic [6:0]	read_index;
	logic [7:0]	read_data;
	modport client(
		output	read,
		output	read_index,
		input	read_data
	);
	modport host(
		input	read,
		input	read_index,
		output	read_data
	);
endinterface

interface ccmnd_if;
	logic 		write;
	logic [7:0]	write_data;
	modport client(
		output	write,
		output	write_data
	);
	modport host(
		input	write,
		input	write_data
	);
endinterface

`default_nettype wire

`endif // IF_HARZ80_SVH