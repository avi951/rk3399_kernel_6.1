/*
 * DJ <d.kabutarwala@yahoo.com>
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <video/of_display_timing.h>

#include <drm/drmP.h>
#include <drm/drm_of.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>

#include "../bridge/analogix/analogix_dp_core.h"

#include "lt7911d.h"

#define I2C_BUS_AVAILABLE (	7)

struct lt7911d {
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct drm_display_mode mode;
	struct device *dev;
	struct i2c_client *client;
	struct analogix_dp_device *dp;
	struct regmap *regmap[3];
	struct gpio_desc *reset_n;
};

struct drm_bridge * lt7911d_bridge_dp;

struct lt7911d_bridge_info {
	unsigned int connector_type;
};

static inline struct lt7911d *bridge_to_lt7911d(struct drm_bridge *b)
{
	return container_of(b, struct lt7911d, bridge);
}

static inline struct lt7911d *connector_to_lt7911d(struct drm_connector *c)
{
	return container_of(c, struct lt7911d, connector);
}

/* lt7911d MIPI to HDMI & LVDS REG setting - 20180115.txt */
static void lt7911d_init(struct lt7911d *lt7911d)
{
	u8 lanes = 4;
	const struct drm_display_mode *mode = &lt7911d->mode;
	u32 hactive, hfp, hsync, hbp, vfp, vsync, vbp, htotal, vtotal;
	unsigned int version[2];

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

	// get chip id
	/* regmap_write(lt7911d->regmap[0], 0xff, 0x80);
	regmap_write(lt7911d->regmap[0], 0xee, 0x01);

	regmap_write(lt7911d->regmap[0], 0xff, 0xa0);

	regmap_read(lt7911d->regmap[0], 0x00, &version[0]);
	regmap_read(lt7911d->regmap[0], 0x01, &version[1]);

	dev_info(lt7911d->dev, "lt7911d ID: %02x, %02x\n",
	 	 version[0], version[1]); */

	// /* DigitalClockEn */
	// regmap_write(lt7911d->regmap[0], 0x08, 0xff);
	// regmap_write(lt7911d->regmap[0], 0x09, 0x81);
	// regmap_write(lt7911d->regmap[0], 0x0a, 0xff);
	// regmap_write(lt7911d->regmap[0], 0x0b, 0x64);
	// regmap_write(lt7911d->regmap[0], 0x0c, 0xff);

	// regmap_write(lt7911d->regmap[0], 0x44, 0x31);
	// regmap_write(lt7911d->regmap[0], 0x51, 0x1f);

	// /* TxAnalog */
	// regmap_write(lt7911d->regmap[0], 0x31, 0xa1);
	// regmap_write(lt7911d->regmap[0], 0x32, 0xa1);
	// regmap_write(lt7911d->regmap[0], 0x33, 0x03);
	// regmap_write(lt7911d->regmap[0], 0x37, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x38, 0x22);
	// regmap_write(lt7911d->regmap[0], 0x60, 0x82);

	// /* CbusAnalog */
	// regmap_write(lt7911d->regmap[0], 0x39, 0x45);
	// regmap_write(lt7911d->regmap[0], 0x3b, 0x00);

	// /* HDMIPllAnalog */
	// regmap_write(lt7911d->regmap[0], 0x44, 0x31);
	// regmap_write(lt7911d->regmap[0], 0x55, 0x44);
	// regmap_write(lt7911d->regmap[0], 0x57, 0x01);
	// regmap_write(lt7911d->regmap[0], 0x5a, 0x02);

	// /* MipiBasicSet */
	// regmap_write(lt7911d->regmap[1], 0x10, 0x01);
	// regmap_write(lt7911d->regmap[1], 0x11, 0x08);
	// regmap_write(lt7911d->regmap[1], 0x12, 0x04);
	// regmap_write(lt7911d->regmap[1], 0x13, lanes % 4);
	// regmap_write(lt7911d->regmap[1], 0x14, 0x00);

	// regmap_write(lt7911d->regmap[1], 0x15, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x1a, 0x03);
	// regmap_write(lt7911d->regmap[1], 0x1b, 0x03);

	// /* MIPIDig */
	// regmap_write(lt7911d->regmap[1], 0x18, hsync);
	// regmap_write(lt7911d->regmap[1], 0x19, vsync);
	// regmap_write(lt7911d->regmap[1], 0x1c, hactive);
	// regmap_write(lt7911d->regmap[1], 0x1d, hactive >> 8);

	// regmap_write(lt7911d->regmap[1], 0x1e, 0x67);
	// regmap_write(lt7911d->regmap[1], 0x2f, 0x0c);

	// regmap_write(lt7911d->regmap[1], 0x34, htotal);
	// regmap_write(lt7911d->regmap[1], 0x35, htotal >> 8);
	// regmap_write(lt7911d->regmap[1], 0x36, vtotal);
	// regmap_write(lt7911d->regmap[1], 0x37, vtotal >> 8);
	// regmap_write(lt7911d->regmap[1], 0x38, vbp);
	// regmap_write(lt7911d->regmap[1], 0x39, vbp >> 8);
	// regmap_write(lt7911d->regmap[1], 0x3a, vfp);
	// regmap_write(lt7911d->regmap[1], 0x3b, vfp >> 8);
	// regmap_write(lt7911d->regmap[1], 0x3c, hbp);
	// regmap_write(lt7911d->regmap[1], 0x3d, hbp >> 8);
	// regmap_write(lt7911d->regmap[1], 0x3e, hfp);
	// regmap_write(lt7911d->regmap[1], 0x3f, hfp >> 8);

	// /* DDSConfig */
	// regmap_write(lt7911d->regmap[1], 0x4e, 0x52);
	// regmap_write(lt7911d->regmap[1], 0x4f, 0xde);
	// regmap_write(lt7911d->regmap[1], 0x50, 0xc0);
	// regmap_write(lt7911d->regmap[1], 0x51, 0x80);
	// regmap_write(lt7911d->regmap[1], 0x51, 0x00);

	// regmap_write(lt7911d->regmap[1], 0x1f, 0x5e);
	// regmap_write(lt7911d->regmap[1], 0x20, 0x01);
	// regmap_write(lt7911d->regmap[1], 0x21, 0x2c);
	// regmap_write(lt7911d->regmap[1], 0x22, 0x01);
	// regmap_write(lt7911d->regmap[1], 0x23, 0xfa);
	// regmap_write(lt7911d->regmap[1], 0x24, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x25, 0xc8);
	// regmap_write(lt7911d->regmap[1], 0x26, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x27, 0x5e);
	// regmap_write(lt7911d->regmap[1], 0x28, 0x01);
	// regmap_write(lt7911d->regmap[1], 0x29, 0x2c);
	// regmap_write(lt7911d->regmap[1], 0x2a, 0x01);
	// regmap_write(lt7911d->regmap[1], 0x2b, 0xfa);
	// regmap_write(lt7911d->regmap[1], 0x2c, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x2d, 0xc8);
	// regmap_write(lt7911d->regmap[1], 0x2e, 0x00);

	// regmap_write(lt7911d->regmap[0], 0x03, 0x7f);
	// usleep_range(10000, 20000);
	// regmap_write(lt7911d->regmap[0], 0x03, 0xff);

	// regmap_write(lt7911d->regmap[1], 0x42, 0x64);
	// regmap_write(lt7911d->regmap[1], 0x43, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x44, 0x04);
	// regmap_write(lt7911d->regmap[1], 0x45, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x46, 0x59);
	// regmap_write(lt7911d->regmap[1], 0x47, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x48, 0xf2);
	// regmap_write(lt7911d->regmap[1], 0x49, 0x06);
	// regmap_write(lt7911d->regmap[1], 0x4a, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x4b, 0x72);
	// regmap_write(lt7911d->regmap[1], 0x4c, 0x45);
	// regmap_write(lt7911d->regmap[1], 0x4d, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x52, 0x08);
	// regmap_write(lt7911d->regmap[1], 0x53, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x54, 0xb2);
	// regmap_write(lt7911d->regmap[1], 0x55, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x56, 0xe4);
	// regmap_write(lt7911d->regmap[1], 0x57, 0x0d);
	// regmap_write(lt7911d->regmap[1], 0x58, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x59, 0xe4);
	// regmap_write(lt7911d->regmap[1], 0x5a, 0x8a);
	// regmap_write(lt7911d->regmap[1], 0x5b, 0x00);
	// regmap_write(lt7911d->regmap[1], 0x5c, 0x34);
	// regmap_write(lt7911d->regmap[1], 0x1e, 0x4f);
	// regmap_write(lt7911d->regmap[1], 0x51, 0x00);

	// regmap_write(lt7911d->regmap[0], 0xb2, 0x01);

	// /* AudioIIsEn */
	// regmap_write(lt7911d->regmap[2], 0x06, 0x08);
	// regmap_write(lt7911d->regmap[2], 0x07, 0xf0);

	// regmap_write(lt7911d->regmap[2], 0x34, 0xd2);

	// regmap_write(lt7911d->regmap[2], 0x3c, 0x41);

	// /* MIPIRxLogicRes */
	// regmap_write(lt7911d->regmap[0], 0x03, 0x7f);
	// usleep_range(10000, 20000);
	// regmap_write(lt7911d->regmap[0], 0x03, 0xff);

	// regmap_write(lt7911d->regmap[1], 0x51, 0x80);
	// usleep_range(10000, 20000);
	// regmap_write(lt7911d->regmap[1], 0x51, 0x00);
}

