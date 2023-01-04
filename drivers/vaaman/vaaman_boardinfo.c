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

static int boardinfo_alloc_tfm_cipher(void)
{
	boardinfo->tfm = crypto_alloc_cipher("aes",
				  CRYPTO_ALG_TYPE_BLKCIPHER, CRYPTO_ALG_ASYNC);
	if (IS_ERR(boardinfo->tfm)) {
		pr_err("Failed to load transform for aes\n");
		return PTR_ERR(boardinfo->tfm);
	}

	return 0;
}

static int boardinfo_setkey_cipher(void)
{
	int ret = 0;

	ret = crypto_cipher_setkey(boardinfo->tfm,
							   vaaman_key, SECRET_SIZE);
	if (ret) {
		pr_err("Invalid key for aes\n");
		return -EINVAL;
	}

	return 0;
}

static int boardinfo_get_block_size(void)
{
	int block_size = 0;

	block_size = crypto_cipher_blocksize(boardinfo->tfm);
	if (!block_size) {
		pr_err("Invalid block size for aes\n");
		return -EINVAL;
	}

	return block_size;
}

#ifdef CONFIG_VAAMAN_BOARDINFO_DEBUG
static void boardinfo_print_secret(char *type, char *secret)
{
	int i;

	printk("%s: ", type);
	for (i = 0; i < strlen(secret); i++)
		printk("%x ", secret[i]);
	printk("\n");
}
#endif

static int boardinfo_encrypt_buffer(void)
{
	unsigned int block_size;
	char *padding = NULL;
	int i, ret = 0;

#ifdef CONFIG_VAAMAN_BOARDINFO_DEBUG
	boardinfo_print_secret("Secret string", secret);
#endif

	/* Allocate a cipher handle */
	ret = boardinfo_alloc_tfm_cipher();
	if (ret)
		goto out;

	/* Set the key for the cipher */
	ret = boardinfo_setkey_cipher();
	if (ret)
		goto free_cipher_handle;

	/* Calculate block size */
	block_size = boardinfo_get_block_size();
	if (block_size < 0)
		goto free_cipher_handle;

	/*
	 * Create padding for the secret string
	 * to be a multiple of the block size
	 */
	padding = kmalloc(block_size + 1, GFP_KERNEL);
	if (!padding) {
		ret = -ENOMEM;
		goto free_cipher_handle;
	}

	/* Encrypt the secret string with AES */
	for (i = 0; i < SECRET_SIZE; i += block_size) {
		memset(padding, 0, block_size + 1);
		strlcpy(padding, &secret[i], block_size + 1);
		/* Encrypt the block (dst: *secret, src: *padding) */
		crypto_cipher_encrypt_one(boardinfo->tfm,
					  &boardinfo->buf[i], padding);
		boardinfo->blocks++;
	}

#ifdef CONFIG_VAAMAN_BOARDINFO_DEBUG
	/* Print encrypted secret */
	boardinfo_print_secret("Encrypted buffer", boardinfo->buf);
#endif

	/* Free the padding memory */
	kfree(padding);

free_cipher_handle:
	/* Free the cipher handle */
	crypto_free_cipher(boardinfo->tfm);
out:
	return ret;
}

static int boardinfo_show(struct seq_file *m, void *v)
{
	/* Check if the board id is valid */
	if (boardinfo->board_id[0] == 0x3 &&
	    boardinfo->board_id[1] == 0xc) {

		/* Encrypt the buf */
		if (boardinfo_encrypt_buffer())
			return -EINVAL;

		seq_printf(m, "%s", boardinfo->buf);
	} else {
		pr_err("%s: boardid: %x %x is incorrect\n",
		       __func__,
				boardinfo->board_id[0],
				boardinfo->board_id[1]);

		seq_printf(m, "%s", "");
	}

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

#ifdef CONFIG_VAAMAN_BOARDINFO_DECRYPT
static int boardinfo_decrypt_buffer(void)
{
	unsigned int block_size;
	int i, ret = 0;

#ifdef CONFIG_VAAMAN_BOARDINFO_DEBUG
	/* Print secret */
	boardinfo_print_secret("Encrypted buffer", boardinfo->buf);
#endif

	/* Allocate a cipher handle */
	ret = boardinfo_alloc_tfm_cipher();
	if (ret)
		goto out;

	/* Set the key for the cipher */
	ret = boardinfo_setkey_cipher();
	if (ret)
		goto free_cipher_handle;

	/* Calculate block size for AES */
	block_size = boardinfo_get_block_size();
	if (block_size < 0)
		goto free_cipher_handle;

	/* Decrypt secret */
	for (i = 0; i < SECRET_SIZE; i += block_size)
		crypto_cipher_decrypt_one(boardinfo->tfm,
					  &boardinfo->dec_buf[i], &boardinfo->buf[i]);

#ifdef CONFIG_VAAMAN_BOARDINFO_DEBUG
	boardinfo_print_secret("Decrypted buffer", boardinfo->dec_buf);
#endif

free_cipher_handle:
	crypto_free_cipher(boardinfo->tfm);
out:
	return ret;
}

static int boardinfo_decrypt_show(struct seq_file *m, void *v)
{
	/* Check if the board id is valid */
	if (boardinfo->board_id[0] == 0x3 &&
	    boardinfo->board_id[1] == 0xc) {
	
		/* Decrypt the buf */
		if (boardinfo_decrypt_buffer())
			return -EINVAL;

		seq_printf(m, "%s", boardinfo->dec_buf);
	} else {
		pr_err("%s: boardid: %x %x is incorrect\n",
		       __func__,
				boardinfo->board_id[0],
				boardinfo->board_id[1]);

		seq_printf(m, "%s", "");
	}

	return 0;
}

static int boardinfo_decrypt_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardinfo_decrypt_show, NULL);
}

