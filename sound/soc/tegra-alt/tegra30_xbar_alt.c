/*
 * tegra30_xbar_alt.c - Tegra30 XBAR driver
 *
 * Copyright (c) 2011-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/soc.h>

#include "tegra30_xbar_alt.h"

#define DRV_NAME "tegra30-ahub-xbar"

static const struct regmap_config tegra30_xbar_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA_AHUB_AUDIO_RX_STRIDE *
			(TEGRA_AHUB_AUDIO_RX_COUNT - 1),
	.cache_type = REGCACHE_RBTREE,
};

static const struct regmap_config tegra124_xbar_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA_AHUB_AUDIO_RX1 + (TEGRA_AHUB_AUDIO_RX_STRIDE *
			(TEGRA_AHUB_AUDIO_RX_COUNT - 1)),
	.cache_type = REGCACHE_RBTREE,
};

static int tegra30_xbar_runtime_suspend(struct device *dev)
{
	struct tegra30_xbar *xbar = dev_get_drvdata(dev);

	regcache_cache_only(xbar->regmap, true);

	clk_disable(xbar->clk);

	return 0;
}

static int tegra30_xbar_runtime_resume(struct device *dev)
{
	struct tegra30_xbar *xbar = dev_get_drvdata(dev);
	int ret;

	ret = clk_enable(xbar->clk);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(xbar->regmap, false);

	return 0;
}

static int tegra30_xbar_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra30_xbar *xbar = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = xbar->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 32, 32, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	return 0;
}

#define DAI(sname)						\
	{							\
		.name = #sname,					\
		.playback = {					\
			.stream_name = #sname " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.capture = {					\
			.stream_name = #sname " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
	}

static struct snd_soc_dai_driver tegra30_xbar_dais[] = {
	DAI(APBIF0),
	DAI(APBIF1),
	DAI(APBIF2),
	DAI(APBIF3),
	DAI(I2S0),
	DAI(I2S1),
	DAI(I2S2),
	DAI(I2S3),
	DAI(I2S4),
	DAI(SPDIF),
	/* index 0..9 above are used on Tegra30 */
	DAI(APBIF4),
	DAI(APBIF5),
	DAI(APBIF6),
	DAI(APBIF7),
	DAI(APBIF8),
	DAI(APBIF9),
	DAI(AMX0),
	DAI(AMX0-0),
	DAI(AMX0-1),
	DAI(AMX0-2),
	DAI(AMX0-3),
	DAI(ADX0-0),
	DAI(ADX0-1),
	DAI(ADX0-2),
	DAI(ADX0-3),
	DAI(ADX0),
	/* index 0..25 above are used on Tegra114 */
	DAI(AMX1),
	DAI(AMX1-0),
	DAI(AMX1-1),
	DAI(AMX1-2),
	DAI(AMX1-3),
	DAI(ADX1-0),
	DAI(ADX1-1),
	DAI(ADX1-2),
	DAI(ADX1-3),
	DAI(ADX1),
	/* index 0..35 above are used on Tegra124 */
};

static const char * const tegra30_xbar_mux_texts[] = {
	"None",
	"APBIF0",
	"APBIF1",
	"APBIF2",
	"APBIF3",
	"I2S0",
	"I2S1",
	"I2S2",
	"I2S3",
	"I2S4",
	"SPDIF",
	/* index 0..10 above are used on Tegra30 */
	"APBIF4",
	"APBIF5",
	"APBIF6",
	"APBIF7",
	"APBIF8",
	"APBIF9",
	"AMX0",
	"ADX0-0",
	"ADX0-1",
	"ADX0-2",
	"ADX0-3",
	/* index 0..21 above are used on Tegra114 */
	"AMX1",
	"ADX1-0",
	"ADX1-1",
	"ADX1-2",
	"ADX1-3",
	/* index 0..26 above are used on Tegra124 */
};

