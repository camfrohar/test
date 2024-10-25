#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#include "16c750_support.h"

#define DEFAULT_BPS 115200
#define UART2_BASE_PHYS_ADDR 0x48024000
#define UART2_REG_SIZE 0x1000
#define UART_IER_RHRIT 0x01
#define UART_MDR1_DISABLE 0x07 // MDR1: 0000000000000111
#define UART_IER_OFFSET 0x6c
#define UART_MDR1_OFFSET 0x20

static unsigned long bps_rate = DEFAULT_BPS;
module_param(bps_rate, long, 0444);
static int uart2_irq;

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
        printk(KERN_ERR "Failed to map UART2 Registers\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "uart_loop: UART2 Registers mapped successfully!\n");
    return 0;
}

static void uart_deinit(void) {
    if (barrometer.uart2_base) {
        iounmap(barrometer.uart2_base);
        printk(KERN_INFO "uart_loop: UART2 Registers unmapped successfully!\n");
    }
}

static void write_uart_reg_raw(u32 reg_offset, u16 value) {
    if (barrometer.uart2_base) {
        iowrite16(value, barrometer.uart2_base + reg_offset);
        printk(KERN_INFO "Wrote 0x%04x to UART register at offset 0x%08x\n", value, reg_offset);
    } else {
        printk(KERN_ERR "UART Register is not mapped!\n");
    }
}

static u16 read_uart_reg_raw(u32 reg_offset) {
    u16 value = 0;
    if (barrometer.uart2_base) {
        value = ioread16(barrometer.uart2_base + reg_offset);
        printk(KERN_INFO "Read 0x%04x from UART register at offset 0x%08x\n", value, reg_offset);
    } else {
        printk(KERN_ERR "UART Register is not mapped!\n");
    }
    return value;
}

static void enable_uart2_interrupts(void __iomem *base) {
    u32 ier_val = readl(base + UART_IER_OFFSET);
    ier_val |= UART_IER_RHRIT;
    writel(ier_val, base + UART_IER_OFFSET);
    printk(KERN_INFO "uart_loop: UART2 interrupt receive enabled\n");
}

static void disable_uart2_interrupts(void __iomem *base) {
    u32 ier_val = readl(base + UART_IER_OFFSET);
    ier_val &= ~UART_IER_RHRIT;
    writel(ier_val, base + UART_IER_OFFSET);
    writel(UART_MDR1_DISABLE, base + UART_MDR1_OFFSET);
    printk(KERN_INFO "uart_loop: UART2 interrupt receive disabled\n");
}

static int barrometer_probe(struct platform_device *pdev) {
    int retval;

    printk(KERN_INFO "uart_loop: Barrometer Probe Function called!\n");

    retval = uart_init();
    if (retval) {
        printk(KERN_ERR "Failed to initialize UART2\n");
        return retval;
    }

    enable_uart2_interrupts(barrometer.uart2_base);

    uart2_irq = 74;

    retval = request_threaded_irq(uart2_irq, uart2_isr, uart2_ist, 0, "uart_loop", &barrometer);
    if (retval) {
        printk(KERN_ERR "Failed to request IRQ for UART2\n");
        uart_deinit();
        return retval;
    }

    printk(KERN_INFO "uart_loop: Requested IRQ for UART2.\n");

    retval = init_uart_reg(); // Call to initialize the UART registers
    if (retval) {
        printk(KERN_ERR "Failed to initialize UART registers\n");
        free_irq(uart2_irq, &barrometer);
        uart_deinit();
        return retval;
    }
    
    uart_arb_device.parent = &pdev->dev;

    retval = misc_register(&uart_arb_device);
    if (retval) {
        printk(KERN_ERR "Failed to register character Device\n");
        free_irq(uart2_irq, &barrometer);
        uart_deinit();
        return retval;
    }

    retval = device_create_file(&pdev->dev, &dev_attr_loopback);
    if (retval) {
        printk(KERN_ERR "uart_loop: Failed to create sysfs file!\n");
        misc_deregister(&uart_arb_device);
        free_irq(uart2_irq, &barrometer);
        uart_deinit();
        return retval;
    }

    strncpy(barrometer.loopback, "off", sizeof(barrometer.loopback));
    printk(KERN_INFO "uart_loop: Driver bound successfully!\n");

    return 0;
}

static int barrometer_remove(struct platform_device *pdev) {
    printk(KERN_INFO "uart_loop: Barrometer Remove Function called!\n");

    disable_uart2_interrupts(barrometer.uart2_base);
    free_irq(uart2_irq, &barrometer);
    device_remove_file(&pdev->dev, &dev_attr_loopback);
    misc_deregister(&uart_arb_device);
    uart_deinit();

    printk(KERN_INFO "uart_loop: Driver unbound successfully!\n");
    return 0;
}

static irqreturn_t uart2_isr(int irq, void *dev_id) {
    printk(KERN_INFO "uart_loop: ISR called for UART2.\n");
    return IRQ_NONE;
}

static irqreturn_t uart2_ist(int irq, void *dev_id) {
    printk(KERN_INFO "uart_loop: IST called for UART2.\n");
    return IRQ_NONE;
}

static int barrometer_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "barrometer: device opened\n");
    return 0;
}

static int barrometer_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "barrometer: device closed\n");
    return 0;
}

static ssize_t barrometer_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    uint8_t val = read_uart_reg_raw(UART_RHR_REG);
    int retval = copy_to_user(buf, &val, sizeof(val));
    if (retval != 0) {
        printk(KERN_ERR "barrometer: failed to copy data to user space.\n");
        return -EFAULT;
    }

    printk(KERN_INFO "Done Reading\n");
    return sizeof(val);
}

static ssize_t barrometer_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    uint8_t val;
    int retval;

    if (count > sizeof(val)) {
        printk(KERN_ERR "barrometer: Attempting to write more than one byte\n");
        return -EINVAL;
    }

    retval = copy_from_user(&val, buf, sizeof(val));
    if (retval != 0) {
        printk(KERN_ERR "barrometer: failed to copy data from user space.\n");
        return -EFAULT;
    }

    write_uart_reg_raw(UART_THR_REG, val);
    printk(KERN_INFO "barrometer: Wrote one byte (0x%02x) to the UART transmit FIFO\n", val);

    return sizeof(val);
}

static const struct file_operations barrometer_fops = {
    .read = barrometer_read,
    .write = barrometer_write,
    .open = barrometer_open,
    .release = barrometer_release,
};

static struct miscdevice uart_arb_device = {
    MISC_DYNAMIC_MINOR, "uart_loop", &barrometer_fops
};

module_init(init_callback_fn);
module_exit(exit_callback_fn);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cameron Frohar");
MODULE_DESCRIPTION("Kernel module for UART loopback");
MODULE_VERSION("1.0");