#define pr_fmt(fmt) KBUILD_MODNAME " :" fmt
/*
 * Copyright (C) 2015-2016 Free Electrons
 * Copyright (C) 2015-2016 NextThing Co
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

struct lt7911d {
	struct drm_bridge	bridge;
	struct drm_connector	connector;

	struct i2c_adapter	*ddc;
	struct regulator	*vdd;
	struct gpio_desc	*gpio_rst;
};

static inline struct lt7911d *
drm_bridge_to_lt7911d(struct drm_bridge *bridge)
{
	return container_of(bridge, struct lt7911d, bridge);
}

static inline struct lt7911d *
drm_connector_to_lt7911d(struct drm_connector *connector)
{
	return container_of(connector, struct lt7911d, connector);
}

static int lt7911d_get_modes(struct drm_connector *connector)
{
	struct lt7911d *lt7911d = drm_connector_to_lt7911d(connector);
	struct edid *edid;
	int ret;

	printk("dumb going to read edid data from chip\n");

	if (IS_ERR(lt7911d->ddc)) {
		printk("dumb returning to fallback bcz of ddc error\n");
		goto fallback;
	}

	edid = drm_get_edid(connector, lt7911d->ddc);
	if (!edid) {
		DRM_INFO("EDID readout failed, falling back to standard modes\n");
		printk("dumb going to fallback bcz edid read failed\n");
		goto fallback;
	} else {
		printk("dumb got edid from chip\n");
	}

	drm_mode_connector_update_edid_property(connector, edid);
	return drm_add_edid_modes(connector, edid);

fallback:
	/*
	 * In case we cannot retrieve the EDIDs (broken or missing i2c
	 * bus), fallback on the XGA standards
	 */
	ret = drm_add_modes_noedid(connector, 1920, 1200);

	/* And prefer a mode pretty much anyone can handle */
	drm_set_preferred_mode(connector, 800, 600);

	return ret;
}

static const struct drm_connector_helper_funcs lt7911d_con_helper_funcs = {
	.get_modes	= lt7911d_get_modes,
};

static enum drm_connector_status
lt7911d_connector_detect(struct drm_connector *connector, bool force)
{
	struct lt7911d *lt7911d = drm_connector_to_lt7911d(connector);

	/*
	 * Even if we have an I2C bus, we can't assume that the cable
	 * is disconnected if drm_probe_ddc fails. Some cables don't
	 * wire the DDC pins, or the I2C bus might not be working at
	 * all.
	 */

	if (!IS_ERR(lt7911d->ddc) && drm_probe_ddc(lt7911d->ddc))
		return connector_status_connected;

	return connector_status_connected;
}

static const struct drm_connector_funcs lt7911d_con_funcs = {
	.dpms			= drm_atomic_helper_connector_dpms,
	.detect			= lt7911d_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.destroy		= drm_connector_cleanup,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};

static int lt7911d_attach(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = drm_bridge_to_lt7911d(bridge);
	int ret;

	if (!bridge->encoder) {
		DRM_ERROR("Missing encoder\n");
		return -ENODEV;
	}

	drm_connector_helper_add(&lt7911d->connector,
				 &lt7911d_con_helper_funcs);
	ret = drm_connector_init(bridge->dev, &lt7911d->connector,
				 &lt7911d_con_funcs, DRM_MODE_CONNECTOR_eDP);
	if (ret) {
		DRM_ERROR("Failed to initialize connector\n");
		return ret;
	}

	drm_mode_connector_attach_encoder(&lt7911d->connector,
					  bridge->encoder);

	return 0;
}

static void lt7911d_enable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = drm_bridge_to_lt7911d(bridge);
	int ret = 0;

	if (lt7911d->vdd)
		ret = regulator_enable(lt7911d->vdd);
	if (ret)
		DRM_ERROR("Failed to enable vdd regulator: %d\n", ret);

	if(lt7911d->gpio_rst) {
		gpiod_set_value(lt7911d->gpio_rst, 1);
		usleep_range(10, 20);
		gpiod_set_value(lt7911d->gpio_rst, 0);
	}
}

static void lt7911d_disable(struct drm_bridge *bridge)
{
	struct lt7911d *lt7911d = drm_bridge_to_lt7911d(bridge);

	if (lt7911d->vdd)
		regulator_disable(lt7911d->vdd);

	if (lt7911d->gpio_rst)
		gpiod_set_value(lt7911d->gpio_rst, 0);
}