static const struct file_operations boardinfo_decrypt_ops = {
	.owner	= THIS_MODULE,
	.open	= boardinfo_decrypt_open,
	.read	= seq_read,
};

static int boardinfo_decrypt_proc_create(void)
{
	/* Create read only proc entry */
	boardinfo->proc_file = proc_create("vaaman-boardinfo-decrypt-test",
			BOARDINFO_PROC_MODE,
			NULL,
			&boardinfo_decrypt_ops);

	/* Check if proc entry is created */
	if (!boardinfo->proc_file) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
#endif

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

	pr_debug("%s: board id: %x %x\n",
		 __func__, boardinfo->board_id[0], boardinfo->board_id[1]);

	return 0;
}

static int boardinfo_probe(struct platform_device *pdev)
{
	int ret;

	/* Allocate memory for boardinfo */
	boardinfo = devm_kzalloc(&pdev->dev, sizeof(*boardinfo), GFP_KERNEL);
	if (!boardinfo) {
		ret = -ENOMEM;
		goto out;
	}

	/* Set the device */
	boardinfo->dev = &pdev->dev;

	/* Initialize the board hw id to indicate that the id is not set */
	memset(boardinfo->hw_id, -1, sizeof(boardinfo->hw_id));

	/* Get board hw_id from gpio pins */
	boardinfo->hw_id[0] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id0", 1);
	if (boardinfo->hw_id[0] < 0) {
		ret = boardinfo->hw_id[0];
		goto out;
	}

	boardinfo->hw_id[1] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id1", 1);
	if (boardinfo->hw_id[1] < 0) {
		ret = boardinfo->hw_id[1];
		goto out;
	}

	boardinfo->hw_id[2] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id2", 0);
	if (boardinfo->hw_id[2] < 0) {
		ret = boardinfo->hw_id[2];
		goto out;
	}

	boardinfo->hw_id[3] =
		boardinfo_get_gpio_value(boardinfo->dev, "hw_id3", 0);
	if (boardinfo->hw_id[3] < 0) {
		ret = boardinfo->hw_id[3];
		goto out;
	}

	/* Create board id from the hw id */
	ret = boardinfo_create_board_id();
	if (ret < 0) {
		pr_err("%s: Failed to create board id\n", __func__);
		goto out;
	}

	/* Create proc entry for board id */
	ret = boardinfo_proc_create();
	if (ret < 0) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		goto out;
	}

#ifdef CONFIG_VAAMAN_BOARDINFO_DECRYPT
	/* Create proc entry for decrypt test */
	ret = boardinfo_decrypt_proc_create();
	if (ret < 0) {
		pr_err("%s: Failed to create proc entry\n", __func__);
		goto out;
	}
#endif

out:
	return ret;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	proc_remove(boardinfo->proc_file);
#ifdef CONFIG_VAAMAN_BOARDINFO_DECRYPT
	proc_remove(boardinfo->proc_file);
#endif
	vfree(boardinfo);

	return 0;
}

static const struct of_device_id of_boardinfo_match[] = {
	{ .compatible = BOARDINFO_NAME, },
	{},
};
MODULE_DEVICE_TABLE(of, of_boardinfo_match);

static struct platform_driver vaaman_boardinfo = {
	.probe = boardinfo_probe,
	.remove = boardinfo_remove,
	.driver = {
		.name = BOARDINFO_NAME,
#ifdef CONFIG_OF_GPIO
		.of_match_table = of_match_ptr(of_boardinfo_match),
#endif
	},
};

module_platform_driver(vaaman_boardinfo);
