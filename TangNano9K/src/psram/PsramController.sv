//
// PSRAM/HyperRAM controller for Tang Nano 9K / Gowin GW1NR-LV9QN88PC6/15.
// Feng Zhou, 2022.8
//
// This is a word or byte based, non-bursting controller for accessing the on-chip HyperRAM.
// - 1:1 clock design. Memory and main logic work under the same clock.
// - Low latency. Under default settings, write latency is 7 cycles (1x) or 10 cycles (2x). 
//   Read latency is 12 cycles (1x) or 15 cycles(2x). In my test, 2x latency happens about 
//   0.05% of time.

//`include "PsramController.svh"

module PsramController #(
    parameter FREQ=74_250_000,// Actual i_CLK frequency, to time 150us initialization delay
    parameter LATENCY=3       // tACC (Initial Latency) in W955D8MBYA datasheet:
                              // 3 (max 83Mhz), 4 (max 104Mhz), 5 (max 133Mhz) or 6 (max 166Mhz)
) (
    input 				i_CLK,
    input 				i_CLK_p,        // phase-shifted clock for driving O_psram_ck
    input 				i_RST_n,
	//
	psram_ctrl_if.host	bus_PsramCtrl,			//
	psram_if.client		bus_Psram		// 
);

assign bus_Psram.O_psram_reset_n[0] = i_RST_n;
assign bus_Psram.O_psram_reset_n[1] = bus_Psram.O_psram_reset_n[0];

typedef enum logic [3:0]
{
	ST_INIT,
	ST_CONFIG,
	ST_IDLE,
	ST_READ8,	// read 1 byte
	ST_WRITE8	// write 1 byte
} state_t;
state_t state;

reg cfg_now, dq_oen, ram_cs_n, ck_e, ck_e_p;
reg wait_for_rd_data;
reg [7:0]	w_din8;
reg [23:0]	cycles_sr;       // shift register counting cycles
reg [63:0]	dq_sr;           // shifts left 8-bit every cycle

// DDR input output signals
wire [7:0] dq_out_ris = dq_sr[63:56];
wire [7:0] dq_out_fal = dq_sr[55:48];
wire [7:0] dq_in_ris;
wire [7:0] dq_in_fal;
reg rwds_out_ris, rwds_out_fal, rwds_oen;
wire rwds_in_ris, rwds_in_fal;
reg additional_latency;

localparam [3:0] CR_LATENCY =
	(LATENCY == 3) ? 4'b1110 :
	(LATENCY == 4) ? 4'b1111 :
	(LATENCY == 5) ? 4'b0000 :
	(LATENCY == 6) ? 4'b0001 : 4'b1110;

reg			odd_addr;
reg	[15:0]	out_data16;
assign bus_PsramCtrl.read_data	= (odd_addr) ? out_data16[7:0] : out_data16[15:8];
assign bus_PsramCtrl.busy		= (state != ST_IDLE);

