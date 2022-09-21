/*
 * Rockchip SoC DP (Display Port) interface driver.
 *
 * Copyright (C) Fuzhou Rockchip Electronics Co., Ltd.
 * Author: Andy Yan <andy.yan@rock-chips.com>
 *         Yakir Yang <ykk@rock-chips.com>
 *         Jeff Chen <jeff.chen@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/component.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>

#include <uapi/linux/videodev2.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/bridge/analogix_dp.h>
#include "lt7911d.h"

#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"

#define to_dp(nm) container_of(nm, struct rockchip_dp_device, nm)

/**
 * struct rockchip_dp_chip_data - splite the grf setting of kind of chips
 * @lcdsel_grf_reg: grf register offset of lcdc select
 * @lcdsel_big: reg value of selecting vop big for eDP
 * @lcdsel_lit: reg value of selecting vop little for eDP
 */
struct rockchip_dp_chip_data
{
	u32 lcdsel_grf_reg;
	u32 lcdsel_big;
	u32 lcdsel_lit;
	u32 chip_type;
	bool has_vop_sel;
};

struct dp_i2c
{
	struct i2c_adapter adap;

	struct mutex lock;
	struct completion cmp;
	u8 stat;

	u8 slave_reg;
	bool is_regaddr;
	bool is_segment;

	unsigned int scl_high_ns;
	unsigned int scl_low_ns;
};

struct rockchip_dp_device
{
	struct drm_device *drm_dev;
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_display_mode mode;
	struct regmap *regmap[3];

	struct clk *pclk;
	struct regmap *grf;
	struct reset_control *rst;
	struct regulator *vcc_supply;
	struct regulator *vccio_supply;
	struct i2c_client *client;
	struct dp_i2c *i2c;

	const struct rockchip_dp_chip_data *data;

	struct analogix_dp_device *adp;
	struct analogix_dp_plat_data plat_data;
};

static int rockchip_dp_pre_init(struct rockchip_dp_device *dp)
{
	reset_control_assert(dp->rst);
	usleep_range(10, 20);
	reset_control_deassert(dp->rst);

	return 0;
}

// Step1 : Read chip ID to check I2C connection
bool CheckChipId(struct rockchip_dp_device *dp)
{

	int bA000, bA001, nChipID;

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0xff, 0xa0);
	regmap_read(dp->regmap[0], 0x00, &bA000);
	regmap_read(dp->regmap[0], 0x01, &bA001);

	nChipID = ((bA000 << 8) | bA001);
	return nChipID == 0x1605;
}

// Step2 : Initial Settings :
void Config(struct rockchip_dp_device *dp)
{
	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0x5A, 0x82);
	regmap_write(dp->regmap[0], 0x5E, 0xC0);
	regmap_write(dp->regmap[0], 0x58, 0x00);
	regmap_write(dp->regmap[0], 0x59, 0x51);
	regmap_write(dp->regmap[0], 0x5A, 0x92);
	regmap_write(dp->regmap[0], 0x5A, 0x82);
}

// Step3 : Read the hdcp key which is stored in the flash and save it.
void ReadKey(struct rockchip_dp_device *dp)
{
	int bRead;
	int bRet;
	long lReadAddr;
	int addr[3] = {0, 0, 0};
	int nPage, i;
	int nPageReadLen = 16;
	int byPageReadData[128];

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0xFF, 0x90);
	regmap_read(dp->regmap[0], 0x02, &bRead);
	bRead &= 0xDF;
	regmap_write(dp->regmap[0], 0x02, bRead);
	bRead |= 0x20;
	regmap_write(dp->regmap[0], 0x02, bRead);

	// wren enable
	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0x5A, 0x86);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	lReadAddr = 0x6000;
	addr[0] = (lReadAddr & 0xFF0000) >> 16;
	addr[1] = (lReadAddr & 0xFF00) >> 8;
	addr[2] = lReadAddr & 0xFF;

	nPage = 286 / 16;
	if (286 % 16 != 0)
	{
		++nPage;
	}

	for (i = 0; i < nPage; ++i)
	{
		//ÉèÖÃ¶ÁÈ¡µØÖ·
		regmap_write(dp->regmap[0], 0x5E, 0x60 | 0xF);
		regmap_write(dp->regmap[0], 0x5A, 0xA2);
		regmap_write(dp->regmap[0], 0x5A, 0x82);
		regmap_write(dp->regmap[0], 0x5B, addr[0]);
		regmap_write(dp->regmap[0], 0x5C, addr[1]);
		regmap_write(dp->regmap[0], 0x5D, addr[2]);
		regmap_write(dp->regmap[0], 0x5A, 0x92);
		regmap_write(dp->regmap[0], 0x5A, 0x82);
		regmap_write(dp->regmap[0], 0x58, 0x01);

		nPageReadLen = 16;
		if (286 - i * 16 < 16)
		{
			nPageReadLen = 286 - i * 16;
		}

		regmap_raw_read(dp->regmap[0], 0x5F, &byPageReadData, nPageReadLen);

		lReadAddr += nPageReadLen;
		addr[0] = (lReadAddr & 0xFF0000) >> 16;
		addr[1] = (lReadAddr & 0xFF00) >> 8;
		addr[2] = lReadAddr & 0xFF;
	}
}