static void lt7911d_exit(struct lt7911d *lt7911d)
{
	// regmap_write(lt7911d->regmap[0], 0x08, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x09, 0x81);
	// regmap_write(lt7911d->regmap[0], 0x0a, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x0b, 0x20);
	// regmap_write(lt7911d->regmap[0], 0x0c, 0x00);

	// regmap_write(lt7911d->regmap[0], 0x54, 0x1d);
	// regmap_write(lt7911d->regmap[0], 0x51, 0x15);

	// regmap_write(lt7911d->regmap[0], 0x44, 0x31);
	// regmap_write(lt7911d->regmap[0], 0x41, 0xbd);
	// regmap_write(lt7911d->regmap[0], 0x5c, 0x11);

	// regmap_write(lt7911d->regmap[0], 0x30, 0x08);
	// regmap_write(lt7911d->regmap[0], 0x31, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x32, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x33, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x34, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x35, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x36, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x37, 0x00);
	// regmap_write(lt7911d->regmap[0], 0x38, 0x00);
}

static void lt7911d_power_on(struct lt7911d *lt7911d)
{
	dev_info(lt7911d->dev, "Reset High");
	// gpiod_direction_output(lt7911d->reset_n, 1);
	msleep(120);
	dev_info(lt7911d->dev, "Reset Low");
	// gpiod_direction_output(lt7911d->reset_n, 0);
}