static const struct drm_bridge_funcs lt7911d_bridge_funcs = {
	.attach		= lt7911d_attach,
	.enable		= lt7911d_enable,
	.disable	= lt7911d_disable,
};

static struct i2c_adapter *lt7911d_retrieve_ddc(struct device *dev)
{
	struct device_node *phandle, *remote;
	struct i2c_adapter *ddc;

	dev_err(dev, "node for checking ddc is: %s\n", dev->of_node->full_name);

	remote = dev->of_node;
	dev_err(dev, "found remote node: %s\n", remote->full_name);
	if (!remote)
		return ERR_PTR(-EINVAL);

	phandle = of_parse_phandle(remote, "ddc-i2c-bus", 0);
	dev_err(dev, "phandle for ddc is: %s\n", phandle->full_name);
	of_node_put(remote);
	if (!phandle)
		return ERR_PTR(-ENODEV);

	ddc = of_get_i2c_adapter_by_node(phandle);
	of_node_put(phandle);
	if (!ddc)
		return ERR_PTR(-EPROBE_DEFER);

	dev_err(dev, "ddc is: %s\n", ddc->dev.of_node->full_name);

	return ddc;
}

static int lt7911d_probe(struct platform_device *pdev)
{
	struct lt7911d *lt7911d;
	int err;

	dev_err(&pdev->dev, "Probing as a platform device");
	dev_err(&pdev->dev, "platform device name is: %s\n", pdev->dev.of_node->full_name);

	lt7911d = devm_kzalloc(&pdev->dev, sizeof(*lt7911d), GFP_KERNEL);
	if (!lt7911d)
		return -ENOMEM;
	platform_set_drvdata(pdev, lt7911d);

	lt7911d->vdd = devm_regulator_get_optional(&pdev->dev, "vdd");
	if (IS_ERR(lt7911d->vdd)) {
		int ret = PTR_ERR(lt7911d->vdd);
		if (ret == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		lt7911d->vdd = NULL;
		dev_dbg(&pdev->dev, "No vdd regulator found: %d\n", ret);
	}

	lt7911d->gpio_rst = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(lt7911d->gpio_rst)) {
		err = PTR_ERR(lt7911d->gpio_rst);
		dev_err(&pdev->dev, "cannot get gpio_rst %d\n", err);
		return err;
	}

	lt7911d->ddc = lt7911d_retrieve_ddc(&pdev->dev);
	if (IS_ERR(lt7911d->ddc)) {
		if (PTR_ERR(lt7911d->ddc) == -ENODEV) {
			dev_dbg(&pdev->dev,
				"No i2c bus specified. Disabling EDID readout\n");
		} else {
			dev_err(&pdev->dev, "Couldn't retrieve i2c bus\n");
			return PTR_ERR(lt7911d->ddc);
		}
	}

	dev_err(&pdev->dev, "retrived i2c bus is: %s", lt7911d->ddc->name);

	lt7911d->bridge.funcs = &lt7911d_bridge_funcs;
	lt7911d->bridge.of_node = pdev->dev.of_node;

	drm_bridge_add(&lt7911d->bridge);

	dev_err(&pdev->dev, "Hurray dumb-lt7911d bridge driver probed!!!!!\n");

	return 0;
}

static int lt7911d_remove(struct platform_device *pdev)
{
	struct lt7911d *lt7911d = platform_get_drvdata(pdev);

	drm_bridge_remove(&lt7911d->bridge);

	if (!IS_ERR(lt7911d->ddc))
		i2c_put_adapter(lt7911d->ddc);

	return 0;
}

static const struct of_device_id lt7911d_match[] = {
	{ .compatible = "lt,lt7911" },
	{},
};
MODULE_DEVICE_TABLE(of, lt7911d_match);

static struct platform_driver lt7911d_driver = {
	.probe	= lt7911d_probe,
	.remove	= lt7911d_remove,
	.driver		= {
		.name		= "dumb-lt7911d",
		.of_match_table	= lt7911d_match,
	},
};

module_platform_driver(lt7911d_driver);

MODULE_AUTHOR("Dhims");
MODULE_DESCRIPTION("lt7911d bridge driver");
MODULE_LICENSE("GPL");
