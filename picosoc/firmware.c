#include <stdint.h>
#include <stdbool.h>

#include "docs.h"


// a pointer to this is a null pointer, but the compiler does not
// know that because "sram" is a linker symbol from sections.lds.
extern uint32_t sram;

#define reg_spictrl (*(volatile uint32_t*)0x02000000)
#define reg_uart_clkdiv (*(volatile uint32_t*)0x02000004)
#define reg_uart_data (*(volatile uint32_t*)0x02000008)
#define reg_leds (*(volatile uint32_t*)0x03000000)
#define reg_text ((volatile uint32_t*)0x04000000)
#define reg_i2c ((volatile uint32_t*)0x05010040)
#define reg_ctrl ((volatile uint32_t*)0x05100000)

// --------------------------------------------------------

extern uint32_t flashio_worker_begin;
extern uint32_t flashio_worker_end;

void flashio(uint8_t *data, int len, uint8_t wrencmd)
{
	uint32_t func[&flashio_worker_end - &flashio_worker_begin];

	uint32_t *src_ptr = &flashio_worker_begin;
	uint32_t *dst_ptr = func;

	while (src_ptr != &flashio_worker_end)
		*(dst_ptr++) = *(src_ptr++);

	((void(*)(uint8_t*, uint32_t, uint32_t))func)(data, len, wrencmd);
}

void set_flash_qspi_flag()
{
	uint32_t addr = 0x800002;
	uint8_t buffer_rd[6] = {0x65, addr >> 16, addr >> 8, addr, 0, 0};
	flashio(buffer_rd, 6, 0);

	uint8_t buffer_wr[5] = {0x71, addr >> 16, addr >> 8, addr, buffer_rd[5] | 2};
	flashio(buffer_wr, 5, 0x06);
}

void set_flash_latency(uint8_t value)
{
	reg_spictrl = (reg_spictrl & ~0x007f0000) | ((value & 15) << 16);

	uint32_t addr = 0x800004;
	uint8_t buffer_wr[5] = {0x71, addr >> 16, addr >> 8, addr, 0x70 | value};
	flashio(buffer_wr, 5, 0x06);
}

const int vga_width = 80;
const int vga_height = 50;
const int vga_size = 4000;
#define cursor_x (*(&sram + 0x100))
#define cursor_y (*(&sram + 0x104))
#define vga_colour (*(&sram + 0x108))
#define cursor_addr (*(&sram + 0x10C))

void vga_clear() {
	cursor_x = 0;
	cursor_y = 0;
	cursor_addr = 0;
	vga_colour = 0;
	for(int i = 0; i < vga_size; i++)
		reg_text[i] = 0x00;
}

#define RED 1
#define WHITE 0

void set_colour(int c) {
	vga_colour = c;
}


//temporary toolchain workaround
#define mul_80(x) ((x << 6) + (x << 4))

void vga_newline() {
	if(cursor_y < (vga_height - 1)) {
		cursor_x = 0;
		cursor_y++;
		cursor_addr = mul_80(cursor_y);

	} else {
		vga_clear();
		cursor_y = 0;
		cursor_x = 0;
		cursor_addr = 0;
	}
}

void vga_putch(char c) {
	if((c == '\n') || (cursor_x >= (vga_width - 1))) {
		vga_newline();
	} else {
		reg_text[cursor_addr++] = vga_colour ? (c | 0x80) : (c & 0x7F);
		cursor_x++;
	}
}

// --------------------------------------------------------

void putchar(char c)
{
	if (c == '\n')
		putchar('\r');
	reg_uart_data = c;
	if (c != '\r')
		vga_putch(c);
}

void print(const char *p)
{
	while (*p)
		putchar(*(p++));
}

void print_hex(uint32_t v, int digits)
{
	for (int i = 7; i >= 0; i--) {
		char c = "0123456789abcdef"[(v >> (4*i)) & 15];
		if (c == '0' && i >= digits) continue;
		putchar(c);
		digits = i;
	}
}