static void lt7911d_power_off(struct lt7911d *lt7911d)
{
	// gpiod_direction_output(lt7911d->reset_n, 1);
}

static enum drm_connector_status
lt7911d_connector_detect(struct drm_connector *connector, bool force)
{
	/* TODO: HPD handing (reg[0xc1] - bit[7]) */
	return connector_status_connected;
}

static const struct drm_connector_funcs lt7911d_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = lt7911d_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_encoder *
lt7911d_connector_best_encoder(struct drm_connector *connector)
{
	struct lt7911d *lt7911d = connector_to_lt7911d(connector);

	return lt7911d->bridge.encoder;
}

static int lt7911d_connector_get_modes(struct drm_connector *connector)
{
	int num_modes = 0;

	num_modes = drm_add_modes_noedid(connector, 1920, 1080);
	drm_set_preferred_mode(connector, 1920, 1080);

	return num_modes;
}

static const struct drm_connector_helper_funcs lt7911d_connector_helper_funcs = {
	.get_modes = lt7911d_connector_get_modes,
	.best_encoder = lt7911d_connector_best_encoder,
};

static void lt7911d_bridge_post_disable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);

	lt7911d_power_off(lt7911d);
}

static void lt7911d_bridge_disable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);

	lt7911d_exit(lt7911d);
}

static void lt7911d_bridge_enable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);

	lt7911d_init(lt7911d);
}

static void lt7911d_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);

	lt7911d_power_on(lt7911d);
}

static void lt7911d_bridge_mode_set(struct drm_bridge *bridge,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adj)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);

	drm_mode_copy(&lt7911d->mode, adj);
}

static int lt7911d_bridge_attach(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = bridge_to_lt7911d(bridge);
	struct drm_connector *connector = &lt7911d->connector;
	int ret;

	ret = drm_connector_init(bridge->dev, connector,
				 &lt7911d_connector_funcs,
				 DRM_MODE_CONNECTOR_eDP);
	if (ret) {
		dev_err(lt7911d->dev, "failed to initialize connector\n");
		return ret;
	}

	drm_connector_helper_add(connector, &lt7911d_connector_helper_funcs);
	drm_mode_connector_attach_encoder(connector, bridge->encoder);

	return 0;
}

static const struct drm_bridge_funcs lt7911d_bridge_funcs = {
	.attach = lt7911d_bridge_attach,
	.mode_set = lt7911d_bridge_mode_set,
	.pre_enable = lt7911d_bridge_pre_enable,
	.enable = lt7911d_bridge_enable,
	.disable = lt7911d_bridge_disable,
	.post_disable = lt7911d_bridge_post_disable,
};

static const struct regmap_config lt7911d_regmap_config = {
 	.reg_bits = 8,
 	.val_bits = 8,
 	.max_register = 0xff,
};

