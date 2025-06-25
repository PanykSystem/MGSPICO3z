//Copyright (C)2014-2024 Gowin Semiconductor Corporation.
//All rights reserved.
//File Title: IP file
//Tool Version: V1.9.9.03 (64-bit)
//Part Number: GW1NR-LV9QN88PC6/I5
//Device: GW1NR-9
//Device Version: C
//Created Time: Sat Jun 14 18:43:22 2025

module Gowin_DQCE (clkout, clkin, ce);

output clkout;
input clkin;
input ce;

DQCE dqce_inst (
    .CLKOUT(clkout),
    .CLKIN(clkin),
    .CE(ce)
);

endmodule //Gowin_DQCE