#define MUX_VALUE(npart, nbit) (1 + nbit + npart * 32)
static const int tegra30_xbar_mux_values[] = {
	0,
	MUX_VALUE(0, 0),
	MUX_VALUE(0, 1),
	MUX_VALUE(0, 2),
	MUX_VALUE(0, 3),
	MUX_VALUE(0, 4),
	MUX_VALUE(0, 5),
	MUX_VALUE(0, 6),
	MUX_VALUE(0, 7),
	MUX_VALUE(0, 8),
	MUX_VALUE(0, 12),
	/* index 0..10 above are used on Tegra30 */
	MUX_VALUE(0, 14),
	MUX_VALUE(0, 15),
	MUX_VALUE(0, 16),
	MUX_VALUE(0, 17),
	MUX_VALUE(0, 18),
	MUX_VALUE(0, 19),
	MUX_VALUE(0, 20),
	MUX_VALUE(0, 21),
	MUX_VALUE(0, 22),
	MUX_VALUE(0, 23),
	MUX_VALUE(0, 24),
	/* index 0..21 above are used on Tegra114 */
	MUX_VALUE(1, 0),
	MUX_VALUE(1, 1),
	MUX_VALUE(1, 2),
	MUX_VALUE(1, 3),
	MUX_VALUE(1, 4),
	/* index 0..26 above are used on Tegra124 */
};

int tegra30_xbar_get_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct tegra30_xbar *xbar = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_count, reg_val, val, bit_pos = 0, reg_idx;

	reg_count = xbar->soc_data->num_mux1_input ? e->num_regs : 1;

	for (reg_idx = 0; reg_idx < reg_count; reg_idx++) {
		reg_val = snd_soc_read(codec, e->reg[reg_idx]);
		val = reg_val & xbar->soc_data->mask[reg_idx];
		if (val != 0) {
			bit_pos = ffs(val) + (8 * codec->val_bytes * reg_idx);
			break;
		}
	}

	ucontrol->value.enumerated.item[0] =
			snd_soc_enum_val_to_item(e, bit_pos);

	return 0;
}

int tegra30_xbar_put_value_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct tegra30_xbar *xbar = snd_soc_codec_get_drvdata(codec);
	unsigned int *item = ucontrol->value.enumerated.item;
	unsigned int change = 0, reg_idx = 0, value, *mask, bit_pos = 0;
	unsigned int i, wi, reg_count, reg_val = 0, update_idx = 0;
	struct snd_soc_dapm_update update;

	/* initialize the reg_count and mask from soc_data */
	reg_count = xbar->soc_data->num_mux1_input ? e->num_regs : 1;
	mask = (unsigned int *)xbar->soc_data->mask;

	if (item[0] >= e->max || reg_count > SND_SOC_DAPM_UPDATE_MAX_REG)
		return -EINVAL;

	value = snd_soc_enum_item_to_val(e, item[0]);

	if (value) {
		/* get the register index and value to set */
		reg_idx = (value - 1) / (8 * codec->val_bytes);
		bit_pos = (value - 1) % (8 * codec->val_bytes);
		reg_val = BIT(bit_pos);
	}

	for (i = 0; i < reg_count; i++) {
		if (i == reg_idx) {
			change |= snd_soc_test_bits(codec, e->reg[i],
							mask[i], reg_val);
			/* set the selected register */
			update.reg[reg_count - 1] = e->reg[reg_idx];
			update.mask[reg_count - 1] = mask[reg_idx];
			update.val[reg_count - 1] = reg_val;
		} else {
			/* accumulate the change to update the DAPM path
			    when none is selected */
			change |= snd_soc_test_bits(codec, e->reg[i],
							mask[i], 0);

			/* clear the register when not selected */
			update.reg[update_idx] = e->reg[i];
			update.mask[update_idx] = mask[i];
			update.val[update_idx++] = 0;
		}
	}

	/* power the widgets */
	if (change) {
		for (wi = 0; wi < wlist->num_widgets; wi++) {
			widget = wlist->widgets[wi];
			widget->value = reg_val;
			update.kcontrol = kcontrol;
			update.widget = widget;
			update.num_regs = reg_count;
			widget->dapm->update = &update;
			snd_soc_dapm_mux_update_power(widget, kcontrol, item[0], e);
			widget->dapm->update = NULL;
		}
	}

	return change;
}

#define MUX0_REG(id) (TEGRA_AHUB_AUDIO_RX + \
			(TEGRA_AHUB_AUDIO_RX_STRIDE * (id)))

#define MUX1_REG(id) (TEGRA_AHUB_AUDIO_RX1 + \
			(TEGRA_AHUB_AUDIO_RX_STRIDE * (id)))

