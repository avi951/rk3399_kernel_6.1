/* Copyright (C) 2022 Vicharak */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "vaaman_boardinfo.h"
#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define BOARDINFO_HWID_MAX_LEN 4
#define BOARDINFO_ID_MAX_LEN 2
#define BOARDINFO_NAME "vaaman-boardinfo"
#define BOARDINFO_PROC_MODE 0444

struct boardinfo_data {
	struct crypto_cipher *tfm;
	struct device *dev;
	int hw_id[BOARDINFO_HWID_MAX_LEN];
	struct proc_dir_entry *proc_file;
	struct proc_dir_entry *proc_test_file;
	int board_id[BOARDINFO_ID_MAX_LEN];
};

static struct boardinfo_data *boardinfo;
#define SECRET_STRING "sss3kk2aaaa4"
char secret[32] = SECRET_STRING;

static const struct of_device_id of_boardinfo_match[] = {
	{ .compatible = BOARDINFO_NAME, },
	{},
};
MODULE_DEVICE_TABLE(of, of_boardinfo_match);

/* print secret */
static void boardinfo_print_secret(char *type, char *secret)
{
	int i;

	pr_info("%s: %s: ", __func__, type);
	for (i = 0; i < 32; i++)
		pr_info("%x ", secret[i]);
	pr_info("\n");
}

static int boardinfo_show(struct seq_file *m, void *v)
{
	unsigned int block_size;
	char *padding;
	int blocks = 0;
	int i;

	if (boardinfo->board_id[0] == 0xc && boardinfo->board_id[1] == 0x3) {
		if (!strncmp(secret, SECRET_STRING, 32))
			pr_err("correct secret string\n");

		/* print secret */
		boardinfo_print_secret("Secret", secret);

		/* Allocate a cipher handle */
		boardinfo->tfm = crypto_alloc_cipher("aes", 0, 0);
		if (IS_ERR(boardinfo->tfm)) {
			pr_err("Failed to load transform for aes\n");
			return PTR_ERR(boardinfo->tfm);
		}

		/* Set the key */
		if (crypto_cipher_setkey(boardinfo->tfm, secret, 32)) {
			crypto_free_cipher(boardinfo->tfm);
			pr_err("Invalid key for aes\n");
			return -EINVAL;
		}

		/* block size */
		block_size = crypto_cipher_blocksize(boardinfo->tfm);
		if (!block_size) {
			crypto_free_cipher(boardinfo->tfm);
			pr_err("Invalid block size for aes\n");
			return -EINVAL;
		}

		/* padding */
		padding = kzalloc(block_size + 1, GFP_KERNEL);
		if (!padding) {
			crypto_free_cipher(boardinfo->tfm);
			pr_err("Failed to allocate padding\n");
			return -ENOMEM;
		}

		/* encrypt secret */
		blocks = 32 / block_size;
		for (i = 0; i < blocks; i++) {
			crypto_cipher_encrypt_one(
					boardinfo->tfm,
					secret + i * block_size,
					secret + i * block_size);
		}

		/* print encrypted secret */
		boardinfo_print_secret("Encrypted Secret", secret);

		/* Free */
		crypto_free_cipher(boardinfo->tfm);
		kfree(padding);

	} else {
		pr_err("%s: boardid: %x %x is incorrect\n",
		       __func__,
				boardinfo->board_id[0],
				boardinfo->board_id[1]);
	}

	seq_printf(m, "%s\n", secret);
	return 0;
}

static int boardinfo_decrypt_test_show(struct seq_file *m, void *v)
{
	unsigned int block_size;
	char *padding;
	int blocks = 0;
	int i;

	if (!strncmp(secret, SECRET_STRING, 32))
		pr_err("correct secret string\n");

	/* print secret */
	boardinfo_print_secret("Enc Secret", secret);

	/* Allocate a cipher handle */
	boardinfo->tfm = crypto_alloc_cipher("aes", 0, 0);
	if (IS_ERR(boardinfo->tfm)) {
		pr_err("Failed to load transform for aes\n");
		return PTR_ERR(boardinfo->tfm);
	}

	/* Set the key */
	if (crypto_cipher_setkey(boardinfo->tfm, secret, 32)) {
		crypto_free_cipher(boardinfo->tfm);
		pr_err("Invalid key for aes\n");
		return -EINVAL;
	}

	/* block size */
	block_size = crypto_cipher_blocksize(boardinfo->tfm);
	if (!block_size) {
		crypto_free_cipher(boardinfo->tfm);
		pr_err("Invalid block size for aes\n");
		return -EINVAL;
	}

	/* padding */
	padding = kzalloc(block_size + 1, GFP_KERNEL);
	if (!padding) {
		crypto_free_cipher(boardinfo->tfm);
		pr_err("Failed to allocate padding\n");
		return -ENOMEM;
	}

	/* decrypt secret */
	blocks = 32 / block_size;
	for (i = 0; i < blocks; i++) {
		crypto_cipher_decrypt_one(
				boardinfo->tfm,
				secret + i * block_size,
				secret + i * block_size);
	}

	/* print decrypted secret */
	boardinfo_print_secret("Decrypted Secret", secret);

	/* Free */
	crypto_free_cipher(boardinfo->tfm);
	kfree(padding);

	seq_printf(m, "%s\n", secret);
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

static int boardinfo_proc_create(void)
{
	/* Create read only proc entry */
	boardinfo->proc_file = proc_create(BOARDINFO_NAME,
			BOARDINFO_PROC_MODE,
			NULL,
			&boardinfo_ops);

	/* Check if proc entry is created */
	if (!boardinfo->proc_file) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int boardinfo_decrypt_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardinfo_decrypt_test_show, NULL);
}

