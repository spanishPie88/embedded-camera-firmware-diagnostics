// SPDX-License-Identifier: GPL-2.0
/* Fictional RAW10 portfolio sensor: registers do not describe real hardware. */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define DEMO123_REG_CHIP_ID_H 0x0000
#define DEMO123_REG_CHIP_ID_L 0x0001
#define DEMO123_CHIP_ID 0x1234
#define DEMO123_REG_MODE_SELECT 0x0100
#define DEMO123_REG_EXPOSURE_H 0x0202
#define DEMO123_REG_ANALOG_GAIN 0x0204
#define DEMO123_REG_VTS_H 0x0340
#define DEMO123_XVCLK_FREQ 24000000
#define DEMO123_EXPOSURE_MIN 4
#define DEMO123_EXPOSURE_MARGIN 8

struct demo123_reg { u16 address; u8 value; };
struct demo123_mode {
	u32 width, height, hts, vts;
	u64 link_freq;
	const struct demo123_reg *registers;
	unsigned int num_registers;
};
struct demo123 {
	struct device *dev;
	struct regmap *regmap;
	struct clk *xvclk;
	struct regulator_bulk_data supplies[3];
	struct gpio_desc *reset_gpio;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_freq, *pixel_rate, *vblank, *exposure;
	struct mutex lock;
	const struct demo123_mode *mode;
	bool streaming;
};

static const struct regmap_config demo123_regmap_config = {
	.reg_bits = 16, .val_bits = 8, .cache_type = REGCACHE_RBTREE,
};
static const struct demo123_reg mode_1080p[] = {
	{0x0300, 0x04}, {0x034c, 0x07}, {0x034d, 0x80},
	{0x034e, 0x04}, {0x034f, 0x38},
};
static const struct demo123_reg mode_720p[] = {
	{0x0300, 0x03}, {0x034c, 0x05}, {0x034d, 0x00},
	{0x034e, 0x02}, {0x034f, 0xd0},
};
static const struct demo123_mode modes[] = {
	{1920, 1080, 2200, 1125, 400000000, mode_1080p, ARRAY_SIZE(mode_1080p)},
	{1280, 720, 1650, 750, 300000000, mode_720p, ARRAY_SIZE(mode_720p)},
};

