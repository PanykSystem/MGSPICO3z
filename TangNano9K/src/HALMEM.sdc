//Copyright (C)2014-2025 GOWIN Semiconductor Corporation.
//All rights reserved.
//File Title: Timing Constraints file
//Tool Version: V1.9.9.03 (64-bit) 
//Created Time: 2025-02-08 21:22:57
create_clock -name i_CLK_3M579 -period 279.408 -waveform {0 139.704} [get_ports {i_CLK_3M579}]
create_clock -name i_CLK_3M072 -period 325.521 -waveform {0 162.761} [get_ports {i_CLK_3M072}]
create_generated_clock -name clk_71M7 -source [get_ports {i_CLK_3M579}] -master_clock i_CLK_3M579 -multiply_by 20 [get_ports {o_CLK_71M7}]
create_generated_clock -name ff_IKASCC_WR_n -source [get_ports {i_CLK_3M579}] -master_clock i_CLK_3M579 -multiply_by 1 [get_nets {u_HarzMMU/u_BasicSlotSystem/ff_IKASCC_WR_n}]
set_clock_groups -asynchronous -group [get_clocks {i_CLK_3M072}]
set_clock_groups -asynchronous -group [get_clocks {i_CLK_3M579}]
