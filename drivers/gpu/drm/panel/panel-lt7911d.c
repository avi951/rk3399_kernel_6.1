#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/mipi_display.h>
#include <linux/of_device.h>
#include <video/of_display_timing.h>
#include <linux/of_graph.h>
#include <video/videomode.h>

struct panel_lt7911 {
	struct drm_panel base;
	bool prepared;
	bool enabled;

	struct device *dev;
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	struct regulator *supply;
	struct i2c_adapter *ddc;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
};

static void lt7911d_sleep(unsigned int msec)
{
	if(msec > 20)
		msleep(msec);
	else
		usleep_range(msec *1000, (msec+1)*1000);
}

static inline struct panel_lt7911 *to_panel_lt7911(struct drm_panel *panel)
{
	return container_of(panel, struct panel_lt7911, base);
}

static int panel_lt7911_of_get_native_mode(struct panel_lt7911 *panel)
{
	struct drm_connector *connector = panel->base.connector;
	struct drm_device *drm = panel->base.drm;
	struct drm_display_mode *mode;
	struct device_node *timings_np;
	int ret;

	timings_np = of_get_child_by_name(panel->dev->of_node, "display-timing");

	if(!timings_np) {
		dev_dbg(panel->dev, "failed to find display-timings node\n");
		return 0;
	}

	of_node_put(timings_np);
	mode = drm_mode_create(drm);
	if(!mode)
		return 0;

	ret = of_get_drm_display_mode(panel->dev->of_node, mode,
						OF_USE_NATIVE_MODE);
	if(ret) {
		dev_dbg(panel->dev, "failed to find dts display timings\n");
		drm_mode_destroy(drm, mode);
		return 0;
	}

	drm_mode_set_name(mode);
	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static int panel_lt7911_regulator_enable(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err = 0;

	err = regulator_enable(p->supply);
	if(err < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n", err);
		return err;
	}

	return err;
}

static int panel_lt7911_regulator_disable(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err = 0;

	regulator_disable(p->supply);

	return err;
}

static int panel_lt7911_loader_protect(struct drm_panel *panel, bool on)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err;

	if (on) {
		err = panel_lt7911_regulator_enable(panel);
		if (err < 0) {
			dev_err(panel->dev, "failed to enable supply: %d\n",
				err);
			return err;
		}

		p->prepared = true;
		p->enabled = true;
	}

	return 0;
}

static int panel_lt7911_disable(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err = 0;

	if (!p->enabled)
		return 0;

	p->enabled = false;

	return 0;
}

static int panel_lt7911_unprepare(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err = 0;

	if (!p->prepared)
		return 0;

	if (p->reset_gpio)
		gpiod_direction_output(p->reset_gpio, 1);

	if (p->enable_gpio)
		gpiod_direction_output(p->enable_gpio, 0);

	panel_lt7911_regulator_disable(panel);

	p->prepared = false;

	return 0;
}

static int panel_lt7911_prepare(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err;

	if (p->prepared)
		return 0;

	err = panel_lt7911_regulator_enable(panel);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n", err);
		return err;
	}

	if (p->enable_gpio)
		gpiod_direction_output(p->enable_gpio, 1);

	if (p->reset_gpio)
		gpiod_direction_output(p->reset_gpio, 1);

	if (p->reset_gpio)
		gpiod_direction_output(p->reset_gpio, 0);

	p->prepared = true;

	return 0;
}

static int panel_lt7911_enable(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int err = 0;

	if (p->enabled)
		return 0;

	p->enabled = true;

	return 0;
}

static int panel_lt7911_get_modes(struct drm_panel *panel)
{
	struct panel_lt7911 *p = to_panel_lt7911(panel);
	int num = 0;

	/* add device node plane modes */
	num += panel_lt7911_of_get_native_mode(p);

	/* add hard-coded panel modes */
	//num += panel_simple_get_fixed_modes(p);

	/* probe EDID if a DDC bus is available */
	if (p->ddc) {
		struct edid *edid = drm_get_edid(panel->connector, p->ddc);
		drm_mode_connector_update_edid_property(panel->connector, edid);
		if (edid) {
			num += drm_add_edid_modes(panel->connector, edid);
			kfree(edid);
		}
	}

	return num;
}

static const struct drm_panel_funcs panel_simple_funcs = {
	.loader_protect = panel_lt7911_loader_protect,
	.disable = panel_lt7911_disable,
	.unprepare = panel_lt7911_unprepare,
	.prepare = panel_lt7911_prepare,
	.enable = panel_lt7911_enable,
	.get_modes = panel_lt7911_get_modes,
};