static inline struct demo123 *to_demo123(struct v4l2_subdev *sd)
{
	return container_of(sd, struct demo123, sd);
}
static int write_u16(struct demo123 *s, u16 reg, u16 value)
{
	int ret = regmap_write(s->regmap, reg, value >> 8);
	return ret ? ret : regmap_write(s->regmap, reg + 1, value & 0xff);
}
static int write_table(struct demo123 *s, const struct demo123_reg *table,
		       unsigned int count)
{
	unsigned int i;
	int ret;
	for (i = 0; i < count; ++i) {
		ret = regmap_write(s->regmap, table[i].address, table[i].value);
		if (ret)
			return dev_err_probe(s->dev, ret, "write 0x%04x failed\n",
					     table[i].address);
	}
	return 0;
}
static int power_on(struct demo123 *s)
{
	int ret = regulator_bulk_enable(ARRAY_SIZE(s->supplies), s->supplies);
	if (ret) return ret;
	ret = clk_prepare_enable(s->xvclk);
	if (ret) goto disable_regulators;
	/* Descriptor logical 1 asserts reset even for GPIO_ACTIVE_LOW. */
	gpiod_set_value_cansleep(s->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(s->reset_gpio, 0);
	usleep_range(10000, 12000);
	regcache_cache_only(s->regmap, false);
	ret = regcache_sync(s->regmap);
	if (!ret) return 0;
	regcache_cache_only(s->regmap, true);
	gpiod_set_value_cansleep(s->reset_gpio, 1);
	clk_disable_unprepare(s->xvclk);
disable_regulators:
	regulator_bulk_disable(ARRAY_SIZE(s->supplies), s->supplies);
	return ret;
}
static void power_off(struct demo123 *s)
{
	regcache_cache_only(s->regmap, true);
	regcache_mark_dirty(s->regmap);
	gpiod_set_value_cansleep(s->reset_gpio, 1);
	clk_disable_unprepare(s->xvclk);
	regulator_bulk_disable(ARRAY_SIZE(s->supplies), s->supplies);
}
static int set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct demo123 *s = container_of(ctrl->handler, struct demo123, ctrls);
	int ret;
	if (!pm_runtime_get_if_in_use(s->dev)) return 0;
	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ret = write_u16(s, DEMO123_REG_EXPOSURE_H, ctrl->val); break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = regmap_write(s->regmap, DEMO123_REG_ANALOG_GAIN, ctrl->val); break;
	case V4L2_CID_VBLANK:
		ret = write_u16(s, DEMO123_REG_VTS_H, s->mode->height + ctrl->val); break;
	default: ret = -EINVAL;
	}
	pm_runtime_put(s->dev);
	return ret;
}
static const struct v4l2_ctrl_ops ctrl_ops = { .s_ctrl = set_ctrl };
static const struct demo123_mode *find_mode(u32 width, u32 height)
{
	const struct demo123_mode *best = &modes[0];
	unsigned int best_delta = UINT_MAX, i;
	for (i = 0; i < ARRAY_SIZE(modes); ++i) {
		unsigned int delta = abs((int)modes[i].width - (int)width) +
			abs((int)modes[i].height - (int)height);
		if (delta < best_delta) { best = &modes[i]; best_delta = delta; }
	}
	return best;
}
static void fill_format(const struct demo123_mode *mode,
			struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width; fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10; fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_XFER_FUNC_NONE;
}
static void update_mode_controls(struct demo123 *s,
				 const struct demo123_mode *mode)
{
	u32 vblank = mode->vts - mode->height;
	__v4l2_ctrl_s_ctrl(s->link_freq, mode->link_freq == 400000000 ? 1 : 0);
	__v4l2_ctrl_s_ctrl_int64(s->pixel_rate, mode->link_freq * 2 * 2 / 10);
	__v4l2_ctrl_modify_range(s->vblank, vblank, 0xffff, 1, vblank);
	__v4l2_ctrl_modify_range(s->exposure, DEMO123_EXPOSURE_MIN,
		mode->vts - DEMO123_EXPOSURE_MARGIN, 1,
		mode->vts - DEMO123_EXPOSURE_MARGIN);
}
static int enum_code(struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
		     struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index) return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	return 0;
}
static int enum_size(struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
		     struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->code != MEDIA_BUS_FMT_SRGGB10_1X10 || fse->index >= ARRAY_SIZE(modes))
		return -EINVAL;
	fse->min_width = fse->max_width = modes[fse->index].width;
	fse->min_height = fse->max_height = modes[fse->index].height;
	return 0;
}
static int get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
		   struct v4l2_subdev_format *fmt)
{
	struct demo123 *s = to_demo123(sd);
	mutex_lock(&s->lock);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt->format = *v4l2_subdev_state_get_format(state, fmt->pad);
	else fill_format(s->mode, &fmt->format);
	mutex_unlock(&s->lock);
	return 0;
}
static int set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
		   struct v4l2_subdev_format *fmt)
{
	struct demo123 *s = to_demo123(sd);
	const struct demo123_mode *mode = find_mode(fmt->format.width, fmt->format.height);
	fill_format(mode, &fmt->format);
	mutex_lock(&s->lock);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		*v4l2_subdev_state_get_format(state, fmt->pad) = fmt->format;
	else if (s->streaming) { mutex_unlock(&s->lock); return -EBUSY; }
	else { s->mode = mode; update_mode_controls(s, mode); }
	mutex_unlock(&s->lock);
	return 0;
}
static int set_stream(struct v4l2_subdev *sd, int enable)
{
	struct demo123 *s = to_demo123(sd);
	int ret = 0;
	mutex_lock(&s->lock);
	if (!!enable == s->streaming) goto out;
	if (enable) {
		ret = pm_runtime_resume_and_get(s->dev);
		if (ret < 0) goto out;
		ret = write_table(s, s->mode->registers, s->mode->num_registers);
		if (!ret) ret = __v4l2_ctrl_handler_setup(&s->ctrls);
		if (!ret) ret = regmap_write(s->regmap, DEMO123_REG_MODE_SELECT, 1);
		if (ret) pm_runtime_put(s->dev); else s->streaming = true;
	} else {
		ret = regmap_write(s->regmap, DEMO123_REG_MODE_SELECT, 0);
		pm_runtime_put(s->dev); s->streaming = false;
	}
out:
	mutex_unlock(&s->lock);
	return ret;
}
static const struct v4l2_subdev_video_ops video_ops = { .s_stream = set_stream };
static const struct v4l2_subdev_pad_ops pad_ops = {
	.enum_mbus_code = enum_code, .enum_frame_size = enum_size,
	.get_fmt = get_fmt, .set_fmt = set_fmt,
};
static const struct v4l2_subdev_ops subdev_ops = { .video = &video_ops, .pad = &pad_ops };