// Step4£ºBlock Erase
void BlockErase(struct rockchip_dp_device *dp)
{
	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	regmap_write(dp->regmap[0], 0x5A, 0x86);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	regmap_write(dp->regmap[0], 0x5B, 0x00);
	regmap_write(dp->regmap[0], 0x5C, 0x00);
	regmap_write(dp->regmap[0], 0x5D, 0x00);

	regmap_write(dp->regmap[0], 0x5A, 0x83);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	msleep(1000);
}

// Step5 : Write firmware data into flash
void Write(struct rockchip_dp_device *dp)
{
	int bRead;
	int addr[3] = {0, 0, 0};
	int DataLen = 286;
	long lWriteAddr = 0;
	long lStartAddr = lWriteAddr;
	int nPage = DataLen / 16;
	int j, k;

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0xFF, 0x90);
	regmap_read(dp->regmap[0], 0x02, &bRead);
	bRead &= 0xDF;
	regmap_write(dp->regmap[0], 0x02, bRead);
	bRead |= 0x20;
	regmap_write(dp->regmap[0], 0x02, bRead);

	// wren enable
	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0x5A, 0x86);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	if (DataLen % 16 != 0)
	{
		++nPage;
	}

	for (j = 0; j < nPage; ++j)
	{

		regmap_write(dp->regmap[0], 0x5A, 0x86);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		regmap_write(dp->regmap[0], 0x5E, 0xE0 | 0xF);
		regmap_write(dp->regmap[0], 0x5A, 0xA2);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		regmap_write(dp->regmap[0], 0x58, 0x01);

		for (k = 0; k < 16; ++j)
			regmap_write(dp->regmap[0], 0x59, k + j * 16);

		regmap_write(dp->regmap[0], 0x5B, addr[0]);
		regmap_write(dp->regmap[0], 0x5C, addr[1]);
		regmap_write(dp->regmap[0], 0x5D, addr[2]);
		regmap_write(dp->regmap[0], 0x5E, 0xE0);
		regmap_write(dp->regmap[0], 0x5A, 0x92);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		lStartAddr += 16;
		addr[0] = (lStartAddr & 0xFF0000) >> 16;
		addr[1] = (lStartAddr & 0xFF00) >> 8;
		addr[2] = lStartAddr & 0xFF;
	}

	regmap_write(dp->regmap[0], 0x5A, 0x8A);
	regmap_write(dp->regmap[0], 0x5A, 0x82);
}

// Step6 : Read the data in the flash and compare it with original data
void Read(struct rockchip_dp_device *dp)
{
	int bRead;
	int bRet;
	long lReadAddr = 0;
	int addr[3] = {0, 0, 0};
	int nPage = 286 / 16;
	int nPageReadLen, i;
	int byPageReadData[128];
	int WriteDataLen = 286;

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0xFF, 0x90);
	regmap_read(dp->regmap[0], 0x02, &bRead);
	bRead &= 0xDF;
	regmap_write(dp->regmap[0], 0x02, bRead);
	bRead |= 0x20;
	regmap_write(dp->regmap[0], 0x02, bRead);

	// wren enable
	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0x5A, 0x86);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	addr[0] = (lReadAddr & 0xFF0000) >> 16;
	addr[1] = (lReadAddr & 0xFF00) >> 8;
	addr[2] = lReadAddr & 0xFF;

	if (WriteDataLen % 16 != 0)
	{
		++nPage;
	}

	for (i = 0; i < nPage; ++i)
	{
		regmap_write(dp->regmap[0], 0x5E, 0x60 | 0xF);
		regmap_write(dp->regmap[0], 0x5A, 0xA2);
		regmap_write(dp->regmap[0], 0x5A, 0x82);
		regmap_write(dp->regmap[0], 0x5B, addr[0]);
		regmap_write(dp->regmap[0], 0x5C, addr[1]);
		regmap_write(dp->regmap[0], 0x5D, addr[2]);
		regmap_write(dp->regmap[0], 0x5A, 0x92);
		regmap_write(dp->regmap[0], 0x5A, 0x82);
		regmap_write(dp->regmap[0], 0x58, 0x01);

		nPageReadLen = 16;
		if (WriteDataLen - i * 16 < 16)
		{
			nPageReadLen = WriteDataLen - i * 16;
		}

		regmap_raw_read(dp->regmap[0], 0x5F, &byPageReadData, nPageReadLen);

		lReadAddr += nPageReadLen;
		addr[0] = (lReadAddr & 0xFF0000) >> 16;
		addr[1] = (lReadAddr & 0xFF00) >> 8;
		addr[2] = lReadAddr & 0xFF;
	}
}

