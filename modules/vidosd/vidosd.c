/**
 * @file vidosd.c  Video-osd filter
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "vidosd.h"


/**
 * @defgroup vidosd vidosd
 *
 * Display video-osd overlay on the encode/decode streams
 *
 * Displays osd like framerate and packet timing, this is mainly
 * for development and debugging.
 */


struct vidosd_enc {
	struct vidfilt_enc_st vf;  /* base member (inheritance) */

	struct panel *panel;
};


struct vidosd_dec {
	struct vidfilt_dec_st vf;  /* base member (inheritance) */

	struct panel *panel;
};


static void encode_destructor(void *arg)
{
	struct vidosd_enc *st = arg;

	list_unlink(&st->vf.le);
	mem_deref(st->panel);
}


static void decode_destructor(void *arg)
{
	struct vidosd_dec *st = arg;

	list_unlink(&st->vf.le);
	mem_deref(st->panel);
}


static int encode_update(struct vidfilt_enc_st **stp, void **ctx,
			 const struct vidfilt *vf)
{
	struct vidosd_enc *st;
	int err = 0;

	if (!stp || !ctx || !vf)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), encode_destructor);
	if (!st)
		return ENOMEM;

	if (err)
		mem_deref(st);
	else
		*stp = (struct vidfilt_enc_st *)st;

	return err;
}


static int decode_update(struct vidfilt_dec_st **stp, void **ctx,
			 const struct vidfilt *vf)
{
	struct vidosd_dec *st;
	int err = 0;

	if (!stp || !ctx || !vf)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), decode_destructor);
	if (!st)
		return ENOMEM;

	if (err)
		mem_deref(st);
	else
		*stp = (struct vidfilt_dec_st *)st;

	return err;
}


static int encode(struct vidfilt_enc_st *_st, struct vidframe *frame)
{
	struct vidosd_enc *st = (struct vidosd_enc *)_st;
	int err = 0;

	if (!st->panel) {

		unsigned width = frame->size.w;
		unsigned height = MIN(PANEL_HEIGHT, frame->size.h);

		err = panel_alloc_osd(&st->panel, "武警青海总队", 0, width, height);
		if (err)
			return err;
	}

	//panel_add_frame_osd(st->panel, tmr_jiffies());

	panel_draw_osd(st->panel, frame);

	return 0;
}


static int decode(struct vidfilt_dec_st *_st, struct vidframe *frame)
{
	struct vidosd_dec *st = (struct vidosd_dec *)_st;
	int err = 0;

	if (!st->panel) {

		unsigned width = frame->size.w;
		unsigned height = MIN(PANEL_HEIGHT, frame->size.h);
		unsigned yoffs = frame->size.h - PANEL_HEIGHT;

		err = panel_alloc_osd(&st->panel, "decode_osd", yoffs, width, height);
		if (err)
			return err;
	}

	panel_add_frame_osd(st->panel, tmr_jiffies());

	panel_draw_osd(st->panel, frame);

	return 0;
}


static struct vidfilt vidosd = {
	//LE_INIT, "vidosd", encode_update, encode, decode_update, decode
	LE_INIT, "vidosd", encode_update, encode, NULL, NULL
};


static int module_init(void)
{
	vidfilt_register(baresip_vidfiltl(), &vidosd);

	return 0;
}


static int module_close(void)
{
	vidfilt_unregister(&vidosd);

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(vidosd) = {
	"vidosd",
	"vidfilt",
	module_init,
	module_close
};