#define SOC_VALUE_ENUM_WIDE(xregs, xnum_regs, shift, xmax, xtexts, xvalues) \
{	.reg = xregs, .shift_l = shift, .shift_r = shift, \
	.max = xmax, .texts = xtexts, .values = xvalues, \
	.mask = &(unsigned int) {(xmax ? roundup_pow_of_two(xmax) - 1 : 0)}, \
	.num_regs = xnum_regs }

#define SOC_VALUE_ENUM_WIDE_DECL(name, xregs, xnum_regs, shift, \
		xtexts, xvalues) \
	struct soc_enum name = SOC_VALUE_ENUM_WIDE(xregs, xnum_regs, shift, \
					ARRAY_SIZE(xtexts), xtexts, xvalues)

#define MUX_ENUM_CTRL_DECL(ename, id) \
	int ename##_regs[2] = { MUX0_REG(id), MUX1_REG(id) };	\
	SOC_VALUE_ENUM_WIDE_DECL(ename##_enum, ename##_regs, 2, 0,	\
			tegra30_xbar_mux_texts, tegra30_xbar_mux_values);	\
	static const struct snd_kcontrol_new ename##_control = \
		SOC_DAPM_ENUM_EXT("Route", ename##_enum,\
				tegra30_xbar_get_value_enum,\
				tegra30_xbar_put_value_enum)

MUX_ENUM_CTRL_DECL(apbif0_tx, 0x00);
MUX_ENUM_CTRL_DECL(apbif1_tx, 0x01);
MUX_ENUM_CTRL_DECL(apbif2_tx, 0x02);
MUX_ENUM_CTRL_DECL(apbif3_tx, 0x03);
MUX_ENUM_CTRL_DECL(i2s0_tx, 0x04);
MUX_ENUM_CTRL_DECL(i2s1_tx, 0x05);
MUX_ENUM_CTRL_DECL(i2s2_tx, 0x06);
MUX_ENUM_CTRL_DECL(i2s3_tx, 0x07);
MUX_ENUM_CTRL_DECL(i2s4_tx, 0x08);
MUX_ENUM_CTRL_DECL(spdif_tx, 0x0f);
/* above controls are used on Tegra30 */
MUX_ENUM_CTRL_DECL(apbif4_tx, 0x11);
MUX_ENUM_CTRL_DECL(apbif5_tx, 0x12);
MUX_ENUM_CTRL_DECL(apbif6_tx, 0x13);
MUX_ENUM_CTRL_DECL(apbif7_tx, 0x14);
MUX_ENUM_CTRL_DECL(apbif8_tx, 0x15);
MUX_ENUM_CTRL_DECL(apbif9_tx, 0x16);
MUX_ENUM_CTRL_DECL(amx00_tx, 0x17);
MUX_ENUM_CTRL_DECL(amx01_tx, 0x18);
MUX_ENUM_CTRL_DECL(amx02_tx, 0x19);
MUX_ENUM_CTRL_DECL(amx03_tx, 0x1a);
MUX_ENUM_CTRL_DECL(adx0_tx, 0x1b);
/* above controls are used on Tegra114 */
MUX_ENUM_CTRL_DECL(amx10_tx, 0x1e);
MUX_ENUM_CTRL_DECL(amx11_tx, 0x1f);
MUX_ENUM_CTRL_DECL(amx12_tx, 0x20);
MUX_ENUM_CTRL_DECL(amx13_tx, 0x21);
MUX_ENUM_CTRL_DECL(adx1_tx, 0x22);
/* above controls are used on Tegra124 */

#define WIDGETS(sname, ename) \
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_VALUE_MUX(sname " Mux", SND_SOC_NOPM, 0, 0, &ename##_control)

#define TX_WIDGETS(sname) \
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0)

/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra30_xbar_codec.num_dapm_widgets near the end of
 * tegra30_xbar_probe()
 */
