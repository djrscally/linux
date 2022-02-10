// SPDX-License-Identifier: GPL-2.0
/* Author: Dan Scally <djrscally@gmail.com> */

#include <linux/leds.h>
#include <linux/mfd/tps68470.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define lcdev_to_led(lcdev) \
	container_of(lcdev, struct tps68470_led, lcdev);

#define led_to_tps68470(led, index) \
	container_of(led, struct tps68470_device, leds[index])

enum tps68470_led_ids {
	TPS68470_ILED_A,
	TPS68470_ILED_B,
	TPS68470_WLED,
	TPS68470_NUM_LEDS
};

static const char *tps68470_led_names[] = {
	[TPS68470_ILED_A] = "tps68470-iled_a",
	[TPS68470_ILED_B] = "tps68470-iled_b",
	[TPS68470_WLED] = "tps68470-wled",
};

struct tps68470_led {
	unsigned int led_id;
	struct led_classdev lcdev;
};

struct tps68470_device {
	struct device *dev;
	struct regmap *regmap;
	struct tps68470_led leds[TPS68470_NUM_LEDS];
};

static int tps68470_led_brightness_set(struct led_classdev *lcdev, enum led_brightness state)
{
	struct tps68470_device *tps68470;
	struct tps68470_led *led;

	pr_info("%s(): %d\n for state %u", __func__, 0, state);

	led = lcdev_to_led(lcdev);
	tps68470 = led_to_tps68470(led, led->led_id);

	/* We only support binary on/off setting for now */
	if (state > LED_ON)
		return -EINVAL;

	switch (led->led_id) {
	case TPS68470_ILED_A:
		pr_info("%s(): %d\n", __func__, 1);
		return regmap_update_bits(tps68470->regmap, TPS68470_REG_ILEDCTL,
					  TPS68470_ILED_A_CTL_MASK,
					  state ? TPS68470_ILED_A_CTL_MASK : ~TPS68470_ILED_A_CTL_MASK);
	case TPS68470_ILED_B:
		pr_info("%s(): %d\n", __func__, 2);
		return regmap_update_bits(tps68470->regmap, TPS68470_REG_ILEDCTL,
					  TPS68470_ILED_B_CTL_MASK,
					  state ? TPS68470_ILED_B_CTL_MASK : ~TPS68470_ILED_B_CTL_MASK);
	case TPS68470_WLED:
		pr_info("%s(): %d\n", __func__, 3);
		return regmap_update_bits(tps68470->regmap, TPS68470_REG_WLEDCTL,
					  TPS68470_WLED_CTL_MASK,
					  state ? TPS68470_WLED_CTL_MASK : ~TPS68470_WLED_CTL_MASK);
	default:
		pr_info("%s(): %d\n", __func__, 4);
		return dev_err_probe(tps68470->dev, -EINVAL, "invalid led id\n");
	}

	return 0;
}

static int tps68470_leds_init(struct tps68470_device *tps68470)
{
	/* Init WLED based on the registers we read, plus
	 * I suppose anything that needs doing for ILED_A
	 * and ILED_B
	 */ 
	int ret;

	pr_info("%s(): %d\n", __func__, 0);

	ret = regmap_write(tps68470->regmap, 0x2f, 0x1f);
	if (ret)
		return dev_err_probe(tps68470->dev, ret, "error setting WLEDMAXF\n");

	ret = regmap_write(tps68470->regmap, 0x30, 0x7);
	if (ret)
		return dev_err_probe(tps68470->dev, ret, "error setting WLEDTO\n");

	ret = regmap_write(tps68470->regmap, 0x34, 0x1f);
	if (ret)
		return dev_err_probe(tps68470->dev, ret, "error setting WLEDC1\n");

	ret = regmap_write(tps68470->regmap, 0x35, 0x1f);
	if (ret)
		return dev_err_probe(tps68470->dev, ret, "error setting WLEDC2\n");

	ret = regmap_write(tps68470->regmap, 0x36, 0xc);
	if (ret)
		return dev_err_probe(tps68470->dev, ret, "error setting WLEDCTL\n");

	pr_info("%s(): %d\n", __func__, 1);

	return 0;
}

static int tps68470_leds_register(struct tps68470_device *tps68470, unsigned int index)
{
	struct tps68470_led *led = &tps68470->leds[index];
	struct led_classdev *lcdev = &led->lcdev;
	int ret;

	led->led_id = index;

	lcdev->name = devm_kasprintf(tps68470->dev, GFP_KERNEL, "%s::%s",
				     tps68470_led_names[index], LED_FUNCTION_INDICATOR);
	if (!lcdev->name)
		return -ENOMEM;

	lcdev->max_brightness = 1;
	lcdev->brightness_set_blocking = tps68470_led_brightness_set;

	ret = devm_led_classdev_register(tps68470->dev, lcdev);
	if (ret)
		return dev_err_probe(tps68470->dev, ret,
				     "error registering led\n");

	return ret;
}

static int tps68470_leds_probe(struct platform_device *pdev)
{
	struct tps68470_device *tps68470;
	unsigned int i;
	int ret;

	pr_info("%s(): begins\n", __func__);
	
	tps68470 = devm_kzalloc(&pdev->dev, sizeof(*tps68470), GFP_KERNEL);
	if (!tps68470)
		return -ENOMEM;

	tps68470->regmap = dev_get_drvdata(pdev->dev.parent);
	if (IS_ERR_OR_NULL(tps68470->regmap))
		return dev_err_probe(&pdev->dev, -EINVAL,
							 "no regmap found for parent\n");

	tps68470->dev = &pdev->dev;

	for (i = 0; i < TPS68470_NUM_LEDS; i++) {
		ret = tps68470_leds_register(tps68470, i);
		if (ret)
			return ret;
	}

	return tps68470_leds_init(tps68470);
}

static struct platform_driver tps68470_leds_driver = {
	.driver = {
		   .name = "tps68470-leds",
	},
	.probe = tps68470_leds_probe,
};

module_platform_driver(tps68470_leds_driver);

MODULE_AUTHOR("Daniel Scally <djrscally@gmail.com");
MODULE_DESCRIPTION("TPS68470 LEDs Driver");
MODULE_LICENSE("GPL v2");
