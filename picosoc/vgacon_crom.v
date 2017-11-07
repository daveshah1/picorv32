//Character ROM for the VGA console
module vgacon_crom(input clk,
                   input [9:0] addr,
                   output reg [7:0] data);
                   
reg [7:0] rom[1023:0];

initial begin
    $readmemh("./font_8x8.dat", rom);
end

always @(posedge clk)
begin
  data <= rom[addr];
end
           
endmodule