static const struct file_operations boardinfo_decrypt_test_ops = {
	.owner	= THIS_MODULE,
	.open	= boardinfo_decrypt_test_open,
	.read	= seq_read,
};

static int boardinfo_decrypt_test_proc_create(void)
{
	/* Create read only proc entry */
	boardinfo->proc_test_file = proc_create("vaaman-boardinfo-decrypt-test",
			BOARDINFO_PROC_MODE,
			NULL,
			&boardinfo_decrypt_test_ops);

	/* Check if proc entry is created */
	if (!boardinfo->proc_test_file) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int boardinfo_get_gpio_value(struct device *dev,
				    char *gpio_name, int flag)
{
	int ret = -1, hwid = -1;

	/* Get the gpio number from the device tree */
	hwid = of_get_named_gpio(dev->of_node, gpio_name, 0);
	if (!gpio_is_valid(hwid)) {
		pr_err("%s: %s pin not available in board!\n",
		       __func__, gpio_name);
		return -ENODEV;
	}

	/* Set gpio direction as input */
	if (flag) {
		ret = devm_gpio_request_one(dev,
					    hwid, GPIOF_DIR_OUT, gpio_name);
		if (ret < 0) {
			pr_err("%s: Failed to set %s pin\n",
			       __func__, gpio_name);
			return ret;
		}
	}

	/* Get gpio value from board */
	ret = gpio_get_value(hwid);

	/* Free the gpio pin */
	gpio_free(hwid);

	pr_info("%s: id: %d\n", __func__, ret);

	return ret;
}

static int boardinfo_create_board_id(void)
{
	/* Check if the board hw id is set correctly */
	if (boardinfo->hw_id[0] < 0 || boardinfo->hw_id[1] < 0 ||
	    boardinfo->hw_id[2] < 0 || boardinfo->hw_id[3] < 0) {
		pr_err("%s: boardid: %x %x %x %x is incorrect\n",
		       __func__, boardinfo->hw_id[0], boardinfo->hw_id[1],
				boardinfo->hw_id[2], boardinfo->hw_id[3]);
		return -EINVAL;
	}

	/* Convert the id bits to a hw id */
	boardinfo->board_id[0] = (boardinfo->hw_id[0] << 3) |
		(boardinfo->hw_id[1] << 2) |
		(boardinfo->hw_id[2] << 1) |
		(boardinfo->hw_id[3] << 0);

	boardinfo->board_id[1] = (boardinfo->hw_id[3] << 3) |
		(boardinfo->hw_id[2] << 2) |
		(boardinfo->hw_id[1] << 1) |
		(boardinfo->hw_id[0] << 0);

	pr_info("%s: board id: 0x%x0x%x\n",
		__func__, boardinfo->board_id[0], boardinfo->board_id[1]);

	return 0;
}

static int boardinfo_probe(struct platform_device *pdev)
{
	int ret;

	/* Allocate memory for boardinfo */
	boardinfo = devm_kzalloc(&pdev->dev, sizeof(*boardinfo), GFP_KERNEL);
	if (!boardinfo)
		return -ENOMEM;

	/* Set the device */
	boardinfo->dev = &pdev->dev;

	/* Initialize the board hw id to indicate that the id is not set */
	boardinfo->hw_id[0] = -1;
	boardinfo->hw_id[1] = -1;
	boardinfo->hw_id[2] = -1;
	boardinfo->hw_id[3] = -1;

	/* get board hw_id from gpio */
	boardinfo->hw_id[0] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id0", 1);
	boardinfo->hw_id[1] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id1", 1);
	boardinfo->hw_id[2] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id2", 0);
	boardinfo->hw_id[3] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id3", 0);

	/* create board id */
	ret = boardinfo_create_board_id();
	if (ret < 0) {
		pr_err("%s: Failed to create board id\n", __func__);
		return ret;
	}

	/* create proc entry */
	ret = boardinfo_proc_create();
	if (ret < 0) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return ret;
	}

	/* create proc entry for decrypt test */
	ret = boardinfo_decrypt_test_proc_create();
	if (ret < 0) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return ret;
	}

	return 0;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	proc_remove(boardinfo->proc_file);
	proc_remove(boardinfo->proc_test_file);
	vfree(boardinfo);

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