static int lt7911d_i2c_init(struct lt7911d *lt7911d,
 			   struct i2c_adapter *adapter)
{
 	struct i2c_board_info info[] = {
 		{ I2C_BOARD_INFO("lt7911d", 0x2b), }
 		// { I2C_BOARD_INFO("lt7911dp1", 0x2c), },
 		// { I2C_BOARD_INFO("lt7911dp2", 0x44), }
 	};
 	struct regmap *regmap;
 	unsigned int i;
 	int ret;

 	for (i = 0; i < ARRAY_SIZE(info); i++) {
 		struct i2c_client *client;

 		client = i2c_new_device(adapter, &info[i]);
 		if (!client)
 			return -ENODEV;

 		regmap = devm_regmap_init_i2c(client, &lt7911d_regmap_config);
 		if (IS_ERR(regmap)) {
 			ret = PTR_ERR(regmap);
 			dev_err(lt7911d->dev,
 				"Failed to initialize regmap: %d\n", ret);
 			return ret;
 		}

 		lt7911d->regmap[i] = regmap;
 	}

 	return 0;
 }

static int lt7911d_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct lt7911d *lt7911d;
	u32 val;
	int ret;

	lt7911d = devm_kzalloc(dev, sizeof(*lt7911d), GFP_KERNEL);
	if(!lt7911d)
		return -ENOMEM;

	lt7911d->dev = dev;
	lt7911d->client = client;
	i2c_set_clientdata(client, lt7911d);

	lt7911d_i2c_init(lt7911d, client->adapter);

	return 0;
}

static int lt7911d_probe(struct platform_device *pdev)
// static int lt7911d_probe(struct device *dev)
{
	struct device *dev = &pdev->dev;
	struct lt7911d *lt7911d;
	struct device_node *node;
	struct i2c_adapter *adapter;
	int ret;

	dev_info(dev, "LT7911D probbed!");

	lt7911d = devm_kzalloc(dev, sizeof(*lt7911d), GFP_KERNEL);
	if (!lt7911d)
		return -ENOMEM;

	lt7911d->dev = dev;
	// lt7911d->dp->dev = dev;
	platform_set_drvdata(pdev, lt7911d);

	lt7911d->reset_n = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(lt7911d->reset_n)) {
		ret = PTR_ERR(lt7911d->reset_n);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		// return ret;
	}

	dev_info(dev, "Device Name: %s", dev->init_name);

	node = of_parse_phandle(dev->of_node, "i2c-bus", 0);
	if (!node) {
		dev_err(dev, "No i2c-bus found\n");
		return -ENODEV;
	}
	dev_info(dev, "Node found: %s: %s", node->full_name, dev->of_node->full_name);

	/* adapter = of_find_i2c_adapter_by_node(node);
	// adapter = i2c_get_adapter(7);
	of_node_put(node);
	if (!adapter) {
		dev_err(dev, "No i2c adapter found\n");
		return -EPROBE_DEFER;
	}
	dev_info(dev, "I2C Adapter found");

	ret = lt7911d_i2c_init(lt7911d, adapter);
	if (ret)
		return ret;

	/* TODO: interrupt handing */

	lt7911d->bridge.funcs = &lt7911d_bridge_funcs;
	lt7911d->bridge.of_node = dev->of_node;
	ret = drm_bridge_add(&lt7911d->bridge);
	if (ret) {
		dev_err(dev, "failed to add bridge: %d\n", ret);
		return ret;
	}
	// Init LT7911D bridge
	lt7911d_init(lt7911d);

	lt7911d_bridge_dp = &(lt7911d->bridge);

	return 0;
}

struct drm_bridge * return_bridge(struct platform_device * pdev) {
	lt7911d_probe(pdev);
	return lt7911d_bridge_dp;
}

static int lt7911d_remove(struct platform_device *pdev)
{
	struct lt7911d *lt7911d = platform_get_drvdata(pdev);

	drm_bridge_remove(&lt7911d->bridge);

	return 0;
}

static const struct of_device_id lt7911d_bridge_match[] = {
	{
		.compatible = "lontium,lt7911d-bridge",
		.data =
			&(const struct lt7911d_bridge_info){
				.connector_type = DRM_MODE_CONNECTOR_eDP,
			},
	},
	{},
};
MODULE_DEVICE_TABLE(of, lt7911d_bridge_match);

static struct platform_driver lt7911d_bridge_driver = {
    .probe = lt7911d_probe,
    .remove = lt7911d_remove,
    .driver = {
        .name = "lt7911d",
        .of_match_table = lt7911d_bridge_match,
    },
};
module_platform_driver(lt7911d_bridge_driver);

MODULE_AUTHOR("DJ <d.kabutarwala@yahoo.com>");
MODULE_DESCRIPTION("Lontium LT7911D bridge driver");
MODULE_LICENSE("GPL");
