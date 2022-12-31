/* Copyright (C) 2022 Vicharak */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include "boardinfo.h"

static int boardid[2] = { -1, -1 };

static const struct of_device_id of_boardinfo_match[] = {
	{ .compatible = "vaaman-boardinfo", },
	{},
};
MODULE_DEVICE_TABLE(of, of_boardinfo_match);

static int boardinfo_show(struct seq_file *m, void *v)
{
	char *boardinfo = NULL;

	/* TODO: Use asymmetric key encryption to encrypt the boardinfo */

	if (boardid[0] == 0xc && boardid[1] == 0x3)
		boardinfo = "sss3kk2aaaa4";
	else
		pr_err("%s: boardid: %x %x is incorrect\n",
		       __func__, boardid[0], boardid[1]);

	seq_printf(m, "%s\n", boardinfo);
	return 0;
}

static int boardinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardinfo_show, NULL);
}

static const struct file_operations boardinfo_ops = {
	.owner	= THIS_MODULE,
	.open	= boardinfo_open,
	.read	= seq_read,
};

static int boardinfo_proc_create(char *proc_name)
{
	struct proc_dir_entry *file = NULL;

	/* Create read only proc entry */
	file = proc_create(proc_name, 0444, NULL, &boardinfo_ops);

	if (!file) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int boardinfo_get_gpio_value(struct device *dev,
				    char *gpio_name, int flag)
{
	int ret = -1, hwid = -1;

	hwid = of_get_named_gpio(dev->of_node, gpio_name, 0);
	if (!gpio_is_valid(hwid)) {
		pr_err("%s: %s pin not available in board!\n",
		       __func__, gpio_name);
		return -ENODEV;
	}

	/* set gpio direction as input */
	if (flag) {
		ret = devm_gpio_request_one(dev,
					    hwid, GPIOF_DIR_OUT, gpio_name);
		if (ret < 0) {
			pr_err("%s: Failed to set %s pin\n",
			       __func__, gpio_name);
			return ret;
		}
	}

	/* get gpio value from board */
	ret = gpio_get_value(hwid);

	/* Free the gpio pin */
	gpio_free(hwid);

	pr_info("%s: id: %d\n", __func__, ret);

	return ret;
}

static int boardinfo_create_boardid(int *id, int size)
{
	if (size != 4) {
		pr_err("%s: boardid size is incorrect\n", __func__);
		return -EINVAL;
	}

	/* Convert the id bits to a hw id */
	boardid[0] = (id[0] << 3) + (id[1] << 2) + (id[2] << 1) + id[3];
	boardid[1] = (id[3] << 3) + (id[2] << 2) + (id[1] << 1) + id[0];

	pr_info("%s: board id: 0x%x0x%x\n", __func__, boardid[0], boardid[1]);

	return 0;
}

static int boardinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	/* take three id bits to generate a hw id */
	int id[4] = { -1, -1, -1, -1 };
	int ret = 0;

	ret = boardinfo_get_gpio_value(dev, "hwid0", 1);
	if (ret < 0) {
		pr_err("%s: Failed to get hwid0\n", __func__);
		return ret;
	}
	id[0] = ret;

	ret = boardinfo_get_gpio_value(dev, "hwid1", 1);
	if (ret < 0) {
		pr_err("%s: Failed to get hwid1\n", __func__);
		return ret;
	}
	id[1] = ret;

	ret = boardinfo_get_gpio_value(dev, "hwid2", 0);
	if (ret < 0) {
		pr_err("%s: Failed to get hwid2\n", __func__);
		return ret;
	}
	id[2] = ret;

	ret = boardinfo_get_gpio_value(dev, "hwid3", 0);
	if (ret < 0) {
		pr_err("%s: Failed to get hwid3\n", __func__);
		return ret;
	}
	id[3] = ret;

	/* get board id from id bits in boardid */
	ret = boardinfo_create_boardid(id, ARRAY_SIZE(id));
	if (ret < 0) {
		pr_err("%s: Failed to create boardid\n", __func__);
		return ret;
	}

	ret = boardinfo_proc_create("boardinfo");
	if (ret < 0) {
		pr_err("Failed to create boardinfo proc node\n");
		return ret;
	}

	return 0;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vaaman_boardinfo = {
	.probe = boardinfo_probe,
	.remove = boardinfo_remove,
	.driver = {
		.name = "vaaman-boardinfo",
#ifdef CONFIG_OF_GPIO
		.of_match_table = of_match_ptr(of_boardinfo_match),
#endif
	},
};

module_platform_driver(vaaman_boardinfo);