static int identify(struct demo123 *s)
{
	unsigned int high, low;
	int ret = regmap_read(s->regmap, DEMO123_REG_CHIP_ID_H, &high);
	if (ret) return ret;
	ret = regmap_read(s->regmap, DEMO123_REG_CHIP_ID_L, &low);
	if (ret) return ret;
	return (high << 8 | low) == DEMO123_CHIP_ID ? 0 : -ENODEV;
}
static int init_controls(struct demo123 *s)
{
	static const s64 freqs[] = {300000000, 400000000};
	u32 vblank = s->mode->vts - s->mode->height;
	v4l2_ctrl_handler_init(&s->ctrls, 6);
	s->link_freq = v4l2_ctrl_new_int_menu(&s->ctrls, NULL, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(freqs) - 1, 1, freqs);
	s->pixel_rate = v4l2_ctrl_new_std(&s->ctrls, NULL, V4L2_CID_PIXEL_RATE,
		1, INT_MAX, 1, s->mode->link_freq * 2 * 2 / 10);
	s->vblank = v4l2_ctrl_new_std(&s->ctrls, &ctrl_ops, V4L2_CID_VBLANK,
		vblank, 0xffff, 1, vblank);
	s->exposure = v4l2_ctrl_new_std(&s->ctrls, &ctrl_ops, V4L2_CID_EXPOSURE,
		DEMO123_EXPOSURE_MIN, s->mode->vts - DEMO123_EXPOSURE_MARGIN, 1,
		s->mode->vts - DEMO123_EXPOSURE_MARGIN);
	v4l2_ctrl_new_std(&s->ctrls, &ctrl_ops, V4L2_CID_ANALOGUE_GAIN, 0, 0xff, 1, 0x10);
	if (s->ctrls.error) return s->ctrls.error;
	s->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	s->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	s->sd.ctrl_handler = &s->ctrls;
	return 0;
}
static int probe(struct i2c_client *client)
{
	static const char * const names[] = {"avdd", "dvdd", "iovdd"};
	struct demo123 *s = devm_kzalloc(&client->dev, sizeof(*s), GFP_KERNEL);
	unsigned int i; int ret;
	if (!s) return -ENOMEM;
	s->dev = &client->dev; s->mode = &modes[0]; mutex_init(&s->lock);
	s->regmap = devm_regmap_init_i2c(client, &demo123_regmap_config);
	if (IS_ERR(s->regmap)) return PTR_ERR(s->regmap);
	s->xvclk = devm_clk_get(s->dev, "xvclk");
	if (IS_ERR(s->xvclk)) return PTR_ERR(s->xvclk);
	for (i = 0; i < ARRAY_SIZE(names); ++i) s->supplies[i].supply = names[i];
	ret = devm_regulator_bulk_get(s->dev, ARRAY_SIZE(s->supplies), s->supplies);
	if (ret) return ret;
	s->reset_gpio = devm_gpiod_get(s->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(s->reset_gpio)) return PTR_ERR(s->reset_gpio);
	v4l2_i2c_subdev_init(&s->sd, client, &subdev_ops);
	s->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	s->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&s->sd.entity, 1, &s->pad);
	if (ret) return ret;
	ret = init_controls(s); if (ret) goto clean_entity;
	ret = power_on(s); if (ret) goto free_ctrls;
	ret = identify(s); if (ret) goto power_off;
	ret = v4l2_async_register_subdev_sensor(&s->sd); if (ret) goto power_off;
	pm_runtime_set_active(s->dev); pm_runtime_enable(s->dev); pm_runtime_idle(s->dev);
	return 0;
power_off: power_off(s);
free_ctrls: v4l2_ctrl_handler_free(&s->ctrls);
clean_entity: media_entity_cleanup(&s->sd.entity); mutex_destroy(&s->lock);
	return ret;
}
static void remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct demo123 *s = to_demo123(sd);
	v4l2_async_unregister_subdev(sd); media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&s->ctrls); pm_runtime_disable(s->dev);
	if (!pm_runtime_status_suspended(s->dev)) power_off(s);
	mutex_destroy(&s->lock);
}
static int runtime_resume(struct device *dev) { return power_on(to_demo123(dev_get_drvdata(dev))); }
static int runtime_suspend(struct device *dev) { power_off(to_demo123(dev_get_drvdata(dev))); return 0; }
static DEFINE_RUNTIME_DEV_PM_OPS(pm_ops, runtime_suspend, runtime_resume, NULL);
static const struct of_device_id of_match[] = {{.compatible = "portfolio,demo123"}, {}};
MODULE_DEVICE_TABLE(of, of_match);
static struct i2c_driver driver = {
	.driver = {.name = "demo123", .pm = pm_ptr(&pm_ops), .of_match_table = of_match},
	.probe = probe, .remove = remove,
};
module_i2c_driver(driver);
MODULE_DESCRIPTION("Fictional V4L2 image sensor portfolio driver");
MODULE_AUTHOR("spanishPie88");
MODULE_LICENSE("GPL");