static const struct snd_soc_dapm_widget tegra30_xbar_widgets[] = {
	WIDGETS("APBIF0", apbif0_tx),
	WIDGETS("APBIF1", apbif1_tx),
	WIDGETS("APBIF2", apbif2_tx),
	WIDGETS("APBIF3", apbif3_tx),
	WIDGETS("I2S0", i2s0_tx),
	WIDGETS("I2S1", i2s1_tx),
	WIDGETS("I2S2", i2s2_tx),
	WIDGETS("I2S3", i2s3_tx),
	WIDGETS("I2S4", i2s4_tx),
	WIDGETS("SPDIF", spdif_tx),
	/* index 0..9 above are used on Tegra30 */
	WIDGETS("APBIF4", apbif4_tx),
	WIDGETS("APBIF5", apbif5_tx),
	WIDGETS("APBIF6", apbif6_tx),
	WIDGETS("APBIF7", apbif7_tx),
	WIDGETS("APBIF8", apbif8_tx),
	WIDGETS("APBIF9", apbif9_tx),
	WIDGETS("AMX0-0", amx00_tx),
	WIDGETS("AMX0-1", amx01_tx),
	WIDGETS("AMX0-2", amx02_tx),
	WIDGETS("AMX0-3", amx03_tx),
	WIDGETS("ADX0", adx0_tx),
	TX_WIDGETS("AMX0"),
	TX_WIDGETS("ADX0-0"),
	TX_WIDGETS("ADX0-1"),
	TX_WIDGETS("ADX0-2"),
	TX_WIDGETS("ADX0-3"),
	/* index 0..25 above are used on Tegra114 */
	WIDGETS("AMX1-0", amx10_tx),
	WIDGETS("AMX1-1", amx11_tx),
	WIDGETS("AMX1-2", amx12_tx),
	WIDGETS("AMX1-3", amx13_tx),
	WIDGETS("ADX1", adx1_tx),
	TX_WIDGETS("AMX1"),
	TX_WIDGETS("ADX1-0"),
	TX_WIDGETS("ADX1-1"),
	TX_WIDGETS("ADX1-2"),
	TX_WIDGETS("ADX1-3"),
	/* index 0..35 above are used on Tegra124 */
};

/* These routes used on Tegra30, Tegra114, Tegra124 */
#define TEGRA30_ROUTES(name)					\
	{ name " RX",       NULL,	name " Receive"},	\
	{ name " Transmit", NULL,	name " TX"},		\
	{ name " TX",       NULL,	name " Mux" },		\
	{ name " Mux",      "APBIF0",	"APBIF0 RX" },		\
	{ name " Mux",      "APBIF1",	"APBIF1 RX" },		\
	{ name " Mux",      "APBIF2",	"APBIF2 RX" },		\
	{ name " Mux",      "APBIF3",	"APBIF3 RX" },		\
	{ name " Mux",      "I2S0",	"I2S0 RX" },		\
	{ name " Mux",      "I2S1",	"I2S1 RX" },		\
	{ name " Mux",      "I2S2",	"I2S2 RX" },		\
	{ name " Mux",      "I2S3",	"I2S3 RX" },		\
	{ name " Mux",      "I2S4",	"I2S4 RX" },		\
	{ name " Mux",      "SPDIF",	"SPDIF RX" },

/* These routes used on Tegra114 and Tegra124 */
#define TEGRA114_ROUTES(name)					\
	{ name " Mux",      "APBIF4",	"APBIF4 RX" },		\
	{ name " Mux",      "APBIF5",	"APBIF5 RX" },		\
	{ name " Mux",      "APBIF6",	"APBIF6 RX" },		\
	{ name " Mux",      "APBIF7",	"APBIF7 RX" },		\
	{ name " Mux",      "APBIF8",	"APBIF8 RX" },		\
	{ name " Mux",      "APBIF9",	"APBIF9 RX" },		\
	{ name " Mux",      "AMX0",	"AMX0 RX" },		\
	{ name " Mux",      "ADX0-0",	"ADX0-0 RX" },		\
	{ name " Mux",      "ADX0-1",	"ADX0-1 RX" },		\
	{ name " Mux",      "ADX0-2",	"ADX0-2 RX" },		\
	{ name " Mux",      "ADX0-3",	"ADX0-3 RX" },

#define AMX_OUT_ADX_IN_ROUTES(name)				\
	{ name " RX",       NULL,	name " Receive"},	\
	{ name " Transmit", NULL,	name " TX"},

/* These routes used on Tegra124 only */
#define TEGRA124_ROUTES(name)					\
	{ name " Mux",      "AMX1",	"AMX1 RX" },		\
	{ name " Mux",      "ADX1-0",	"ADX1-0 RX" },	\
	{ name " Mux",      "ADX1-1",	"ADX1-1 RX" },	\
	{ name " Mux",      "ADX1-2",	"ADX1-2 RX" },	\
	{ name " Mux",      "ADX1-3",	"ADX1-3 RX" },