// Main FSM for HyperRAM 
always @(posedge i_CLK) begin
    if (~i_RST_n) begin
        state <= ST_INIT;
        ram_cs_n <= 1;
        ck_e <= 0;
		ck_e_p <= 0;
		rwds_oen <= 1;
		dq_oen <= 1;
		dq_sr <= 64'b0;
		cycles_sr <= 0;
		wait_for_rd_data <= 0;
		odd_addr <= 1'b0;
    end
	else begin
		ck_e_p <= ck_e;
		if( dq_oen == 0 ) begin
			dq_sr <= {dq_sr[47:0], 16'b0};          // shift 16-bits each cycle
		end
		if (state == ST_INIT && cfg_now) begin
			cycles_sr <= 24'b1;
			ram_cs_n <= 0;
			state <= ST_CONFIG;
		end
		else case(state)
			ST_CONFIG: begin
				cycles_sr <= {cycles_sr[22:0], 1'b0};
				if (cycles_sr[0]) begin
					dq_sr <= {
						8'b011_00000, 8'h00, 8'h01, 8'h00, 8'h00, 8'h00,
						4'h8,
						4'b1111, // 1111b - Reserved (default)
						CR_LATENCY,
						1'b0,	// Fixed Latency Enable, 0b – Variable Initial Latency – 1 or 2 times Initial Latency depending on RWDS during CA cycles.
						1'b1,	// Burst Type 1b: Wrapped burst sequences in legacy wrapped burst manner (default)
						2'b11	// Burst Length 11b - 32 bytes
					}; 
					dq_oen <= 0;
					ck_e <= 1;      // this needs to be earlier 1 cycle to allow for phase shifted i_CLK_p
				end else if (cycles_sr[4]) begin
					state <= ST_IDLE;
					ck_e <= 0;
					dq_oen <= 1;
					ram_cs_n <= 1;
				end
			end
			ST_IDLE: begin
				if (bus_PsramCtrl.read8 || bus_PsramCtrl.write8) begin
					// start operation
					dq_sr <= {
						!bus_PsramCtrl.write8,
						2'b00,
						11'b00000000000,	// reserved
						bus_PsramCtrl.address[21:4],		// Upper Column Address
						13'b0,				// reserved
						bus_PsramCtrl.address[3:1],		// Lower Column Address
						16'b0000_0100_1101_0100
					};
					odd_addr <= bus_PsramCtrl.address[0];
					dq_oen <= 0;
					ram_cs_n <= 0;
					ck_e <= 1;
					wait_for_rd_data <= 0;
					w_din8 <= bus_PsramCtrl.write_data;
					cycles_sr <= 24'b10;
					state <= (bus_PsramCtrl.write8)  ? ST_WRITE8 : ST_READ8;
				end
				else begin
					ck_e <= 0;
					ram_cs_n <= 1;
					dq_oen <= 1;
					rwds_oen <= 1;
				end
			end
			ST_READ8: begin
				cycles_sr <= {cycles_sr[22:0], 1'b0};
				if (cycles_sr[3]) begin
					dq_oen <= 1;	// command sent, now wait for result
				end 
				if (cycles_sr[9]) begin
					wait_for_rd_data <= 1;
				end
				if (wait_for_rd_data && (rwds_in_ris ^ rwds_in_fal)) begin     // sample rwds falling edge to get a word / \_
					out_data16 <= {dq_in_fal, dq_in_ris};						// 偶数アドレスのbyte, 奇数アドレスのbyte
					ram_cs_n <= 1;
					state <= ST_IDLE;
				end
			end
			ST_WRITE8: begin
				cycles_sr <= {cycles_sr[22:0], 1'b0};
				// Write timing is trickier - we sample RWDS at cycle 5 to determine whether we need to wait another tACC.
				// If it is low, data starts at 2+LATENCY. If high, then data starts at 2+LATENCY*2.
				if (cycles_sr[5]) begin
					additional_latency <= rwds_in_fal;  // sample RWDS to see if we need additional latency
				end
				if ((cycles_sr[2+LATENCY] && ((LATENCY==3) ? ~rwds_in_fal : ~additional_latency)) ||
					 cycles_sr[2+LATENCY*2] )
				begin
					rwds_oen <= 0;
					rwds_out_ris <= ~odd_addr;			// 0=write odd_addr_byte
					rwds_out_fal <= odd_addr;			// 0=write even_addr_byte
					dq_sr[63:48] <= {w_din8,w_din8};	// 63:56=odd_addr_byte, 55:32=even_addr_byte
					state <= ST_IDLE;
				end
			end
		endcase
	end
end


// 150us initialization delay
//
// Generate cfg_now pulse after 150us delay
//
localparam INIT_TIME = FREQ / 1000 * 160 / 1000;
reg  [$clog2(INIT_TIME+1):0]   rst_cnt;
reg rst_done, rst_done_p1;
  
always @(posedge i_CLK) begin
    if (~i_RST_n) begin
        rst_cnt  <= 15'd0;
		rst_done_p1 <= 0;
        rst_done <= 0;
    end
	else begin
		if (rst_cnt != INIT_TIME) begin      // count to 160 us
			rst_cnt  <= rst_cnt[$clog2(INIT_TIME+1):0] + 1'b1;
			rst_done <= 0;
		end else begin
			rst_done <= 1;
		end
	    rst_done_p1 <= rst_done;
    	cfg_now     <= rst_done & ~rst_done_p1;// Rising Edge Detect
	end
end

// Tristate DDR output
ODDR oddr_cs_n(
    .CLK(i_CLK),
	.D0(ram_cs_n),
	.TX(1'b0),
	.D1(ram_cs_n),
	.Q0(cs_n_tbuf),
	.Q1()
);
assign bus_Psram.O_psram_cs_n[0] = cs_n_tbuf;
assign bus_Psram.O_psram_cs_n[1] = 1'b1;

ODDR oddr_rwds(
    .CLK(i_CLK),
	.D0(rwds_out_ris),
	.D1(rwds_out_fal),
	.TX(rwds_oen),
	.Q0(rwds_tbuf),
	.Q1(rwds_oen_tbuf)
);

wire dq_out_tbuf[7:0];
wire dq_oen_tbuf[7:0];
assign bus_Psram.IO_psram_rwds[0] = rwds_oen_tbuf ? 1'bz : rwds_tbuf;
assign bus_Psram.IO_psram_rwds[1] = 1'bz;
genvar i1;
generate
    for (i1=0; i1<=7; i1=i1+1) begin: gen_i1
        ODDR oddr_dq_i1(
            .CLK(i_CLK), .D0(dq_out_ris[i1]), .D1(dq_out_fal[i1]), .TX(dq_oen), .Q0(dq_out_tbuf[i1]), .Q1(dq_oen_tbuf[i1])
        );
        assign bus_Psram.IO_psram_dq[i1] = dq_oen_tbuf[i1] ? 1'bz : dq_out_tbuf[i1];
        assign bus_Psram.IO_psram_dq[i1+8] = 1'bz;
    end
endgenerate

// Note: ck uses phase-shifted clock i_CLK_p
wire ck_tbuf;
ODDR oddr_ck(
	.CLK(i_CLK_p),
	.D0(ck_e_p),
	.D1(1'b0),
	.TX(1'b0),
	.Q0(ck_tbuf) ,
	.Q1() 
);
assign bus_Psram.O_psram_ck[0] = ck_tbuf;
assign bus_Psram.O_psram_ck[1] = 1'bz;
assign bus_Psram.O_psram_ck_n[0] = 1'bz;
assign bus_Psram.O_psram_ck_n[1] = 1'bz;

// Tristate DDR input
IDDR iddr_rwds(
    .CLK(i_CLK), .D(bus_Psram.IO_psram_rwds[0]), .Q0(rwds_in_ris), .Q1(rwds_in_fal)
);
genvar i2;
generate
    for (i2=0; i2<=7; i2=i2+1) begin: gen_i2
        IDDR iddr_dq_i2(
            .CLK(i_CLK), .D(bus_Psram.IO_psram_dq[i2]), .Q0(dq_in_ris[i2]), .Q1(dq_in_fal[i2])
        );
    end
endgenerate

endmodule


