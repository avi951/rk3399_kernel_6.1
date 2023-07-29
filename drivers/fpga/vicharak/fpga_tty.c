#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/gpio.h> 
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

static struct tty_driver *fpga_tty_driver;
static struct tty_port port;
struct tty_struct *tty_fpga_nl;

extern void export_netlink_data(char* data_buf, int len);

void send_uart_data(unsigned char data, int len)
{
	if(tty_fpga_nl != NULL){
		tty_insert_flip_char(tty_fpga_nl->port, data, TTY_NORMAL);
		tty_flip_buffer_push(tty_fpga_nl->port);

	}
}
EXPORT_SYMBOL(send_uart_data);

int fpga_uart_open(struct tty_struct* tty)
{
	int success = 1;
	return success;
}

static int fpga_nl_tty_open(struct tty_struct* tty, struct file* file){
	int error = 0;
	tty_fpga_nl = tty;
	if (!fpga_uart_open(tty))
	{
		printk(KERN_ALERT "soft_uart: Device busy.\n");
		error = -ENODEV;
	}
	return error;
} 

static void fpga_nl_tty_close(struct tty_struct *tty, struct file *file){
}

static int fpga_nl_tty_write(struct tty_struct *tty, const unsigned char *buffer, int count){
	char* data_buf = kmalloc(count + 1, GFP_KERNEL);

	data_buf[0] = 2;
	memcpy(data_buf + 1, buffer, count);
	export_netlink_data(data_buf, count + 1);
	kfree(data_buf);
	return count;
} 

static int fpga_nl_write_room(struct tty_struct *tty){
	return 0;
}         

static void fpga_nl_tty_set_termios(struct tty_struct *tty, struct ktermios *old){
} 	

static const struct tty_operations serial_ops = {
	.open = fpga_nl_tty_open,
	.close = fpga_nl_tty_close,
	.write = fpga_nl_tty_write,
	.write_room = fpga_nl_write_room,
	.set_termios = fpga_nl_tty_set_termios
};


static int fpga_nl_tty_probe(void){

	int retval;
	tty_port_init(&port);
	/* allocate the tty driver */
	fpga_tty_driver = tty_alloc_driver(1, TTY_DRIVER_REAL_RAW);
	if (!fpga_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	fpga_tty_driver->owner = THIS_MODULE;
	fpga_tty_driver->driver_name = "tty_fpga";
	fpga_tty_driver->name = "ttyFPGA";
	fpga_tty_driver->major = 0;
	fpga_tty_driver->minor_start = 0;
	fpga_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	fpga_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	fpga_tty_driver->flags = TTY_DRIVER_REAL_RAW;
	fpga_tty_driver->init_termios = tty_std_termios;
	fpga_tty_driver->init_termios.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(fpga_tty_driver, &serial_ops);
	tty_port_link_device(&port, fpga_tty_driver, 0);
	/* register the tty driver */
	retval = tty_register_driver(fpga_tty_driver);
	if (retval) {
		printk(KERN_ERR "failed to register tty driver");
		tty_driver_kref_put(fpga_tty_driver);
		return retval;
	}

	return 0;
}

static void fpga_nl_tty_remove(void){

	tty_unregister_driver(fpga_tty_driver);
	tty_driver_kref_put(fpga_tty_driver);
}

static int __init fpga_nl_tiny_init(void){
	return fpga_nl_tty_probe();
}

static void __exit fpga_nl_tiny_exit(void){
	fpga_nl_tty_remove();
}

module_init(fpga_nl_tiny_init);
module_exit(fpga_nl_tiny_exit);

MODULE_DESCRIPTION("FPGA TTY driver");
MODULE_AUTHOR("Pratiksha0102 <pratikshathanki2501@gmail.com>");
MODULE_LICENSE("GPL v2");
