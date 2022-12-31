/* Copyright (C) 2022 Vicharak */

#ifndef VAAMAN_BOARDINFO_H
#define VAAMAN_BOARDINFO_H

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
static int blocks;

#define SECRET_STRING "VaamanIsTheBest"
#define SECRET_SIZE 32
char secret[SECRET_SIZE] = SECRET_STRING;
char buf[SECRET_SIZE];

#endif /* VAAMAN_BOARDINFO_H */
