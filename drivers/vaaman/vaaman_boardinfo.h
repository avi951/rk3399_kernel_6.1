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

#define SECRET_STRING "0123abcdefghijklmnopqrstuvwxyz"
#define SECRET_SIZE 32
char secret[SECRET_SIZE] = SECRET_STRING;
char buf[SECRET_SIZE];

char vaaman_key[SECRET_SIZE] = {
	0x5B, 0x11, 0xE3, 0x87, 0x59, 0x4F, 0xA5, 0xC3,
	0xBB, 0xB2, 0xB9, 0x2C, 0xDB, 0xAE, 0x8E, 0x42,
};

#endif /* VAAMAN_BOARDINFO_H */
