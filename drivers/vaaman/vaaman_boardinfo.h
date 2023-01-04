/* Copyright (C) 2022 Vicharak */

#ifndef VAAMAN_BOARDINFO_H
#define VAAMAN_BOARDINFO_H

#define BOARDINFO_HWID_MAX_LEN 4
#define BOARDINFO_ID_MAX_LEN 2
#define BOARDINFO_NAME "vaaman-boardinfo"
#define BOARDINFO_PROC_MODE 0644
#define SECRET_SIZE 16

char secret[SECRET_SIZE] = {
	0x76, 0x69, 0x63, 0x68, 0x61, 0x72, 0x61, 0x6b,
	0x76, 0x61, 0x61, 0x6d, 0x61, 0x6e, 0x30, 0x30,
};

char vaaman_key[SECRET_SIZE] = {
	0x56, 0x9c, 0x60, 0x45, 0x3f, 0x02, 0xb9, 0xcb,
	0x3b, 0xf7, 0xf4, 0xfe, 0xd9, 0xf3, 0x08, 0xd4,
};

struct boardinfo_data {
	char buf[SECRET_SIZE];
	int blocks;
	int board_id[BOARDINFO_ID_MAX_LEN];
	int hw_id[BOARDINFO_HWID_MAX_LEN];
	struct crypto_cipher *tfm;
	struct device *dev;
	struct proc_dir_entry *proc_file;
#ifdef CONFIG_BOARDINFO_DECRYPT
	struct proc_dir_entry *proc_file;
#endif
};

static struct boardinfo_data *boardinfo;

#endif /* VAAMAN_BOARDINFO_H */
