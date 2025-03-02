`ifndef PSRAMCONTROLLER_SVH
`define PSRAMCONTROLLER_SVH
`default_nettype none


interface psram_if;
	logic [1:0] 	O_psram_ck;
	logic [1:0] 	O_psram_ck_n;
	logic [1:0] 	IO_psram_rwds;
	logic [15:0] 	IO_psram_dq;
	logic [1:0] 	O_psram_reset_n;
	logic [1:0] 	O_psram_cs_n;
	modport client(
		output	O_psram_ck,
		output	O_psram_ck_n,
		inout	IO_psram_rwds,
		inout	IO_psram_dq,
		output	O_psram_reset_n,
		output	O_psram_cs_n
	);
endinterface

interface psram_ctrl_if;
	psram_addr_t	address;
	logic			read8;
	logic			write8;
	logic [7:0]		write_data;
	logic [7:0]		read_data;
	logic			busy;
	modport client(
		output	address,
		output	read8,
		output	write8,
		output	write_data,
		input	read_data,
		input	busy
	);
	modport host(
		input	address,
		input	read8,
		input	write8,
		input	write_data,
		output	read_data,
		output	busy
	);
endinterface


`default_nettype wire
`endif // PSRAMCONTROLLER_SVH