// Step7 : Write the key you saved into the flash after erase .
void WriteKey(struct rockchip_dp_device *dp)
{
	int bRead;
	int addr[3] = {0x00, 0x60, 0x00};
	long lWriteAddr = 0x6000;
	long lStartAddr = lWriteAddr;
	long lEndAddr = lWriteAddr;
	int nPage = 286 / 16;
	int DataLen = 286;
	int j, k;

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0xEE, 0x01);

	regmap_write(dp->regmap[0], 0xFF, 0x90);
	regmap_read(dp->regmap[0], 0x02, &bRead);
	bRead &= 0xDF;
	regmap_write(dp->regmap[0], 0x02, bRead);
	bRead |= 0x20;
	regmap_write(dp->regmap[0], 0x02, bRead);

	regmap_write(dp->regmap[0], 0xFF, 0x80);
	regmap_write(dp->regmap[0], 0x5A, 0x86);
	regmap_write(dp->regmap[0], 0x5A, 0x82);

	if (DataLen % 16 != 0)
	{
		++nPage;
	}

	for (j = 0; j < nPage; ++j)
	{
		regmap_write(dp->regmap[0], 0x5A, 0x86);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		regmap_write(dp->regmap[0], 0x5E, 0xE0 | 0xF);
		regmap_write(dp->regmap[0], 0x5A, 0xA2);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		regmap_write(dp->regmap[0], 0x58, 0x01);

		for (k = 0; k < 16; ++j)
			regmap_write(dp->regmap[0], 0x59, k + j * 16);

		regmap_write(dp->regmap[0], 0x5B, addr[0]);
		regmap_write(dp->regmap[0], 0x5C, addr[1]);
		regmap_write(dp->regmap[0], 0x5D, addr[2]);
		regmap_write(dp->regmap[0], 0x5E, 0xE0);
		regmap_write(dp->regmap[0], 0x5A, 0x92);
		regmap_write(dp->regmap[0], 0x5A, 0x82);

		lStartAddr += 16;
		addr[0] = (lStartAddr & 0xFF0000) >> 16;
		addr[1] = (lStartAddr & 0xFF00) >> 8;
		addr[2] = lStartAddr & 0xFF;
	}

	regmap_write(dp->regmap[0], 0x5A, 0x8A);
	regmap_write(dp->regmap[0], 0x5A, 0x82);
}

