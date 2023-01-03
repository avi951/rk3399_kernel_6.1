/* Copyright (C) 2022 Vicharak */

#ifndef VAAMAN_BOARDINFO_H
#define VAAMAN_BOARDINFO_H

#define BOARDINFO_HWID_MAX_LEN 4
#define BOARDINFO_ID_MAX_LEN 2
#define BOARDINFO_NAME "vaaman-boardinfo"
#define BOARDINFO_PROC_MODE 0644

struct boardinfo_data {
	struct crypto_cipher *tfm;
	struct device *dev;
	int hw_id[BOARDINFO_HWID_MAX_LEN];
	struct proc_dir_entry *proc_file;
	struct proc_dir_entry *proc_test_file;
	int board_id[BOARDINFO_ID_MAX_LEN];
};

static struct boardinfo_data *boardinfo;
static int blocks;

#define SECRET_STRING "01234abcdefghijklmnopqrstuvwxyz"
#define SECRET_SIZE 32
char secret[SECRET_SIZE] = SECRET_STRING;
char buf[SECRET_SIZE];

char vaaman_key[SECRET_SIZE] = {
	0x56, 0x9c, 0x60, 0x45, 0x3f, 0x02, 0xb9, 0xcb,
	0x3b, 0xf7, 0xf4, 0xfe, 0xd9, 0xf3, 0x08, 0xd4,
	0x26, 0xd6, 0x54, 0xa7, 0x42, 0x04, 0x74, 0x26,
	0x9d, 0xfe, 0x58, 0x4e, 0x2c, 0x89, 0xed, 0x67
};

#endif /* VAAMAN_BOARDINFO_H */