/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra30_xbar_codec.num_dapm_routes near the end of
 * tegra30_xbar_probe()
 */
static const struct snd_soc_dapm_route tegra30_xbar_routes[] = {
	TEGRA30_ROUTES("APBIF0")
	TEGRA30_ROUTES("APBIF1")
	TEGRA30_ROUTES("APBIF2")
	TEGRA30_ROUTES("APBIF3")
	TEGRA30_ROUTES("I2S0")
	TEGRA30_ROUTES("I2S1")
	TEGRA30_ROUTES("I2S2")
	TEGRA30_ROUTES("I2S3")
	TEGRA30_ROUTES("I2S4")
	TEGRA30_ROUTES("SPDIF")
	/* above routes are used on Tegra30 */
	TEGRA30_ROUTES("APBIF4")
	TEGRA30_ROUTES("APBIF5")
	TEGRA30_ROUTES("APBIF6")
	TEGRA30_ROUTES("APBIF7")
	TEGRA30_ROUTES("APBIF8")
	TEGRA30_ROUTES("APBIF9")
	TEGRA30_ROUTES("AMX0-0")
	TEGRA30_ROUTES("AMX0-1")
	TEGRA30_ROUTES("AMX0-2")
	TEGRA30_ROUTES("AMX0-3")
	TEGRA30_ROUTES("ADX0")
	TEGRA114_ROUTES("APBIF0")
	TEGRA114_ROUTES("APBIF1")
	TEGRA114_ROUTES("APBIF2")
	TEGRA114_ROUTES("APBIF3")
	TEGRA114_ROUTES("I2S0")
	TEGRA114_ROUTES("I2S1")
	TEGRA114_ROUTES("I2S2")
	TEGRA114_ROUTES("I2S3")
	TEGRA114_ROUTES("I2S4")
	TEGRA114_ROUTES("SPDIF")
	TEGRA114_ROUTES("APBIF4")
	TEGRA114_ROUTES("APBIF5")
	TEGRA114_ROUTES("APBIF6")
	TEGRA114_ROUTES("APBIF7")
	TEGRA114_ROUTES("APBIF8")
	TEGRA114_ROUTES("APBIF9")
	TEGRA114_ROUTES("AMX0-0")
	TEGRA114_ROUTES("AMX0-1")
	TEGRA114_ROUTES("AMX0-2")
	TEGRA114_ROUTES("AMX0-3")
	TEGRA114_ROUTES("ADX0")
	AMX_OUT_ADX_IN_ROUTES("AMX0")
	AMX_OUT_ADX_IN_ROUTES("ADX0-0")
	AMX_OUT_ADX_IN_ROUTES("ADX0-1")
	AMX_OUT_ADX_IN_ROUTES("ADX0-2")
	AMX_OUT_ADX_IN_ROUTES("ADX0-3")
	/* above routes are used on Tegra114 */
	TEGRA30_ROUTES("AMX1-0")
	TEGRA30_ROUTES("AMX1-1")
	TEGRA30_ROUTES("AMX1-2")
	TEGRA30_ROUTES("AMX1-3")
	TEGRA30_ROUTES("ADX1")
	TEGRA114_ROUTES("AMX1-0")
	TEGRA114_ROUTES("AMX1-1")
	TEGRA114_ROUTES("AMX1-2")
	TEGRA114_ROUTES("AMX1-3")
	TEGRA114_ROUTES("ADX1")
	TEGRA124_ROUTES("APBIF0")
	TEGRA124_ROUTES("APBIF1")
	TEGRA124_ROUTES("APBIF2")
	TEGRA124_ROUTES("APBIF3")
	TEGRA124_ROUTES("I2S0")
	TEGRA124_ROUTES("I2S1")
	TEGRA124_ROUTES("I2S2")
	TEGRA124_ROUTES("I2S3")
	TEGRA124_ROUTES("I2S4")
	TEGRA124_ROUTES("SPDIF")
	TEGRA124_ROUTES("APBIF4")
	TEGRA124_ROUTES("APBIF5")
	TEGRA124_ROUTES("APBIF6")
	TEGRA124_ROUTES("APBIF7")
	TEGRA124_ROUTES("APBIF8")
	TEGRA124_ROUTES("APBIF9")
	TEGRA124_ROUTES("AMX0-0")
	TEGRA124_ROUTES("AMX0-1")
	TEGRA124_ROUTES("AMX0-2")
	TEGRA124_ROUTES("AMX0-3")
	TEGRA124_ROUTES("ADX0")
	TEGRA124_ROUTES("AMX1-0")
	TEGRA124_ROUTES("AMX1-1")
	TEGRA124_ROUTES("AMX1-2")
	TEGRA124_ROUTES("AMX1-3")
	TEGRA124_ROUTES("ADX1")
	AMX_OUT_ADX_IN_ROUTES("AMX1")
	AMX_OUT_ADX_IN_ROUTES("ADX1-0")
	AMX_OUT_ADX_IN_ROUTES("ADX1-1")
	AMX_OUT_ADX_IN_ROUTES("ADX1-2")
	AMX_OUT_ADX_IN_ROUTES("ADX1-3")
	/* above routes are used on Tegra124 */
};

