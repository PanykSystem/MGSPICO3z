`ifndef PsramAccessMUX_svh
`define PsramAccessMUX_svh

`default_nettype none

interface pamux_if;
	psram_addr_t	address;
	logic			write;
	logic			read;
	logic [7:0]		write_data;
	logic [7:0]		read_data;
	logic			busy;
	modport client(
		output	address,
		output	write,
		output	read,
		output	write_data,
		input	read_data,
		input	busy
	);
	modport host(
		input	address,
		input	write,
		input	read,
		input	write_data,
		output	read_data,
		output	busy
	);
endinterface

`default_nettype wire
`endif // PsramAccessMUX_svh