void print_dec(uint32_t v)
{
	if (v >= 100) {
		print(">=100");
		return;
	}

	if      (v >= 90) { putchar('9'); v -= 90; }
	else if (v >= 80) { putchar('8'); v -= 80; }
	else if (v >= 70) { putchar('7'); v -= 70; }
	else if (v >= 60) { putchar('6'); v -= 60; }
	else if (v >= 50) { putchar('5'); v -= 50; }
	else if (v >= 40) { putchar('4'); v -= 40; }
	else if (v >= 30) { putchar('3'); v -= 30; }
	else if (v >= 20) { putchar('2'); v -= 20; }
	else if (v >= 10) { putchar('1'); v -= 10; }

	if      (v >= 9) { putchar('9'); v -= 9; }
	else if (v >= 8) { putchar('8'); v -= 8; }
	else if (v >= 7) { putchar('7'); v -= 7; }
	else if (v >= 6) { putchar('6'); v -= 6; }
	else if (v >= 5) { putchar('5'); v -= 5; }
	else if (v >= 4) { putchar('4'); v -= 4; }
	else if (v >= 3) { putchar('3'); v -= 3; }
	else if (v >= 2) { putchar('2'); v -= 2; }
	else if (v >= 1) { putchar('1'); v -= 1; }
	else putchar('0');
}