static void dp_init(struct rockchip_dp_device *dp)
{
	u8 lanes = 4;
	const struct drm_display_mode *mode = &dp->mode;
	u32 hactive, hfp, hsync, hbp, vfp, vsync, vbp, htotal, vtotal;
	unsigned int version[2], clk[3];
	int i, j;
	unsigned char EDID[128] = {
		0x00,
		0xff,
		0xff,
		0xff,
		0xff,
		0xff,
		0xff,
		0x00,
		0x31,
		0xd8,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x05,
		0x16,
		0x01,
		0x03,
		0x6d,
		0x32,
		0x1c,
		0x78,
		0xea,
		0x5e,
		0xc0,
		0xa4,
		0x59,
		0x4a,
		0x98,
		0x25,
		0x20,
		0x50,
		0x54,
		0x00,
		0x00,
		0x00,
		0xd1,
		0xc0,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x02,
		0x3a,
		0x80,
		0x18,
		0x71,
		0x38,
		0x2d,
		0x40,
		0x58,
		0x2c,
		0x45,
		0x00,
		0xf4,
		0x19,
		0x11,
		0x00,
		0x00,
		0x1e,
		0x00,
		0x00,
		0x00,
		0xff,
		0x00,
		0x4c,
		0x69,
		0x6e,
		0x75,
		0x78,
		0x20,
		0x23,
		0x30,
		0x0a,
		0x20,
		0x20,
		0x20,
		0x20,
		0x00,
		0x00,
		0x00,
		0xfd,
		0x00,
		0x3b,
		0x3d,
		0x42,
		0x44,
		0x0f,
		0x00,
		0x0a,
		0x20,
		0x20,
		0x20,
		0x20,
		0x20,
		0x20,
		0x00,
		0x00,
		0x00,
		0xfc,
		0x00,
		0x4c,
		0x69,
		0x6e,
		0x75,
		0x78,
		0x20,
		0x46,
		0x48,
		0x44,
		0x0a,
		0x20,
		0x20,
		0x20,
		0x00,
		0x05,
	};

	/* TODO: lvds output init */

	hactive = mode->hdisplay;
	hfp = mode->hsync_start - mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;
	vfp = mode->vsync_start - mode->vdisplay;
	vsync = mode->vsync_end - mode->vsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	htotal = mode->htotal;
	vtotal = mode->vtotal;

	// mutex_lock(&dp->i2c->lock);

	// while (version[0] != 0x16) {
	// get chip id
	regmap_write(dp->regmap[0], 0xff, 0x80);
	regmap_write(dp->regmap[0], 0xee, 0x01);

	regmap_write(dp->regmap[0], 0xff, 0xa0);

	regmap_read(dp->regmap[0], 0x00, &version[0]);
	regmap_read(dp->regmap[0], 0x01, &version[1]);

	dev_info(dp->dev, "lt7911d ID: %02x, %02x\n",
			 version[0], version[1]);
	// }
	// mutex_unlock(&dp->i2c->lock);
	// dev_info(dp->dev, "Regs:\n");
	// regmap_read(dp->regmap[0], 0xEE);
	// regmap_read(dp->regmap[0], 0xFF);

	// // I2C
	// regmap_write(dp->regmap[0], 0xFF, 0x80);
	// regmap_write(dp->regmap[0], 0xEE, 0x01);

	// // configure
	// regmap_write(dp->regmap[0], 0x5A, 0x80);
	// regmap_write(dp->regmap[0], 0x5E, 0xC0);
	// regmap_write(dp->regmap[0], 0x58, 0x00);
	// regmap_write(dp->regmap[0], 0x59, 0x51);
	// regmap_write(dp->regmap[0], 0x5A, 0x90);
	// msleep(1);

	// regmap_write(dp->regmap[0], 0x5A, 0x80);

	// // block erase
	// regmap_write(dp->regmap[0], 0x5A, 0x84);
	// msleep(1);
	// regmap_write(dp->regmap[0], 0x5A, 0x80);
	// regmap_write(dp->regmap[0], 0x5B, 0x01);
	// regmap_write(dp->regmap[0], 0x5C, 0x80);
	// regmap_write(dp->regmap[0], 0x5D, 0x00);
	// regmap_write(dp->regmap[0], 0x5A, 0x81);
	// msleep(1);
	// regmap_write(dp->regmap[0], 0x5A, 0x00);
	// msleep(600);

	// for (j = 0; j < 8; j++)
	// {
	// 	regmap_write(dp->regmap[0], 0xFF, 0x90);
	// 	regmap_write(dp->regmap[0], 0x02, 0xDF);
	// 	msleep(1);
	// 	regmap_write(dp->regmap[0], 0x02, 0xFF);

	// 	// WREN
	// 	regmap_write(dp->regmap[0], 0xFF, 0x80);
	// 	regmap_write(dp->regmap[0], 0x5A, 0x84);
	// 	msleep(1);
	// 	regmap_write(dp->regmap[0], 0x5A, 0x80);

	// 	// Data to FIFO
	// 	regmap_write(dp->regmap[0], 0x5E, 0xEF);
	// 	regmap_write(dp->regmap[0], 0x5A, 0xA0);
	// 	msleep(1);
	// 	regmap_write(dp->regmap[0], 0x5A, 0x80);
	// 	regmap_write(dp->regmap[0], 0x58, 0x01);

	// 	for (i = 0; i < 16; i++)
	// 	{
	// 		regmap_write(dp->regmap[0], 0x59, EDID[j * 16 + i]);
	// 		mdelay(1);
	// 		// EDID[]++;
	// 	}

	// 	// FIFO to flash
	// 	regmap_write(dp->regmap[0], 0x5B, 0x01);
	// 	regmap_write(dp->regmap[0], 0x5C, 0x80);
	// 	regmap_write(dp->regmap[0], 0x5D, j * 16);
	// 	regmap_write(dp->regmap[0], 0x5E, 0xE0);
	// 	regmap_write(dp->regmap[0], 0x5A, 0x90);
	// 	msleep(1);
	// 	regmap_write(dp->regmap[0], 0x5A, 0x80);
	// 	msleep(10);
	// }

	// // WRDI
	// regmap_write(dp->regmap[0], 0x5A, 0x88);
	// mdelay(1);
	// regmap_write(dp->regmap[0], 0x5A, 0x80);

	// // get Hsync, Vsync, HBP, VBP, HFP, VFP
	// regmap_write(dp->regmap[0], 0xFF, 0xA0);
	// regmap_write(dp->regmap[0], 0x34, 0x21);
	// regmap_write(dp->regmap[0], 0xFF, 0xB8);
	// regmap_read(dp->regmap[0], 0xB1, &clk[0]);
	// regmap_read(dp->regmap[0], 0xB2, &clk[1]);
	// regmap_read(dp->regmap[0], 0xB3, &clk[2]);
	// dev_info(dp->dev, "Hsync: %02x%02x%02x\n",
	// 		 clk[0], clk[1], clk[2]);

	// if (CheckChipId(dp)) {
	// 	dev_info(dp->dev, "Chip ID is correct");
	// }

	Config(dp);
	ReadKey(dp);
	BlockErase(dp);
	// Write(dp);
	// Read(dp);
	// WriteKey(dp);
}

