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
    barrometer.uart2_base = ioremap_nocache(UART2_BASE_PHYS_ADDR, UART2_REG_SIZE); // Maps a physical address range into kernel's virtual memory
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
    printk(KERN_INFO "BPS_RATE = %lu \n", bps_rate);
    return platform_driver_register(&barrometer_driver); // Register the barrometer_driver struct
}

// Last Function called (__exit)
static void __exit exit_callback_fn(void) {
    printk(KERN_INFO "uart_loop: Module unloaded successfully! \n");
    platform_driver_unregister(&barrometer_driver); // Unregister the driver
}

// Read a Device Attribute Value
static ssize_t loopback_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%s\n", barrometer.loopback); // Show loopback status
}

// Write a Device Attribute Value
static ssize_t loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    if (strncmp(buf, "on", 2) == 0) {
        strncpy(barrometer.loopback, "on", sizeof(barrometer.loopback));
    } else if (strncmp(buf, "off", 3) == 0) {
        strncpy(barrometer.loopback, "off", sizeof(barrometer.loopback));
    } else {
        return -EINVAL; // Invalid Argument Error Code
    }

    barrometer.loopback[sizeof(barrometer.loopback) - 1] = '\0'; // Set null character
    return count;
}

// Linking the attribute to the show and store functions
static DEVICE_ATTR(loopback, 0644, loopback_show, loopback_store); // 0644 allows Read and Write

// Binds the driver to the specific hardware device
static int barrometer_probe(struct platform_device *pdev) {
    int retval;
    
    printk(KERN_INFO "uart_loop: Barrometer Probe Function called! \n");
    
    retval = uart_init();
    if (retval) {
        printk(KERN_ERR "Failed to initialize UART2\n");
        return retval;
    }

    retval = device_create_file(&pdev->dev, &dev_attr_loopback); // Create sysfs file
    if (retval) {
        printk(KERN_ERR "uart_loop: Failed to create sysfs file! \n");
        uart_deinit();
        return retval;
    }

    strncpy(barrometer.loopback, "off", sizeof(barrometer.loopback)); // Default loopback status
    printk(KERN_INFO "uart_loop: Driver bound successfully! \n");
    
    return 0;
}

static int barrometer_remove(struct platform_device *pdev) {
    printk(KERN_INFO "uart_loop: Barrometer Remove Function called! \n");
    device_remove_file(&pdev->dev, &dev_attr_loopback); // Remove sysfs file
    uart_deinit();
    printk(KERN_INFO "uart_loop: Driver unbound successfully! \n");
    return 0;
}

module_init(init_callback_fn);
module_exit(exit_callback_fn);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cameron Frohar");
MODULE_DESCRIPTION("Kernel module for UART loopback");
MODULE_VERSION("1.0");