uint32_t xorshift32(uint32_t *state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

void memtest() {
	int cyc_count = 5;
	uint32_t state;
	for(int i = 1; i <= cyc_count; i++) {
		state = i;
		for(int addr = 1000; addr < 32000; addr++) {
			*(&sram + addr) = xorshift32(&state);
		}
		state = i;
		for(int addr = 1000; addr < 32000; addr++) {
			if(*(&sram + addr) != xorshift32(&state)) {
				print("Memtest ***FAIL*** at ");
				print_hex(4*addr, 4);
				print("\n");
				return;
			};
		}
	}
	print("Memtest pass\n");
}

#define I2CCR1 		(0x08)
#define I2CCMDR 	(0x09)
#define I2CBRLSB 	(0x0A)
#define I2CBRMSB 	(0x0B)
#define I2CSR			(0x0C)
#define I2CTXDR 	(0x0D)
#define I2CRXDR 	(0x0E)
#define I2CGCDR 	(0x0F)
#define I2CSADDR 	(0x03)
#define I2CINTCR 	(0x07)
#define I2CINTSR 	(0x06)

#define BMP085_ADDRESS 0x77

void i2c_wait() {
	while((reg_i2c[I2CSR] & 0x04) == 0) ;

}

void i2c_wait_srw() {
	while((reg_i2c[I2CSR] & 0x10) == 0) ;
}

void i2c_begin(uint8_t addr, bool is_read) {
	reg_i2c[I2CTXDR] = (addr << 1) | (is_read & 0x01);
	reg_i2c[I2CCMDR] = 0x94;
	reg_i2c[I2CCMDR] = 0x0;
	if(is_read) {
		i2c_wait_srw();
		reg_i2c[I2CCMDR] = 0x24;

	} else {
		i2c_wait();
	}
}

void i2c_write(uint8_t data) {
	reg_i2c[I2CTXDR] = data;
	reg_i2c[I2CCMDR] = 0x14;
	i2c_wait();
	reg_i2c[I2CCMDR] = 0x0;
}

void i2c_stop() {
	reg_i2c[I2CCMDR] = 0x44;
}

void i2c_init() {
	reg_i2c[I2CCR1] = 0x80;
	uint16_t prescale = 50;
	reg_i2c[I2CBRMSB] = (prescale >> 8) & 0xFF;
	reg_i2c[I2CBRLSB] = prescale & 0xFF;
}




void delay_cyc(uint32_t cycles) {
	uint32_t cycles_begin, cycles_now;
	__asm__ volatile ("rdcycle %0" : "=r"(cycles_begin));
	do {
		__asm__ volatile ("rdcycle %0" : "=r"(cycles_now));
	} while((cycles_now - cycles_begin) < cycles);
	
}

uint8_t i2c_read(bool is_last) {
	if(is_last) {
		uint8_t data = reg_i2c[I2CRXDR];
		delay_cyc(300);
		reg_i2c[I2CCMDR] = 0x6C;
		i2c_wait();
		//uint8_t dummy = reg_i2c[I2CRXDR];
		return data;
	} else {
		i2c_wait();
		return reg_i2c[I2CRXDR] & 0xFF;
	}
}

uint8_t bmp085Read(uint8_t address)
{
	i2c_begin(BMP085_ADDRESS, false);
	i2c_write(address);
	i2c_begin(BMP085_ADDRESS, true);
	return i2c_read(true);
}

int bmp085ReadInt(uint8_t address)
{
	i2c_begin(BMP085_ADDRESS, false);
	i2c_write(address);
	i2c_begin(BMP085_ADDRESS, true);
	uint16_t msb = i2c_read(false);
	uint16_t lsb = i2c_read(true);
	//uint16_t msb = bmp085Read(address);
	//uint16_t lsb = bmp085Read(address+1);
	return (int16_t)((msb << 8) | lsb);
}

int16_t bmp085ReadUT()
{
  int16_t ut;
  
  // Write 0x2E into Register 0xF4
  // This requests a temperature reading
  i2c_begin(BMP085_ADDRESS, false);
  i2c_write(0xF4);
  i2c_write(0x2E);
  i2c_stop();
  
  // Wait at least 4.5ms
  delay_cyc(20*12000);
  
  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

#define OSS 0

uint32_t bmp085ReadUP()
{
  unsigned char msb, lsb, xlsb;
  uint32_t up = 0;
  
  // Write 0x34+(OSS<<6) into register 0xF4
  // Request a pressure reading w/ oversampling setting
	i2c_begin(BMP085_ADDRESS, false);
  i2c_write(0xF4);
  i2c_write(0x34 + OSS);
  i2c_stop();
  
  // Wait for conversion, delay time dependent on OSS
  delay_cyc(24000 + (36000<<OSS));
  
  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  i2c_begin(BMP085_ADDRESS, false);
  i2c_write(0xF6);
  i2c_stop();
  i2c_begin(BMP085_ADDRESS, true);
  
  msb = i2c_read(false);
  lsb = i2c_read(false);
  xlsb = i2c_read(true);
  
  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);
  
  return up;
}


void print_dec_ex(int i) {
	char buf[10];
	char *ptr = buf;
	if(i < 0) {
		i = -i;
		print("-");
	}
	if(i == 0) {
		print("0");
	}
	while(i > 0) {
		*(ptr++) = '0' + (i % 10);
		i /= 10;
	}
	char rev[10];
	char *rptr = rev;
	while(ptr != buf) {
		*(rptr++) = *(--ptr);
	}
	*rptr = '\0';
	print(rev);
}

void i2c_test() {

	i2c_init();

	// Calibration values
	int ac1;
	int ac2; 
	int ac3; 
	unsigned int ac4;
	unsigned int ac5;
	unsigned int ac6;
	int b1; 
	int b2;
	int mb;
	int mc;
	int md;

	// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
	// so ...Temperature(...) must be called before ...Pressure(...).
	long b5; 
	ac1 = bmp085ReadInt(0xAA);
	ac2 = bmp085ReadInt(0xAC);
	ac3 = bmp085ReadInt(0xAE);
	ac4 = (uint16_t)bmp085ReadInt(0xB0);
	ac5 = (uint16_t)bmp085ReadInt(0xB2);
	ac6 = (uint16_t)bmp085ReadInt(0xB4);
	b1 = bmp085ReadInt(0xB6);
	b2 = bmp085ReadInt(0xB8);
	mb = bmp085ReadInt(0xBA);
	mc = bmp085ReadInt(0xBC);
	md = bmp085ReadInt(0xBE);
	
	uint16_t ut = bmp085ReadUT();
	print("ac1 = 0x");
	print_hex(ac1, 4);
	print("\n");
	
	print("ac2 = 0x");
	print_hex(ac2, 4);
	print("\n");
	
	print("ac3 = 0x");
	print_hex(ac3, 4);
	print("\n");
	
	print("ut = 0x");
	print_hex(ut, 4);
	print("\n");
	
	long x1, x2;

	x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
	x2 = ((long)mc << 11)/(x1 + md);
	b5 = x1 + x2;

	int temp = ((b5 + 8)>>4);  
	print("temp = ");
	print_dec(temp / 10);
	print(".");
	print_dec(temp % 10);
	print("C\n");
	
	uint32_t up = bmp085ReadUP();
	
	long x3, b3, b6, p;
	unsigned long b4, b7;

	b6 = b5 - 4000;
	// Calculate B3
	x1 = (b2 * (b6 * b6)>>12)>>11;
	x2 = (ac2 * b6)>>11;
	x3 = x1 + x2;
	b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;

	// Calculate B4
	x1 = (ac3 * b6)>>13;
	x2 = (b1 * ((b6 * b6)>>12))>>16;
	x3 = ((x1 + x2) + 2)>>2;
	b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;

	b7 = ((unsigned long)(up - b3) * (50000>>OSS));
	if (b7 < 0x80000000)
		p = (b7<<1)/b4;
	else
		p = (b7/b4)<<1;
		
	x1 = (p>>8) * (p>>8);
	x1 = (x1 * 3038)>>16;
	x2 = (-7357 * p)>>16;
	p += (x1 + x2 + 3791)>>4;
		
	print("press = ");
	print_dec_ex(p);
	print("Pa\n");
}

char getchar_prompt(char *prompt)
{
	int32_t c = -1;

	uint32_t cycles_begin, cycles_now, cycles;
	__asm__ volatile ("rdcycle %0" : "=r"(cycles_begin));

	if (prompt)
		print(prompt);

	reg_leds = ~0;
	while (c == -1) {
		__asm__ volatile ("rdcycle %0" : "=r"(cycles_now));
		cycles = cycles_now - cycles_begin;
		if (cycles > 12000000) {
			if (prompt)
				print(prompt);
			cycles_begin = cycles_now;
			reg_leds = ~reg_leds;
		}
		c = reg_uart_data;
	}
	reg_leds = 0;
	return c;
}

char getchar()
{
	return getchar_prompt(0);
}

// --------------------------------------------------------

void cmd_read_flash_id()
{
	uint8_t buffer[17] = { 0x9F, /* zeros */ };
	flashio(buffer, 17, 0);

	for (int i = 1; i <= 16; i++) {
		putchar(' ');
		print_hex(buffer[i], 2);
	}
	putchar('\n');
}

// --------------------------------------------------------

uint8_t cmd_read_flash_regs_print(uint32_t addr, const char *name)
{
	set_flash_latency(8);

	uint8_t buffer[6] = {0x65, addr >> 16, addr >> 8, addr, 0, 0};
	flashio(buffer, 6, 0);

	print("0x");
	print_hex(addr, 6);
	print(" ");
	print(name);
	print(" 0x");
	print_hex(buffer[5], 2);
	print("\n");

	return buffer[5];
}

void cmd_read_flash_regs()
{
	print("\n");
	uint8_t sr1v = cmd_read_flash_regs_print(0x800000, "SR1V");
	uint8_t sr2v = cmd_read_flash_regs_print(0x800001, "SR2V");
	uint8_t cr1v = cmd_read_flash_regs_print(0x800002, "CR1V");
	uint8_t cr2v = cmd_read_flash_regs_print(0x800003, "CR2V");
	uint8_t cr3v = cmd_read_flash_regs_print(0x800004, "CR3V");
	uint8_t vdlp = cmd_read_flash_regs_print(0x800005, "VDLP");
}

// --------------------------------------------------------

uint32_t cmd_benchmark(bool verbose, uint32_t *instns_p)
{
	uint8_t data[256];
	uint32_t *words = (void*)data;

	uint32_t x32 = 314159265;

	uint32_t cycles_begin, cycles_end;
	uint32_t instns_begin, instns_end;
	__asm__ volatile ("rdcycle %0" : "=r"(cycles_begin));
	__asm__ volatile ("rdinstret %0" : "=r"(instns_begin));

	for (int i = 0; i < 20; i++)
	{
		for (int k = 0; k < 256; k++)
		{
			x32 ^= x32 << 13;
			x32 ^= x32 >> 17;
			x32 ^= x32 << 5;
			data[k] = x32;
		}

		for (int k = 0, p = 0; k < 256; k++)
		{
			if (data[k])
				data[p++] = k;
		}

		for (int k = 0, p = 0; k < 64; k++)
		{
			x32 = x32 ^ words[k];
		}
	}

	__asm__ volatile ("rdcycle %0" : "=r"(cycles_end));
	__asm__ volatile ("rdinstret %0" : "=r"(instns_end));

	if (verbose)
	{
		print("Cycles: 0x");
		print_hex(cycles_end - cycles_begin, 8);
		putchar('\n');

		print("Instns: 0x");
		print_hex(instns_end - instns_begin, 8);
		putchar('\n');

		print("Chksum: 0x");
		print_hex(x32, 8);
		putchar('\n');
	}

	if (instns_p)
		*instns_p = instns_end - instns_begin;

	return cycles_end - cycles_begin;
}

// --------------------------------------------------------

void cmd_benchmark_all()
{
	uint32_t instns = 0;

	print("default        ");
	reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00000000;
	print(": ");
	print_hex(cmd_benchmark(false, &instns), 8);
	putchar('\n');

	for (int i = 8; i > 0; i--)
	{
		print("dspi-");
		print_dec(i);
		print("         ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00400000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	for (int i = 8; i > 0; i--)
	{
		print("dspi-crm-");
		print_dec(i);
		print("     ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00500000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	for (int i = 8; i > 0; i--)
	{
		print("qspi-");
		print_dec(i);
		print("         ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00200000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	for (int i = 8; i > 0; i--)
	{
		print("qspi-crm-");
		print_dec(i);
		print("     ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00300000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	for (int i = 8; i > 0; i--)
	{
		print("qspi-ddr-");
		print_dec(i);
		print("     ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00600000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	for (int i = 8; i > 0; i--)
	{
		print("qspi-ddr-crm-");
		print_dec(i);
		print(" ");

		set_flash_latency(i);
		reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00700000;

		print(": ");
		print_hex(cmd_benchmark(false, &instns), 8);
		putchar('\n');
	}

	print("instns         : ");
	print_hex(instns, 8);
	putchar('\n');
}

void read_docs() {
	vga_clear();
	const char *docs_pos = docs_text;
	while(*docs_pos != '\0') {
		if((cursor_y == (vga_height - 1)) && ((cursor_x == (vga_width - 1)) || (*docs_pos == '\n'))) {
			//Wait at end of page
			char c = getchar();
			if(c == 0x04) break; //Ctrl+D to exit
		}
		vga_putch(*(docs_pos++));
	}
	vga_clear();
}

// --------------------------------------------------------

void main()
{
	vga_clear();
	reg_uart_clkdiv = 139;
	//set_flash_qspi_flag();

	while (getchar_prompt("Press ENTER to continue..\n") != '\r') { /* wait */ }

	print("\n");
	print("  ____  _          ____         ____\n");
	print(" |  _ \\(_) ___ ___/ ___|  ___  / ___|\n");
	print(" | |_) | |/ __/ _ \\___ \\ / _ \\| |\n");
	print(" |  __/| | (_| (_) |__) | (_) | |___\n");
	print(" |_|   |_|\\___\\___/____/ \\___/ \\____|\n");

	while (1)
	{
		print("\n");
		print("\n");
		print("SPI State:\n");

		print("  LATENCY ");
		print_dec((reg_spictrl >> 16) & 15);
		print("\n");

		print("  DDR ");
		if ((reg_spictrl & (1 << 22)) != 0)
			print("ON\n");
		else
			print("OFF\n");

		print("  QSPI ");
		if ((reg_spictrl & (1 << 21)) != 0)
			print("ON\n");
		else
			print("OFF\n");

		print("  CRM ");
		if ((reg_spictrl & (1 << 20)) != 0)
			print("ON\n");
		else
			print("OFF\n");

		print("\n");
		print("Select an action:\n");
		print("\n");
		print("   [1] Read SPI Flash ID\n");
		print("   [2] Read SPI Config Regs\n");
		print("   [3] Switch to default mode\n");
		print("   [4] Switch to Dual I/O mode\n");
		print("   [5] Switch to Quad I/O mode\n");
		print("   [6] Switch to Quad DDR mode\n");
		print("   [7] Toggle continuous read mode\n");
		print("   [9] Run simplistic benchmark\n");
		print("   [0] Benchmark all configs\n");
		print("   [M] Run SPRAM test\n");
		print("   [I] Run I2C test\n");

		print("\n");

		for (int rep = 10; rep > 0; rep--)
		{
			set_colour(RED);
			print("Command> ");
			set_colour(WHITE);

			char cmd = getchar();
			if (cmd > 32 && cmd < 127)
				putchar(cmd);
			print("\n");

			switch (cmd)
			{
			case '1':
				cmd_read_flash_id();
				break;
			case '2':
				cmd_read_flash_regs();
				break;
			case '3':
				reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00000000;
				break;
			case '4':
				reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00400000;
				break;
			case '5':
				reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00200000;
				break;
			case '6':
				reg_spictrl = (reg_spictrl & ~0x00700000) | 0x00600000;
				break;
			case '7':
				reg_spictrl = reg_spictrl ^ 0x00100000;
				break;
			case '9':
				cmd_benchmark(true, 0);
				break;
			case '0':
				cmd_benchmark_all();
				break;
			case 'M':
				memtest();
				break;
			case 'I':
				i2c_test();
				break;
			default:
				continue;
			}

			break;
		}
	}
}