static struct snd_soc_codec_driver tegra30_xbar_codec = {
	.probe = tegra30_xbar_codec_probe,
	.dapm_widgets = tegra30_xbar_widgets,
	.dapm_routes = tegra30_xbar_routes,
};

static const struct tegra30_xbar_soc_data soc_data_tegra30 = {
	.regmap_config = &tegra30_xbar_regmap_config,
	.num_dais = 10,
	.num_mux_widgets = 10,
	.num_mux0_input = 10,
	.num_mux1_input = 0,
	.mask[0] = 0x11ff,
	.mask[1] = 0,
};

static const struct tegra30_xbar_soc_data soc_data_tegra114 = {
	.regmap_config = &tegra30_xbar_regmap_config,
	.num_dais = 26,
	.num_mux_widgets = 21,
	.num_mux0_input = 21,
	.num_mux1_input = 0,
	.mask[0] = 0x1ffd1ff,
	.mask[1] = 0,
};

static const struct tegra30_xbar_soc_data soc_data_tegra124 = {
	.regmap_config = &tegra124_xbar_regmap_config,
	.num_dais = 36,
	.num_mux_widgets = 26,
	.num_mux0_input = 21,
	.num_mux1_input = 5,
	.mask[0] = 0x1ffd1ff,
	.mask[1] = 0x1f,
};

static const struct of_device_id tegra30_xbar_of_match[] = {
	{ .compatible = "nvidia,tegra30-ahub", .data = &soc_data_tegra30 },
	{ .compatible = "nvidia,tegra114-ahub", .data = &soc_data_tegra114 },
	{ .compatible = "nvidia,tegra124-ahub", .data = &soc_data_tegra124 },
	{},
};

static int tegra30_xbar_probe(struct platform_device *pdev)
{
	struct tegra30_xbar *xbar;
	void __iomem *regs;
	int ret;
	const struct of_device_id *match;
	struct tegra30_xbar_soc_data *soc_data;
	struct clk *parent_clk;

	match = of_match_device(tegra30_xbar_of_match, pdev->dev.parent);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra30_xbar_soc_data *)match->data;

	xbar = devm_kzalloc(&pdev->dev, sizeof(*xbar), GFP_KERNEL);
	if (!xbar) {
		dev_err(&pdev->dev, "Can't allocate xbar\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, xbar);

	xbar->soc_data = soc_data;

	xbar->clk = devm_clk_get(&pdev->dev, "d_audio");
	if (IS_ERR(xbar->clk)) {
		dev_err(&pdev->dev, "Can't retrieve clock\n");
		ret = PTR_ERR(xbar->clk);
		goto err;
	}

	xbar->clk_parent = clk_get_sys(NULL, "pll_a_out0");
	if (IS_ERR(xbar->clk)) {
		dev_err(&pdev->dev, "Can't retrieve pll_a_out0 clock\n");
		ret = PTR_ERR(xbar->clk_parent);
		goto err_clk_put;
	}

	parent_clk = clk_get_parent(xbar->clk);
	if (IS_ERR(parent_clk)) {
		dev_err(&pdev->dev, "Can't get parent clock fo xbar\n");
		ret = PTR_ERR(parent_clk);
		goto err_clk_put;
	}

	ret = clk_set_parent(xbar->clk, xbar->clk_parent);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set parent clock with pll_a_out0\n");
		goto err_clk_put;
	}

	regs = devm_request_and_ioremap(&pdev->dev, pdev->resource);
	if (!regs) {
		dev_err(&pdev->dev, "request/iomap region failed\n");
		ret = -ENODEV;
		goto err_clk_set_parent;
	}

	xbar->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					     soc_data->regmap_config);
	if (IS_ERR(xbar->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(xbar->regmap);
		goto err_clk_put_parent;
	}
	regcache_cache_only(xbar->regmap, true);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra30_xbar_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	tegra30_xbar_codec.num_dapm_widgets = (soc_data->num_dais * 2) +
					soc_data->num_mux_widgets;

	tegra30_xbar_codec.num_dapm_routes = (soc_data->num_dais * 2) +
					(soc_data->num_mux_widgets *
					(soc_data->num_mux0_input +
					soc_data->num_mux1_input + 1));

	ret = snd_soc_register_codec(&pdev->dev, &tegra30_xbar_codec,
				tegra30_xbar_dais, soc_data->num_dais);
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_xbar_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put_parent:
	clk_put(xbar->clk_parent);
err_clk_set_parent:
	clk_set_parent(xbar->clk, parent_clk);
err_clk_put:
	devm_clk_put(&pdev->dev, xbar->clk);
err:
	return ret;
}

