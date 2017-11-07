// Simple VGA text console
// (C) 2017 David Shah

// Top level wrapping vgacon, PLL, CROM and TRAM

module vgacon_top(input sysclk,
                  input resetn,
                  input [10:0] sys_addr, //System TRAM interface
                  input [31:0] sys_write_data,
                  input [3:0] sys_wren,
                  output vga_r,
                  output vga_g,
                  output vga_b,
                  output vga_hsync_n,
                  output vga_vsync_n,
                  output vga_pixck_dbg);

wire vgaclk, vga_rstn, pll_locked;

vga_pll pll_inst(
    .clock_in(sysclk),
    .clock_out(vgaclk),
    .locked(pll_locked));
    
assign vga_rstn = resetn;
assign vga_pixck_dbg = vgaclk;

wire [12:0] vga_tram_addr;
wire [7:0] vga_tram_data;
                  
vgacon_tram tram_inst(
    .sys_clk(sysclk),
    .sys_addr(sys_addr),
    .sys_data(sys_write_data),
    .sys_wren(sys_wren), 
                   
    .video_clk(vgaclk),
    .video_addr(vga_tram_addr), 
    .video_data(vga_tram_data));

wire [9:0] crom_addr;
wire [7:0] crom_data;

vgacon_crom crom_inst(
    .clk(vgaclk),
    .addr(crom_addr),
    .data(crom_data));
    
vgacon vga_drv(
    .clk(vgaclk),
    .resetn(vga_rstn),
    .tram_addr(vga_tram_addr),
    .tram_data(vga_tram_data),
    .crom_addr(crom_addr),
    .crom_data(crom_data),
    .vga_r(vga_r),
    .vga_g(vga_g),
    .vga_b(vga_b),
    .vga_hsync_n(vga_hsync_n),
    .vga_vsync_n(vga_vsync_n));
                  
endmodule 

// The main VGA console driver

module vgacon(input clk, //25MHz pixel clock in
              input resetn,
              output [12:0] tram_addr, //address output to text RAM, organised as a 80x50 array
              input  [7:0] tram_data, //text ram data, organised as 1 bit (MSB) color toggle and 7 bit ASCII char
              output [9:0] crom_addr, //address output to character data ROM, 7 MSBs char and 3 LSBs y-offset
              input  [7:0] crom_data, //data input from character ROM, LSB is x7 and MSB is x0
              output reg vga_r,
              output reg vga_g,
              output reg vga_b,
              output reg vga_hsync_n,
              output reg vga_vsync_n);
              
            
parameter h_visible = 10'd640;
parameter h_front = 10'd16;
parameter h_sync = 10'd96;
parameter h_back = 10'd48;
parameter h_total = h_visible + h_front + h_sync + h_back;

parameter v_visible = 10'd480;
parameter v_front = 10'd10;
parameter v_sync = 10'd2;
parameter v_back = 10'd33;
parameter v_total = v_visible + v_front + v_sync + v_back;

reg [9:0] h_pos = 0;
reg [9:0] v_pos = 0;

wire h_active, v_active, visible;

wire [6:0] text_x = h_pos[9:3];
wire [6:0] text_y = v_pos[9:3];

wire [2:0] char_x = h_pos[2:0];
wire [2:0] char_y = (h_pos > h_active) ? (v_pos[2:0] + 1) : (v_pos[2:0]); //preload correct data before start of new line

reg [7:0] tram_data_reg;
reg [7:0] crom_data_reg;

reg [1:0] char_colour_reg; //delay char colour bit equally to CROM data

parameter ainc_to_valid = 1 + 1 + 1 + 1 + 1; //latency from incremeting TRAM addr to data change
wire incr_tram_addr = visible && (char_x == (8 - ainc_to_valid));

reg [12:0] tram_addr_reg;

always @(posedge clk) 
begin
  if (resetn == 0) begin
    h_pos <= 10'b0;
    v_pos <= 10'b0;
    tram_addr_reg <= 8'b0;
    tram_data_reg <= 8'b0;
    char_colour_reg <= 2'b0;
    tram_addr_reg <= 8'b0;
  end else begin
    //Pixel counters
    if (h_pos == h_total - 1) begin
      h_pos <= 0;
      if (v_pos == v_total - 1) begin
        v_pos <= 0;
      end else begin
        v_pos <= v_pos + 1;
      end
    end else begin
      h_pos <= h_pos + 1;
    end
    //TRAM address control
    if (!v_active)
      tram_addr_reg <= 0;
    else if(incr_tram_addr)
      tram_addr_reg <= tram_addr_reg + 1;
    //Data registers
    tram_data_reg <= x"39";
    char_colour_reg[0] <= tram_data_reg[7];
    char_colour_reg[1] <= char_colour_reg[0];
    crom_data_reg <= crom_data;
    //Output registers
    vga_hsync_n <= !((h_pos >= (h_visible + h_front)) && (h_pos < (h_visible + h_front + h_sync)));
    vga_vsync_n <= !((v_pos >= (v_visible + v_front)) && (v_pos < (v_visible + v_front + v_sync)));
    vga_r <= current_pixel;
    vga_g <= current_pixel;
    vga_b <= current_pixel;

  end
end


assign h_active = (h_pos < h_visible);
assign v_active = (v_pos < v_visible);
assign visible = h_active && v_active;

assign tram_addr = tram_addr_reg;
assign crom_addr = {tram_data_reg[6:0], char_y[2:0]}; //height 
 
wire in_footer = (text_y >= 50);

wire current_pixel = visible && (crom_data_reg[7 - char_x] && (!in_footer));

endmodule