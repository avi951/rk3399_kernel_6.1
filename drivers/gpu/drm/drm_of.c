#include <linux/component.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/of_graph.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>
#include <drm/drm_of.h>

/**
 * drm_crtc_port_mask - find the mask of a registered CRTC by port OF node
 * @dev: DRM device
 * @port: port OF node
 *
 * Given a port OF node, return the possible mask of the corresponding
 * CRTC within a device's list of CRTCs.  Returns zero if not found.
 */
static uint32_t drm_crtc_port_mask(struct drm_device *dev,
				   struct device_node *port)
{
	unsigned int index = 0;
	struct drm_crtc *tmp;

	drm_for_each_crtc(tmp, dev) {
		dev_err(dev->dev, "value of index is: %d\n", index);
		if (tmp->port == port)
			return 1 << index;

		index++;
	}

	dev_err(dev->dev, "returning 0\n");

	return 0;
}

/**
 * drm_of_find_possible_crtcs - find the possible CRTCs for an encoder port
 * @dev: DRM device
 * @port: encoder port to scan for endpoints
 *
 * Scan all endpoints attached to a port, locate their attached CRTCs,
 * and generate the DRM mask of CRTCs which may be attached to this
 * encoder.
 *
 * See Documentation/devicetree/bindings/graph.txt for the bindings.
 */
uint32_t drm_of_find_possible_crtcs(struct drm_device *dev,
				    struct device_node *port)
{
	struct device_node *remote_port, *ep;
	uint32_t possible_crtcs = 0;

	for_each_endpoint_of_node(port, ep) {
		if (!of_device_is_available(ep)) {
			of_node_put(ep);
			continue;
		}

		dev_err(dev->dev, "ep is: %s\n", ep->full_name);

		remote_port = of_graph_get_remote_port(ep);
		if (!remote_port) {
			of_node_put(ep);
			return 0;
		}

		dev_err(dev->dev, "remote port is: %s\n", remote_port->full_name);

		dev_err(dev->dev, "possible crtcs value is: %d\n", possible_crtcs);

		possible_crtcs |= drm_crtc_port_mask(dev, remote_port);

		dev_err(dev->dev, "possible crtcs value aftre checking is: %d\n", possible_crtcs);

		of_node_put(remote_port);
	}

	return possible_crtcs;
}
EXPORT_SYMBOL(drm_of_find_possible_crtcs);

/**
 * drm_of_component_probe - Generic probe function for a component based master
 * @dev: master device containing the OF node
 * @compare_of: compare function used for matching components
 * @master_ops: component master ops to be used
 *
 * Parse the platform device OF node and bind all the components associated
 * with the master. Interface ports are added before the encoders in order to
 * satisfy their .bind requirements
 * See Documentation/devicetree/bindings/graph.txt for the bindings.
 *
 * Returns zero if successful, or one of the standard error codes if it fails.
 */
int drm_of_component_probe(struct device *dev,
			   int (*compare_of)(struct device *, void *),
			   const struct component_master_ops *m_ops)
{
	struct device_node *ep, *port, *remote;
	struct component_match *match = NULL;
	int i;

	if (!dev->of_node)
		return -EINVAL;

	/*
	 * Bind the crtc's ports first, so that drm_of_find_possible_crtcs()
	 * called from encoder's .bind callbacks works as expected
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "no available port\n");
		return -ENODEV;
	}

	/*
	 * For bound crtcs, bind the encoders attached to their remote endpoint
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		for_each_child_of_node(port, ep) {
			remote = of_graph_get_remote_port_parent(ep);
			if (!remote || !of_device_is_available(remote)) {
				of_node_put(remote);
				continue;
			} else if (!of_device_is_available(remote->parent)) {
				dev_warn(dev, "parent device of %s is not available\n",
					 remote->full_name);
				of_node_put(remote);
				continue;
			}

			component_match_add(dev, &match, compare_of, remote);
			of_node_put(remote);
		}
		of_node_put(port);
	}

	return component_master_add_with_match(dev, m_ops, match);
}
EXPORT_SYMBOL(drm_of_component_probe);

/*
 * drm_of_encoder_active_endpoint - return the active encoder endpoint
 * @node: device tree node containing encoder input ports
 * @encoder: drm_encoder
 *
 * Given an encoder device node and a drm_encoder with a connected crtc,
 * parse the encoder endpoint connecting to the crtc port.
 */
int drm_of_encoder_active_endpoint(struct device_node *node,
				   struct drm_encoder *encoder,
				   struct of_endpoint *endpoint)
{
	struct device_node *ep;
	struct drm_crtc *crtc = encoder->crtc;
	struct device_node *port;
	int ret;

	if (!node || !crtc)
		return -EINVAL;

	for_each_endpoint_of_node(node, ep) {
		port = of_graph_get_remote_port(ep);
		of_node_put(port);
		if (port == crtc->port) {
			ret = of_graph_parse_endpoint(ep, endpoint);
			of_node_put(ep);
			return ret;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(drm_of_encoder_active_endpoint);

/*
 * drm_of_find_panel_or_bridge - return connected panel or bridge device
 * @np: device tree node containing encoder output ports
 * @panel: pointer to hold returned drm_panel
 * @bridge: pointer to hold returned drm_bridge
 *
 * Given a DT node's port and endpoint number, find the connected node and
 * return either the associated struct drm_panel or drm_bridge device. Either
 * @panel or @bridge must not be NULL.
 *
 * Returns zero if successful, or one of the standard error codes if it fails.
 */
int drm_of_find_panel_or_bridge(const struct device_node *np,
				int port, int endpoint,
				struct drm_panel **panel,
				struct drm_bridge **bridge)
{
	int ret = -EPROBE_DEFER;
	struct device_node *remote;

	printk(KERN_INFO "Finding drm for panel or bridge\n");

	if (!panel && !bridge) {
		printk(KERN_INFO "Invalid panel or bridge\n");
		return -EINVAL;
	}

	remote = of_graph_get_remote_node(np, port, endpoint);
	if (!remote) {
		 printk(KERN_INFO "Getting remote endpoint and ports\n");
		return -ENODEV;
	}

	if (panel) {
		printk(KERN_INFO "Checking for panel: %s\n", remote->name);
		*panel = of_drm_find_panel(remote);
		if (*panel) {
			printk(KERN_INFO "%s: Panel Found\n", remote->name);
			printk(KERN_INFO "Panel name is: %s\n", (*panel)->dev->of_node->name);
			ret = 0;
		}
	}

	/* No panel found yet, check for a bridge next. */
	if (bridge) {
		printk(KERN_INFO "Checking for bridge: %s\n", remote->name);
		if (ret) {
			printk(KERN_INFO "Eprobe defer checking\n");
			*bridge = of_drm_find_bridge(remote);
			if (*bridge) {
				printk(KERN_INFO "Bridge found\n");
				ret = 0;
			} else {
				printk(KERN_INFO "Bridge not found\n");
			}
		} else {
			printk(KERN_INFO "Not checked for bridge as panel is already founded\n");
			*bridge = NULL;
		}
	} else {
		printk(KERN_INFO "No bridge found\n");
	}

	of_node_put(remote);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_of_find_panel_or_bridge);