static int rockchip_dp_poweron(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);
	int ret;

	if (dp->vcc_supply)
	{
		ret = regulator_enable(dp->vcc_supply);
		if (ret)
			dev_warn(dp->dev, "failed to enable vcc: %d\n", ret);
	}

	if (dp->vccio_supply)
	{
		ret = regulator_enable(dp->vccio_supply);
		if (ret)
			dev_warn(dp->dev, "failed to enable vccio: %d\n", ret);
	}

	clk_prepare_enable(dp->pclk);

	ret = rockchip_dp_pre_init(dp);
	if (ret < 0)
	{
		dev_err(dp->dev, "failed to dp pre init %d\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_dp_powerdown(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);

	clk_disable_unprepare(dp->pclk);

	if (dp->vccio_supply)
		regulator_disable(dp->vccio_supply);

	if (dp->vcc_supply)
		regulator_disable(dp->vcc_supply);

	return 0;
}

static int rockchip_dp_get_modes(struct analogix_dp_plat_data *plat_data,
								 struct drm_connector *connector)
{
	struct drm_display_info *di = &connector->display_info;

	if (di->color_formats & DRM_COLOR_FORMAT_YCRCB444 ||
		di->color_formats & DRM_COLOR_FORMAT_YCRCB422)
	{
		di->color_formats &= ~(DRM_COLOR_FORMAT_YCRCB422 |
							   DRM_COLOR_FORMAT_YCRCB444);
		di->color_formats |= DRM_COLOR_FORMAT_RGB444;
		di->bpc = 8;
	}

	return 0;
}

static bool
rockchip_dp_drm_encoder_mode_fixup(struct drm_encoder *encoder,
								   const struct drm_display_mode *mode,
								   struct drm_display_mode *adjusted_mode)
{
	/* do nothing */
	return true;
}

static void rockchip_dp_drm_encoder_mode_set(struct drm_encoder *encoder,
											 struct drm_display_mode *mode,
											 struct drm_display_mode *adjusted)
{
	/* do nothing */
}

static void rockchip_dp_drm_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;
	u32 val;

	if (!dp->data->has_vop_sel)
	{
		dev_info(dp->dev, "does not have vop sel");
		return;
	}

	ret = drm_of_encoder_active_endpoint_id(dp->dev->of_node, encoder);
	if (ret < 0)
	{
		dev_info(dp->dev, "No active endpoint for DRM encoder");
		return;
	}

	if (ret)
		val = dp->data->lcdsel_lit;
	else
		val = dp->data->lcdsel_big;

	dev_dbg(dp->dev, "vop %s output to dp\n", (ret) ? "LIT" : "BIG");

	ret = regmap_write(dp->grf, dp->data->lcdsel_grf_reg, val);
	if (ret != 0)
	{
		dev_err(dp->dev, "Could not write to GRF: %d\n", ret);
		return;
	}
}

static void rockchip_dp_drm_encoder_nop(struct drm_encoder *encoder)
{
	/* do nothing */
}

static int
rockchip_dp_drm_encoder_atomic_check(struct drm_encoder *encoder,
									 struct drm_crtc_state *crtc_state,
									 struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_connector *connector = conn_state->connector;
	struct drm_display_info *info = &connector->display_info;

	/*
	 * The hardware IC designed that VOP must output the RGB10 video
	 * format to eDP contoller, and if eDP panel only support RGB8,
	 * then eDP contoller should cut down the video data, not via VOP
	 * contoller, that's why we need to hardcode the VOP output mode
	 * to RGA10 here.
	 */
	s->output_mode = ROCKCHIP_OUT_MODE_AAAA;
	s->output_type = DRM_MODE_CONNECTOR_eDP;
	if (info->num_bus_formats)
		s->bus_format = info->bus_formats[0];
	else
		s->bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	s->tv_state = &conn_state->tv;
	s->eotf = TRADITIONAL_GAMMA_SDR;
	s->color_space = V4L2_COLORSPACE_DEFAULT;

	return 0;
}

static int rockchip_dp_drm_encoder_loader_protect(struct drm_encoder *encoder,
												  bool on)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;

	if (on)
	{
		if (dp->vcc_supply)
		{
			ret = regulator_enable(dp->vcc_supply);
			if (ret)
				dev_warn(dp->dev,
						 "failed to enable vcc: %d\n", ret);
		}

		if (dp->vccio_supply)
		{
			ret = regulator_enable(dp->vccio_supply);
			if (ret)
				dev_warn(dp->dev,
						 "failed to enable vccio: %d\n", ret);
		}

		clk_prepare_enable(dp->pclk);
	}
	else
	{
		clk_disable_unprepare(dp->pclk);

		if (dp->vccio_supply)
			regulator_disable(dp->vccio_supply);

		if (dp->vcc_supply)
			regulator_disable(dp->vcc_supply);
	}

	return 0;
}

static struct drm_encoder_helper_funcs rockchip_dp_encoder_helper_funcs = {
	.mode_fixup = rockchip_dp_drm_encoder_mode_fixup,
	.mode_set = rockchip_dp_drm_encoder_mode_set,
	.enable = rockchip_dp_drm_encoder_enable,
	.disable = rockchip_dp_drm_encoder_nop,
	.atomic_check = rockchip_dp_drm_encoder_atomic_check,
	.loader_protect = rockchip_dp_drm_encoder_loader_protect,
};

static void rockchip_dp_drm_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static struct drm_encoder_funcs rockchip_dp_encoder_funcs = {
	.destroy = rockchip_dp_drm_encoder_destroy,
};

static int rockchip_dp_init(struct rockchip_dp_device *dp)
{
	struct device *dev = dp->dev;
	struct device_node *np = dev->of_node;
	int ret;

	dp->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(dp->grf))
	{
		dev_err(dev, "failed to get rockchip,grf property\n");
		return PTR_ERR(dp->grf);
	}

	dp->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(dp->pclk))
	{
		dev_err(dev, "failed to get pclk property\n");
		return PTR_ERR(dp->pclk);
	}

	dp->rst = devm_reset_control_get(dev, "dp");
	if (IS_ERR(dp->rst))
	{
		dev_err(dev, "failed to get dp reset control\n");
		return PTR_ERR(dp->rst);
	}

	dp->vcc_supply = devm_regulator_get_optional(dev, "vcc");
	if (IS_ERR(dp->vcc_supply))
	{
		if (PTR_ERR(dp->vcc_supply) != -ENODEV)
		{
			ret = PTR_ERR(dp->vcc_supply);
			dev_err(dev, "failed to get vcc regulator: %d\n", ret);
			return ret;
		}

		dp->vcc_supply = NULL;
	}

	dp->vccio_supply = devm_regulator_get_optional(dev, "vccio");
	if (IS_ERR(dp->vccio_supply))
	{
		if (PTR_ERR(dp->vccio_supply) != -ENODEV)
		{
			ret = PTR_ERR(dp->vccio_supply);
			dev_err(dev, "failed to get vccio regulator: %d\n",
					ret);
			return ret;
		}

		dp->vccio_supply = NULL;
	}

	return 0;
}

