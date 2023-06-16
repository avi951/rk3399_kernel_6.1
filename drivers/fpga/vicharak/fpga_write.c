#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include "queue.h"

dev_t fpga_write_device_number;
static struct queue queue_tx;

struct fpga_write_dev {
	struct device *dev;
	struct class *cls;
	unsigned major;
	unsigned minor;
	struct cdev *cdev;
};

struct fpga_write_dev *fw_dev;

char get_mask(uint8_t rem) {
	char mask = 0;

	switch(rem) {
		case 1:
			mask = 0b1;
			break;
		case 2:
			mask = 0b11;
			break;
		case 3:
			mask = 0b111;
			break;
		case 4:
			mask = 0b1111;
			break;
		case 5:
			mask = 0b11111;
			break;
		default:
			mask = 0b1;
			break;
	}

	return mask;
}

static ssize_t buffer_read(struct file *filp, char __user *data, size_t count,
		loff_t *f_ops) {
	int ret = 0, i;

	for (i = 0; i < count; i++) {
		ret = dequeue_character(&queue_tx, &data[i]);
		if(!ret) {
			data[i] = 0;
		}
	}

	return count;
}

static ssize_t buffer_write(struct file *filp, const char __user *data, size_t count,
		loff_t *f_ops) {
	int i;
	if (count > 1) {
		char* data_buf = kmalloc(count + 17, GFP_KERNEL); 
		uint32_t dlen_recv = count - 1;
		uint8_t rem = dlen_recv % 6;
		uint32_t dlen_packet = rem ? (dlen_recv / 6) + 1 : (dlen_recv / 6);
		char sof[] = { 0xEA, 0xFF, 0x99, 0xDE, 0xAD, 0xFF };
		char eof[] = { 0xEA, 0xFF, 0x99, 0xDE, 0xAD, 0xAA };
		int n_elements = 0;
		char dlen_chars[] = { (dlen_packet >> 16) & 0xFF, (dlen_packet >> 8) & 0xFF, dlen_packet & 0xFF };
		
		// Copy predefined SOF
		memcpy(data_buf, sof, 6);

		// Data byte 0 is APP ID
		data_buf[6] = data[0];

		// Count of MIPI packet
		memcpy(data_buf + 7, dlen_chars, 3);

		// Mask for the last packet
		data_buf[10] = get_mask(rem);

		// Reserved Byte
		data_buf[11] = 0;

		// Copy data to Data buf
		memcpy(data_buf + 12, data + 1, dlen_recv);

		// Copy predefined EOF
		memcpy(data_buf + 12 + dlen_recv, eof, 6);

		for (i = 0; i < count + 18; i++) {
			printk("%02x ", data_buf[i]);
		}

		n_elements = enqueue_string(&queue_tx, data_buf, count + 17);
		if (n_elements != count + 17) {
			printk(KERN_INFO "Copy from user fault error");
			goto err;
		}

		kfree(data_buf);
		return count;
	}

err:
	return -EFAULT;
}

static int buffer_open(struct inode *inode, struct file *filp) {
	return 0;  
}


static int buffer_release(struct inode *inode, struct file *filp) {
	return 0;
}

struct file_operations fpga_write_fops = {
	.owner = THIS_MODULE,
	.write = buffer_write,
	.read = buffer_read,
	.open = buffer_open,
	.release = buffer_release
};

static int fpga_write_probe(void) {
	int ret;

	fw_dev = kzalloc(sizeof(struct fpga_write_dev), GFP_KERNEL);
	if (!fw_dev) {
		printk(KERN_ALERT "Fpga Write memory allocation failed\n");
		return -ENOMEM;
	}

	ret = alloc_chrdev_region(&fpga_write_device_number, 0, 1, "fpga_write");
	if (ret) {
		kfree(fw_dev);
		goto err;
	}

	fw_dev->major = MAJOR(fpga_write_device_number);
	fw_dev->minor = MINOR(fpga_write_device_number);

	fw_dev->cdev = cdev_alloc();
	cdev_init(fw_dev->cdev, &fpga_write_fops);

	fw_dev->cdev->owner = THIS_MODULE;

	ret = cdev_add(fw_dev->cdev, fpga_write_device_number, 1);
	if (ret) {
		printk(KERN_ALERT "Failed to allocate region for fpga_write\n");
		goto err;
	}

	fw_dev->cls = class_create(THIS_MODULE, "fpga_write");
	if (IS_ERR(fw_dev->cls)) {
		pr_err("Error creating fw_dev-> class.\n");
		return PTR_ERR(fw_dev->cls);
	}

	fw_dev->dev = device_create(fw_dev->cls,
			NULL, fpga_write_device_number, (void *)fw_dev, "fpga_write");
	if (!fw_dev->dev)
		return -EIO;
	initialize_queue(&queue_tx);

err:
	return ret;
}

static void fpga_write_remove(void) {
	if (fw_dev) {
		if (fw_dev->cdev) {
			device_destroy(fw_dev->cls,
					MKDEV(fw_dev->major,
						fw_dev->minor));
			cdev_del(fw_dev->cdev);
		}
		if (!IS_ERR(fw_dev->cls))
			class_destroy(fw_dev->cls);
		kfree(fw_dev);
	}

	unregister_chrdev_region(fpga_write_device_number, 1);
}

static int __init fpga_write_device_init(void) {
	return fpga_write_probe();
}

static void __exit fpga_write_device_exit(void) {
	fpga_write_remove();
}

module_init(fpga_write_device_init);
module_exit(fpga_write_device_exit);

MODULE_DESCRIPTION("FPGA Write");
MODULE_AUTHOR("djkabutar <d.kabutarwala@yahoo.com>");
MODULE_LICENSE("GPL v2");
