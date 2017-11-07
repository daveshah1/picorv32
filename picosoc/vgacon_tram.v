//Text RAM for the VGA console
module vgacon_tram(input sys_clk, //system side clock
                   input [10:0] sys_addr, //system side address (word address)
                   input [31:0] sys_data, //system side write data
                   input [3:0] sys_wren, //bytewise wren
                   
                   input video_clk, //video side clock
                   input [12:0] video_addr, 
                   output [7:0] video_data);

parameter WORDS = 2 ** 11;

reg [31:0] mem [0:WORDS-1];

always @(posedge sys_clk) begin
	if (sys_wren[0]) mem[sys_addr][ 7: 0] <= sys_data[ 7: 0];
	if (sys_wren[1]) mem[sys_addr][15: 8] <= sys_data[15: 8];
	if (sys_wren[2]) mem[sys_addr][23:16] <= sys_data[23:16];
	if (sys_wren[3]) mem[sys_addr][31:24] <= sys_data[31:24];
end
        
reg [1:0] sel_byte;
reg [31:0] int_rdata;

always @(posedge video_clk) begin
  int_rdata <= mem[video_addr[12:2]];
  sel_byte <= video_addr[1:0];
end     
                   
assign video_data = sel_byte[1] ? (sel_byte[0] ? int_rdata[31:24] : int_rdata[23:16]) : (sel_byte[0] ? int_rdata[15:8] : int_rdata[7:0]);                   
endmodule