static int rockchip_dp_drm_create_encoder(struct rockchip_dp_device *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_device *drm_dev = dp->drm_dev;
	struct device *dev = dp->dev;
	int ret;

	encoder->port = dev->of_node;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev,
														 dev->of_node);
	dev_info(dp->dev, "possible_crtcs = x%x\n", encoder->possible_crtcs);

	ret = drm_encoder_init(drm_dev, encoder, &rockchip_dp_encoder_funcs,
						   DRM_MODE_ENCODER_TMDS, NULL);
	if (ret)
	{
		dev_info(dp->dev, "failed to initialize encoder with drm\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &rockchip_dp_encoder_helper_funcs);

	return 0;
}

static const struct regmap_config rockchip_dp_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int dp_i2c_read(struct rockchip_dp_device *dp,
					   unsigned char *buf, unsigned int length)
{
	struct dp_i2c *i2c = dp->i2c;
	int stat;

	if (!i2c->is_regaddr)
	{
		dev_dbg(dp->dev, "set read register address to 0\n");
		i2c->slave_reg = 0x00;
		i2c->is_regaddr = true;
	}

	/* while (length--) {
			reinit_completion(&i2c->cmp);

			hdmi_writeb(hdmi, i2c->slave_reg++, HDMI_I2CM_ADDRESS);
			if (i2c->is_segment)
					hdmi_writeb(hdmi, HDMI_I2CM_OPERATION_READ_EXT,
								HDMI_I2CM_OPERATION);
			else
					hdmi_writeb(hdmi, HDMI_I2CM_OPERATION_READ,
								HDMI_I2CM_OPERATION);

			stat = wait_for_completion_timeout(&i2c->cmp, HZ / 10);
			if (!stat)
					return -EAGAIN;

			/* Check for error condition on the bus
			if (i2c->stat & HDMI_IH_I2CM_STAT0_ERROR)
					return -EIO;

			*buf++ = hdmi_readb(hdmi, HDMI_I2CM_DATAI);
	} */
	i2c->is_segment = false;

	return 0;
}

static int dp_i2c_write(struct rockchip_dp_device *dp,
						unsigned char *buf, unsigned int length)
{
	struct dp_i2c *i2c = dp->i2c;
	int stat;

	if (!i2c->is_regaddr)
	{
		/* Use the first write byte as register address */
		i2c->slave_reg = buf[0];
		length--;
		buf++;
		i2c->is_regaddr = true;
	}

	/* while (length--) {
			reinit_completion(&i2c->cmp);

			hdmi_writeb(hdmi, *buf++, HDMI_I2CM_DATAO);
			hdmi_writeb(hdmi, i2c->slave_reg++, HDMI_I2CM_ADDRESS);
			hdmi_writeb(hdmi, HDMI_I2CM_OPERATION_WRITE,
						HDMI_I2CM_OPERATION);

	stat = wait_for_completion_timeout(&i2c->cmp, HZ / 10);
			if (!stat)
					return -EAGAIN;

			/* Check for error condition on the bus
			if (i2c->stat & HDMI_IH_I2CM_STAT0_ERROR)
					return -EIO;
	} */

	return 0;
}

static int dp_i2c_xfer(struct i2c_adapter *adap,
					   struct i2c_msg *msgs, int num)
{
	struct rockchip_dp_device *dp = i2c_get_adapdata(adap);
	struct dp_i2c *i2c = dp->i2c;
	u8 addr = msgs[0].addr;
	int i, ret = 0;

	dev_info(dp->dev, "xfer: num: %d, addr: %#x\n", num, addr);

	for (i = 0; i < num; i++)
	{
		if (msgs[i].len == 0)
		{
			dev_info(dp->dev,
					 "unsupported transfer %d/%d, no data\n",
					 i + 1, num);
			return -EOPNOTSUPP;
		}
	}

	mutex_lock(&i2c->lock);

	// hdmi_writeb(hdmi, 0x00, HDMI_IH_MUTE_I2CM_STAT0);

	/* Set slave device address taken from the first I2C message */
	/* if (addr == DDC_SEGMENT_ADDR && msgs[0].len == 1)
			addr = DDC_ADDR; */
	// hdmi_writeb(hdmi, 0x2b, HDMI_I2CM_SLAVE);

	/* Set slave device register address on transfer */
	i2c->is_regaddr = false;

	/* Set segment pointer for I2C extended read mode operation */
	i2c->is_segment = false;

	for (i = 0; i < num; i++)
	{
		// dev_info(dp->dev, "xfer: num: %d/%d, len: %d, flags: %#x\n",
		//        i + 1, num, msgs[i].len, msgs[i].flags);
		if (msgs[i].addr == 0x2b && msgs[i].len == 1)
		{
			i2c->is_segment = true;
			// hdmi_writeb(hdmi, DDC_SEGMENT_ADDR, HDMI_I2CM_SEGADDR);
			// hdmi_writeb(hdmi, *msgs[i].buf, HDMI_I2CM_SEGPTR);
		}
		else
		{
			if (msgs[i].flags & I2C_M_RD)
				ret = dp_i2c_read(dp, msgs[i].buf,
								  msgs[i].len);
			else
				ret = dp_i2c_write(dp, msgs[i].buf,
								   msgs[i].len);
		}
		if (ret < 0)
			break;
	}
	if (!ret)
		ret = num;

	/* Mute DONE and ERROR interrupts */
	// hdmi_writeb(hdmi, HDMI_IH_I2CM_STAT0_ERROR | HDMI_IH_I2CM_STAT0_DONE,
	//            HDMI_IH_MUTE_I2CM_STAT0);

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 dp_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm dp_algorithm = {
	.master_xfer = dp_i2c_xfer,
	.functionality = dp_i2c_func,
};

static struct i2c_adapter *dp_i2c_adapter(struct rockchip_dp_device *dp)
{
	struct i2c_adapter *adap;
	struct dp_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(dp->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);

	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = dp->dev;
	adap->dev.of_node = dp->dev->of_node;
	adap->algo = &dp_algorithm;
	strlcpy(adap->name, "lt7911d", sizeof(adap->name));
	i2c_set_adapdata(adap, dp);

	ret = i2c_add_adapter(adap);
	if (ret)
	{
		dev_warn(dp->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(dp->dev, i2c);
		return ERR_PTR(ret);
	}

	dp->i2c = i2c;

	dev_info(dp->dev, "registered %s I2C bus driver\n", adap->name);

	return adap;
}

static const struct regmap_config dp_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int dp_i2c_init(struct rockchip_dp_device *dp)
{
	struct regmap *regmap;
	unsigned int i;
	int ret;

	struct i2c_client *client = dp->client;

	regmap = devm_regmap_init_i2c(client, &dp_regmap_config);
	if (IS_ERR(regmap))
	{
		ret = PTR_ERR(regmap);
		dev_info(dp->dev,
				 "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	dp->regmap[i] = regmap;

	return 0;
}

static int rockchip_dp_bind(struct device *dev, struct device *master,
							void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);
	const struct rockchip_dp_chip_data *dp_data;
	struct drm_panel *panel = NULL;
	// struct drm_bridge *bridge = return_bridge(dev);
	struct drm_bridge *bridge = NULL;
	struct drm_device *drm_dev = data;
	struct i2c_adapter *adapter;
	int ret;

	printk(KERN_DEBUG "Rockchip DP Bind\n");

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, -1, &panel, &bridge);
	dev_info(dp->dev, "Finding drm panel or bridge\n");
	if (ret)
	{
		dev_info(dp->dev, "Finding drm panel or bridge failed\n");
		return ret;
	}

	dp->plat_data.panel = panel;
	dp->plat_data.bridge = bridge;
	// dp->plat_data.bridge = return_bridge(to_platform_device(dev));
	// dp->plat_data.bridge = return_bridge(of_find_device_by_node(dev->of_node->child));

	dp_data = of_device_get_match_data(dev);
	dev_info(dp->dev, "Device matching\n");
	if (!dp_data)
	{
		dev_info(dp->dev, "Device doesn't match\n");
		return -ENODEV;
	}

	ret = rockchip_dp_init(dp);
	if (ret < 0)
		return ret;

	dp->data = dp_data;
	dp->drm_dev = drm_dev;

	ret = rockchip_dp_drm_create_encoder(dp);
	dev_info(dp->dev, "creating drm encoder\n");
	if (ret)
	{
		dev_info(dp->dev, "failed to create drm encoder\n");
		return ret;
	}

	dp->plat_data.encoder = &dp->encoder;
	dev_info(dp->dev, "Setted DP Encoder\n");
	dp->plat_data.dev_type = ROCKCHIP_DP;
	dev_info(dp->dev, "Device type is Rockchip DP\n");
	dp->plat_data.subdev_type = dp_data->chip_type;
	dev_info(dp->dev, "Subdev Type is chip type\n");
	dp->plat_data.power_on = rockchip_dp_poweron;
	dev_info(dp->dev, "Power on rockchip DP\n");
	dp->plat_data.power_off = rockchip_dp_powerdown;
	dev_info(dp->dev, "Power off rockchip DP\n");
	dp->plat_data.get_modes = rockchip_dp_get_modes;
	dev_info(dp->dev, "Get modes rockchip DP\n");

	dp->adp = analogix_dp_bind(dev, dp->drm_dev, &dp->plat_data);
	printk(KERN_DEBUG "Analogix DP bind");
	if (IS_ERR(dp->adp))
	{
		printk(KERN_DEBUG "Problem in analogix DP bind");
		return PTR_ERR(dp->adp);
	}

	/* node = of_parse_phandle(dev->of_node, "i2c-bus", 0);
		if (!node) {
				dev_err(dev, "No i2c-bus found\n");
				return -ENODEV;
		}
		dev_info(dev, "Node found: %s: %s", node->full_name, dev->of_node->full_name); */

	/* adapter = dp_i2c_adapter(dp);
	// adapter = i2c_get_adapter(7);
	// of_node_put(node);
	if (!adapter) {
			dev_err(dev, "No i2c adapter found\n");
			return -EPROBE_DEFER;
	}
	dev_info(dev, "I2C Adapter found");

	ret = dp_i2c_init(dp, adapter);
	if (ret)
			return ret; */

	// dp_init(dp);

	return 0;
}

static void rockchip_dp_unbind(struct device *dev, struct device *master,
							   void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	analogix_dp_unbind(dp->adp);
}

static const struct component_ops rockchip_dp_component_ops = {
	.bind = rockchip_dp_bind,
	.unbind = rockchip_dp_unbind,
};

static int rockchip_dp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rockchip_dp_device *dp;

	dev_info(dev, "DP probbed!\n");

	dp = devm_kzalloc(dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->dev = dev;

	platform_set_drvdata(pdev, dp);

	return component_add(dev, &rockchip_dp_component_ops);
}

static int rockchip_dp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rockchip_dp_component_ops);

	return 0;
}

static int dp_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct rockchip_dp_device *dp;
	int ret;

	dev_info(dev, "Initializing lt7911d i2c\n");

	dp = devm_kzalloc(dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->dev = dev;
	dp->client = client;
	i2c_set_clientdata(client, dp);

	dev_info(dev, "Client data set\n");
	ret = dp_i2c_init(dp);
	if (ret < 0)
		dev_dbg(dev, "Creating I2C regmap failed");

	dp_init(dp);
	return 0;
}

static int dp_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id dp_i2c_id[] = {
	{"lt7911d"},
	{}};

#ifdef CONFIG_PM_SLEEP
static int rockchip_dp_suspend(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return analogix_dp_suspend(dp->adp);
}

static int rockchip_dp_resume(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return analogix_dp_resume(dp->adp);
}
#endif

static const struct dev_pm_ops rockchip_dp_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend_late = rockchip_dp_suspend,
	.resume_early = rockchip_dp_resume,
#endif
};

