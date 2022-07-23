
#define UART_RHR_REG	0x000
#define UART_THR_REG	0x000
#define UART_IER_REG	0x004
#define UART_IIR_REG	0x008
#define UART_FCR_REG	0x008
#define UART_LCR_REG	0x00C
#define UART_MCR_REG	0x010
#define UART_LSR_REG	0x014
#define UART_MSR_REG	0x018
#define UART_SPR_REG	0x01C
#define UART_MDR1_REG	0x020
#define UART_MDR2_REG	0x024
#define UART_MDR3_REG	0x080
#define UART_SYSC_REG	0x054
#define UART_SYSS_REG	0x058
#define UART_RXFIFO_LVL_REG	0x064
#define UART_TXFIFO_LVL_REG	0x068
#define UART_DLL_REG	0x100
#define UART_DLH_REG	0x104
#define UART_EFR_REG	0x208

// This function writes a 16-bit value to the UART register with byte offset reg_offset 
static void write_uart_reg_raw (u32 reg_offset, u16 value);

// This function reads a 16-bit value from the UART register with the byte offset reg_offset
static u16 read_uart_reg_raw (u32 reg_offset);

static void uart_deinit(void);

int init_uart_reg (void);

static u16 cached_lcr = 0;

void write_uart_reg (u32 reg, u16 value)
{
        u16 lcr_selector_index = (reg / 256) % 3;
        u16 reg_offset = (reg % 256);

        if (lcr_selector_index == 1)
                write_uart_reg_raw (UART_LCR_REG, cached_lcr | 0x80);
        else if (lcr_selector_index == 2)
                write_uart_reg_raw (UART_LCR_REG, 0xBF);

        write_uart_reg_raw (reg_offset, value);
        if (reg_offset == UART_LCR_REG)
                cached_lcr = value;

        if (lcr_selector_index != 0)
                write_uart_reg_raw (UART_LCR_REG, cached_lcr);
}

u16 read_uart_reg (u32 reg)
{
        u16 value = 0;
        u16 lcr_selector_index = (reg / 256) % 3;
        u16 reg_offset = (reg % 256);

        if (lcr_selector_index == 1)
                write_uart_reg_raw (UART_LCR_REG, cached_lcr | 0x80);
        else if (lcr_selector_index == 2)
                write_uart_reg_raw (UART_LCR_REG, 0xBF);

        value = read_uart_reg_raw (reg_offset);

        if (lcr_selector_index != 0)
                write_uart_reg_raw (UART_LCR_REG, cached_lcr);

        return value;
}

int init_uart_reg (void)
{
	uint32_t val;
        write_uart_reg_raw(UART_LCR_REG, cached_lcr);

	write_uart_reg(UART_SYSC_REG,0x0002);

	do {
		val = read_uart_reg(UART_SYSC_REG);
	} while (!(val & 0x01)); // Wait for bit 0


	write_uart_reg(UART_MCR_REG,0x0010);
	write_uart_reg(UART_FCR_REG,0x0007);
	write_uart_reg(UART_LCR_REG,0x0003);
	write_uart_reg(UART_DLH_REG,0x0001);
	write_uart_reg(UART_MCR_REG,0x0017);
	write_uart_reg(UART_MDR1_REG,0x0000);

	val = read_uart_reg(UART_MCR_REG);
	printk(KERN_INFO "UART MCR Register Value = 0x%04x\n", val);
	
	if(val != 0x0017) {
		printk(KERN_ERR "UART MCR Register Verification Failed \n");
		return -EIO;
	}

	write_uart_reg(UART_THR_REG,0x0041);

	do {
		val = read_uart_reg(UART_RXFIFO_LVL_REG);
	} while ((val & 0x1F) == 0); // Wait for non-zero

	val = read_uart_reg(UART_RHR_REG);
	printk(KERN_INFO "Received value from RX FIFO = 0x%04x\n", val & 0x00FF);

	if((val & 0x00FF) != 0x41) {
		printk(KERN_ERR "UART MCR Register Verification Failed \n");
		uart_deinit();
		return -EIO;
	}



	printk(KERN_INFO "UART MCR Register Verification Complete!!");


	return 0;



}

