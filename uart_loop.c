#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/io.h>
#include "16c750_support.h"

#define DEFAULT_BPS 115200
#define UART2_BASE_PHYS_ADDR 0x48022000
#define UART2_REG_SIZE 0x1000

static unsigned long bps_rate = DEFAULT_BPS;
module_param(bps_rate, long, 0444);

struct barrometer_data {
    char loopback[16];
    void __iomem *uart2_base; // Virtual Memory Base Address
}; 

static struct barrometer_data barrometer;

static int barrometer_probe(struct platform_device *pdev);
static int barrometer_remove(struct platform_device *pdev);
static int uart_init(void);
static void uart_deinit(void);
static void write_uart_reg_raw(u32 reg_offset, u16 value);
static u16 read_uart_reg_raw(u32 reg_offset);

static struct platform_driver barrometer_driver = {
    .probe = barrometer_probe, 
    .remove = barrometer_remove,
    .driver = {
        .name = "barrometer_uart2",
    },
};

static int uart_init(void) {
    barrometer.uart2_base = ioremap_nocache(UART2_BASE_PHYS_ADDR, UART2_REG_SIZE);
    if (!barrometer.uart2_base) {
        printk(KERN_ERR "Failed to map UART2 Registers \n");
        return -ENOMEM;
    }
    printk(KERN_INFO "uart_loop: UART2 Registers mapped successfully! \n");
    return 0;
}

static void uart_deinit(void) {
    if (barrometer.uart2_base) {
        iounmap(barrometer.uart2_base);
        printk(KERN_INFO "uart_loop: UART2 Registers unmapped successfully! \n");
    }
}

static void write_uart_reg_raw(u32 reg_offset, u16 value) {
    if (barrometer.uart2_base) {
        iowrite16(value, barrometer.uart2_base + reg_offset);
        printk(KERN_INFO "Wrote 0x%04x to UART register at offset 0x%08x \n", value, reg_offset);
    } else {
        printk(KERN_ERR "UART Register is not mapped!!\n");
    }
}

static u16 read_uart_reg_raw(u32 reg_offset) {
    u16 value = 0;

    if (barrometer.uart2_base) {
        value = ioread16(barrometer.uart2_base + reg_offset);
        printk(KERN_INFO "Read 0x%04x from UART register at offset 0x%08x \n", value, reg_offset);
    } else {
        printk(KERN_ERR "UART Register is not mapped!!\n");
    }
    return value;
}

// Initial Function called (__init)
static int __init init_callback_fn(void) {
    printk(KERN_INFO "uart_loop: Module loaded successfully! \n");
    printk(KERN_INFO "BPS_RATE = %lu \n", bps_rate