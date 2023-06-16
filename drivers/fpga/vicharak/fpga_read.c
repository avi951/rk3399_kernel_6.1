#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>

dev_t fpga_read_device_number;
extern void fpga_get_data(const unsigned char *, int);

struct fpga_read_dev {
	struct device *dev;
	struct class *cls;
	unsigned major;
	unsigned minor;
	struct cdev *cdev;
};

struct fpga_read_dev *fr_dev;

static ssize_t buffer_read(struct file *filp, char __user *data, size_t count,
		loff_t *f_ops) {
	return count;
}

static ssize_t buffer_write(struct file *filp, const char __user *data, size_t count,
		loff_t *f_ops) {
	fpga_get_data(data, count);
	return count;
}

static int buffer_open(struct inode *inode, struct file *filp) {
	return 0;  
}


static int buffer_release(struct inode *inode, struct file *filp) {
	return 0;
}

struct file_operations fpga_read_fops = {
	.owner = THIS_MODULE,
	.write = buffer_write,
	.read = buffer_read,
	.open = buffer_open,
	.release = buffer_release
};

static int fpga_read_probe(void) {
	int ret;

	fr_dev = kzalloc(sizeof(struct fpga_read_dev), GFP_KERNEL);
	if (!fr_dev) {
		printk(KERN_ALERT "Fpga Write memory allocation failed\n");
		return -ENOMEM;
	}

	ret = alloc_chrdev_region(&fpga_read_device_number, 0, 1, "fpga_read");
	if (ret) {
		kfree(fr_dev);
		goto err;
	}

	fr_dev->major = MAJOR(fpga_read_device_number);
	fr_dev->minor = MINOR(fpga_read_device_number);

	fr_dev->cdev = cdev_alloc();
	cdev_init(fr_dev->cdev, &fpga_read_fops);

	fr_dev->cdev->owner = THIS_MODULE;

	ret = cdev_add(fr_dev->cdev, fpga_read_device_number, 1);
	if (ret) {
		printk(KERN_ALERT "Failed to allocate region for fpga_read\n");
		goto err;
	}

	fr_dev->cls = class_create(THIS_MODULE, "fpga_read");
	if (IS_ERR(fr_dev->cls)) {
		pr_err("Error creating fr_dev-> class.\n");
		return PTR_ERR(fr_dev->cls);
	}

	fr_dev->dev = device_create(fr_dev->cls,
			NULL, fpga_read_device_number, (void *)fr_dev, "fpga_read");
	if (!fr_dev->dev)
		return -EIO;

err:
	return ret;
}

static void fpga_read_remove(void) {
	if (fr_dev) {
		if (fr_dev->cdev) {
			device_destroy(fr_dev->cls,
					MKDEV(fr_dev->major,
						fr_dev->minor));
			cdev_del(fr_dev->cdev);
		}
		if (!IS_ERR(fr_dev->cls))
			class_destroy(fr_dev->cls);
		kfree(fr_dev);
	}

	unregister_chrdev_region(fpga_read_device_number, 1);
}

static int __init fpga_read_device_init(void) {
	return fpga_read_probe();
}

static void __exit fpga_read_device_exit(void) {
	fpga_read_remove();
}

module_init(fpga_read_device_init);
module_exit(fpga_read_device_exit);

MODULE_DESCRIPTION("FPGA Read");
MODULE_AUTHOR("djkabutar <d.kabutarwala@yahoo.com>");
MODULE_LICENSE("GPL v2");