static const struct rockchip_dp_chip_data rk3399_edp = {
	.lcdsel_grf_reg = 0x6250,
	.lcdsel_big = 0 | BIT(21),
	.lcdsel_lit = BIT(5) | BIT(21),
	.chip_type = RK3399_EDP,
	.has_vop_sel = true,
};

static const struct rockchip_dp_chip_data rk3368_edp = {
	.chip_type = RK3368_EDP,
};

static const struct rockchip_dp_chip_data rk3288_dp = {
	.lcdsel_grf_reg = 0x025c,
	.lcdsel_big = 0 | BIT(21),
	.lcdsel_lit = BIT(5) | BIT(21),
	.chip_type = RK3288_DP,
	.has_vop_sel = true,
};

static const struct of_device_id dp_i2c_match[] = {
	{.compatible = "rockchip,lt7911d"},
	{}};
MODULE_DEVICE_TABLE(of, dp_i2c_match);

MODULE_DEVICE_TABLE(i2c, dp_i2c_id);
static struct i2c_driver dp_i2c_driver = {
	.driver = {
		.name = "rockchip,lt7911d",
		.of_match_table = of_match_ptr(dp_i2c_match),
	},
	.probe = dp_i2c_probe,
	.remove = dp_i2c_remove,
	.id_table = dp_i2c_id,
};

