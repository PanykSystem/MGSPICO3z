
`timescale 1ns/1ps
module tb () ;
    reg clk ; 
    reg rst_n ; 
    reg [(16 - 1):0] I_audio_d = 0 ; 
    reg I_validity_bit ; 
    reg I_user_bit ; 
    reg I_chan_status_bit ; 
    wire O_audio_d_req ; 
    wire O_validity_bit_req ; 
    wire O_user_bit_req ; 
    wire O_chan_status_bit_req ; 
    wire O_block_start_flag ; 
    wire O_sub_frame0_flag ; 
    wire O_sub_frame1_flag ; 
    wire O_Spdif_tx_data ; 
    GSR GSR (.GSRI(1'b1)) ; 
    always
        #(1000) clk = (~clk) ;
    initial
        begin
            clk = 0 ;
            rst_n = 0 ;
            #(2000) rst_n = 1 ;
        end
    always
        @(posedge clk)
        begin
            I_audio_d = ({$random} % 16777216) ;
        end
    always
        @(negedge O_validity_bit_req)
        I_validity_bit = 0 ;
    always
        @(negedge O_user_bit_req)
        I_user_bit = 1 ;
    always
        @(negedge O_chan_status_bit_req)
        I_chan_status_bit = 0 ;
    SPDIF_TX_Top u1 (.I_rst_n(rst_n), .I_clk(clk), .I_audio_d(I_audio_d), .I_validity_bit(I_validity_bit), .I_user_bit(I_user_bit), .I_chan_status_bit(I_chan_status_bit), .O_audio_d_req(O_audio_d_req), .O_validity_bit_req(O_validity_bit_req), .O_user_bit_req(O_user_bit_req), .O_chan_status_bit_req(O_chan_status_bit_req), .O_block_start_flag(O_block_start_flag), .O_sub_frame0_flag(O_sub_frame0_flag), .O_sub_frame1_flag(O_sub_frame1_flag), .O_Spdif_tx_data(O_Spdif_tx_data)) ; 
    initial
        begin
            #(35000000) ;
            $stop  ;
        end
endmodule


