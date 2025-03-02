`include "../defines.svh"
`include "PsramAccessMUX.svh"

`default_nettype none
module PsramAccessMUX (
	pamux_if.host			bus_pri,
	pamux_if.host			bus_sec,
	psram_ctrl_if.client	bus_Psram
);

assign bus_Psram.address =
	(bus_pri.write|bus_pri.read) ? bus_pri.address :
	(bus_sec.write|bus_sec.read) ? bus_sec.address : 22'bz;

assign bus_Psram.write8	= bus_pri.write | bus_sec.write;
assign bus_Psram.read8	= bus_pri.read | bus_sec.read;

assign bus_Psram.write_data = 
	(bus_pri.write) ? bus_pri.write_data :
	(bus_sec.write) ? bus_sec.write_data : 8'bz;

//
assign bus_pri.read_data	= bus_Psram.read_data;
assign bus_pri.busy			= bus_Psram.busy;
// //
assign bus_sec.read_data	= bus_Psram.read_data;
assign bus_sec.busy		= bus_Psram.busy;







endmodule
`default_nettype wire