/* static int __init dp_i2c_driver_init(void)
{
	return i2c_add_driver(&dp_i2c_driver);
}

static void __exit dp_i2c_driver_exit(void)
{
	i2c_del_driver(&dp_i2c_driver);
} */

// device_initcall_sync(dp_i2c_driver_init);
// module_init(dp_i2c_driver_init);
// module_exit(dp_i2c_driver_exit);
// module_i2c_driver(dp_i2c_driver);

static const struct of_device_id rockchip_dp_dt_ids[] = {
	{.compatible = "rockchip,rk3288-dp", .data = &rk3288_dp},
	{.compatible = "rockchip,rk3368-edp", .data = &rk3368_edp},
	{.compatible = "rockchip,rk3399-edp", .data = &rk3399_edp},
	{}};
MODULE_DEVICE_TABLE(of, rockchip_dp_dt_ids);

static struct platform_driver rockchip_dp_driver = {
	.probe = rockchip_dp_probe,
	.remove = rockchip_dp_remove,
	.driver = {
		.name = "rockchip-dp",
		.owner = THIS_MODULE,
		.pm = &rockchip_dp_pm_ops,
		.of_match_table = of_match_ptr(rockchip_dp_dt_ids),
	},
};

module_platform_driver(rockchip_dp_driver);

MODULE_AUTHOR("Yakir Yang <ykk@rock-chips.com>");
MODULE_AUTHOR("Jeff chen <jeff.chen@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip Specific Analogix-DP Driver Extension");
MODULE_LICENSE("GPL v2");