static int panel_lt7911_probe(struct device *dev)
{
	struct device_node *backlight, *ddc;
	struct panel_lt7911 *panel;
	u32 val;
	int err;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled = false;
	panel->prepared = false;
	panel->dev = dev;

	panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(panel->supply)) {
		err = PTR_ERR(panel->supply);
		dev_err(dev, "failed to get power regulator: %d\n", err);
		return err;
	}

	panel->enable_gpio = devm_gpiod_get_optional(dev, "enable", 0);
	if (IS_ERR(panel->enable_gpio)) {
		err = PTR_ERR(panel->enable_gpio);
		dev_err(dev, "failed to request enable GPIO: %d\n", err);
		return err;
	}

	panel->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->reset_gpio)) {
		err = PTR_ERR(panel->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", err);
		return err;
	}

	dev_err(dev, "of_node of device is %s", dev->of_node->name);

	ddc = of_parse_phandle(dev->of_node, "ddc-i2c-bus", 0);
	dev_err(dev, "found ddc %s\n", ddc->full_name);
	if (ddc) {
		panel->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);
		dev_err(dev, "found i2c adapter %s", panel->ddc->name);

		if (!panel->ddc) {
			err = -EPROBE_DEFER;
			dev_err(dev, "failed to find ddc-i2c-bus: %d\n", err);
			//return err;
		}
	}

	dev_err(dev, "going to init panel");

	drm_panel_init(&panel->base);
	panel->base.dev = dev;
	panel->base.funcs = &panel_simple_funcs;

	err = drm_panel_add(&panel->base);
	if (err < 0)
		return err;
	else
		dev_err(dev, "Panel added successfully");

	dev_set_drvdata(dev, panel);

	dev_err(dev, "Hurray panel-lt7911 driver probed!!!!!!");

	return 0;
}

static int panel_lt7911_remove(struct device *dev)
{
	struct panel_lt7911 *panel = dev_get_drvdata(dev);

	drm_panel_detach(&panel->base);
	drm_panel_remove(&panel->base);

	panel_lt7911_disable(&panel->base);
	panel_lt7911_unprepare(&panel->base);

	if (panel->ddc)
		put_device(&panel->ddc->dev);

	return 0;
}

static void panel_lt7911_shutdown(struct device *dev)
{
	struct panel_lt7911 *panel = dev_get_drvdata(dev);

	panel_lt7911_disable(&panel->base);

	if (panel->prepared) {
		if (panel->reset_gpio)
			gpiod_direction_output(panel->reset_gpio, 1);

		if (panel->enable_gpio)
			gpiod_direction_output(panel->enable_gpio, 0);

		panel_lt7911_regulator_disable(&panel->base);
	}
}

static const struct of_device_id platform_of_match[] = {
	{
		.compatible = "lontium,lt7911",
		.data = NULL,
	}, {
		/* sentienl */
	}
};
MODULE_DEVICE_TABLE(of, platform_of_match);

static int panel_lt7911_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	dev_err(&pdev->dev, "Probing as a platform device");
	dev_err(&pdev->dev, "platform device name is: %s\n", pdev->dev.of_node->full_name);
	dev_err(&pdev->dev, "given panel is: %s\n", platform_of_match->compatible);

	id = of_match_node(platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return panel_lt7911_probe(&pdev->dev);
}

static int panel_lt7911_platform_remove(struct platform_device *pdev)
{
	return panel_lt7911_remove(&pdev->dev);
}

static void panel_lt7911_platform_shutdown(struct platform_device *pdev)
{
	panel_lt7911_shutdown(&pdev->dev);
}

static struct platform_driver panel_lt7911_platform_driver = {
	.driver = {
		.name = "panel-lt7911",
		.of_match_table = platform_of_match,
	},
	.probe = panel_lt7911_platform_probe,
	.remove = panel_lt7911_platform_remove,
	.shutdown = panel_lt7911_platform_shutdown,
};

static int __init panel_lt7911_init(void)
{
	int err;

	err = platform_driver_register(&panel_lt7911_platform_driver);
	if (err < 0)
		return err;

	return 0;
}
module_init(panel_lt7911_init);

static void __exit panel_lt7911_exit(void)
{
	platform_driver_unregister(&panel_lt7911_platform_driver);
}
module_exit(panel_lt7911_exit);

MODULE_AUTHOR("DHIMS");
MODULE_DESCRIPTION("DRM Driver for LT7911");
MODULE_LICENSE("GPL and additional rights");