static int tegra30_xbar_remove(struct platform_device *pdev)
{
	struct tegra30_xbar *xbar = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_xbar_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, xbar->clk);
	clk_put(xbar->clk_parent);

	return 0;
}

static const struct dev_pm_ops tegra30_xbar_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra30_xbar_runtime_suspend,
			   tegra30_xbar_runtime_resume, NULL)
};

static struct platform_driver tegra30_xbar_driver = {
	.probe = tegra30_xbar_probe,
	.remove = tegra30_xbar_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra30_xbar_of_match,
		.pm = &tegra30_xbar_pm_ops,
	},
};
module_platform_driver(tegra30_xbar_driver);

void tegra30_xbar_set_cif(struct regmap *regmap, unsigned int reg,
			  struct tegra30_xbar_cif_conf *conf)
{
	unsigned int value;

	value = (conf->threshold <<
			TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
		((conf->audio_channels - 1) <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
		((conf->client_channels - 1) <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
		(conf->audio_bits <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
		(conf->client_bits <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT) |
		(conf->expand <<
			TEGRA30_AUDIOCIF_CTRL_EXPAND_SHIFT) |
		(conf->stereo_conv <<
			TEGRA30_AUDIOCIF_CTRL_STEREO_CONV_SHIFT) |
		(conf->replicate <<
			TEGRA30_AUDIOCIF_CTRL_REPLICATE_SHIFT) |
		(conf->direction <<
			TEGRA30_AUDIOCIF_CTRL_DIRECTION_SHIFT) |
		(conf->truncate <<
			TEGRA30_AUDIOCIF_CTRL_TRUNCATE_SHIFT) |
		(conf->mono_conv <<
			TEGRA30_AUDIOCIF_CTRL_MONO_CONV_SHIFT);

	regmap_write(regmap, reg, value);
}
EXPORT_SYMBOL_GPL(tegra30_xbar_set_cif);

void tegra124_xbar_set_cif(struct regmap *regmap, unsigned int reg,
			   struct tegra30_xbar_cif_conf *conf)
{
	unsigned int value;

	value = (conf->threshold <<
			TEGRA124_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
		((conf->audio_channels - 1) <<
			TEGRA124_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
		((conf->client_channels - 1) <<
			TEGRA124_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
		(conf->audio_bits <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
		(conf->client_bits <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT) |
		(conf->expand <<
			TEGRA30_AUDIOCIF_CTRL_EXPAND_SHIFT) |
		(conf->stereo_conv <<
			TEGRA30_AUDIOCIF_CTRL_STEREO_CONV_SHIFT) |
		(conf->replicate <<
			TEGRA30_AUDIOCIF_CTRL_REPLICATE_SHIFT) |
		(conf->direction <<
			TEGRA30_AUDIOCIF_CTRL_DIRECTION_SHIFT) |
		(conf->truncate <<
			TEGRA30_AUDIOCIF_CTRL_TRUNCATE_SHIFT) |
		(conf->mono_conv <<
			TEGRA30_AUDIOCIF_CTRL_MONO_CONV_SHIFT);

	regmap_write(regmap, reg, value);
}
EXPORT_SYMBOL_GPL(tegra124_xbar_set_cif);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra30 XBAR driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
