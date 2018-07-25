#include <re.h>
#include <baresip.h>
#include "webapp.h"

static struct ua *uag_cur(void)
{
	return uag_current();
}
#ifdef USE_VIDEO
static int switch_video_source(void *arg)
{
	const  char *carg = (char *)arg;
	struct pl pl_name, pl_dev;
	struct config_video *vidcfg;
	struct config *cfg;
	struct video *a;
	struct le *le;
	char name[16], dev[128] = "";
	int err = 0;

	if (carg) {
		if (re_regex(carg, str_len(carg), "[^,]+,[~]*", &pl_name, &pl_dev)) {
			warning("\rFormat should be:"  " name,dev\n");
			return 0;
		}

		pl_strcpy(&pl_name, name, sizeof(name));
		pl_strcpy(&pl_dev, dev, sizeof(dev));
	
		if (!vidsrc_find(baresip_vidsrcl(), name)) {
			warning("no such video-source: %s\n", name);
			return 0;
		}

		info( "switch video device: %s,%s\n", name, dev);

		cfg = conf_config();
		if (!cfg) {
			warning("no config object\n");
			return 0;
		}
		
		vidcfg = &cfg->video;
		
		//str_ncpy(vidcfg->src_mod, name, sizeof(vidcfg->src_mod));
		//str_ncpy(vidcfg->src_dev, dev, sizeof(vidcfg->src_dev));
		
		vidcfg->width=1280; 
		vidcfg->height=720;
		//vidcfg->bitrate=4000000;
		vidcfg->fps=25;
/*
		//size.w = v->cfg.width;
		//size.h = v->cfg.height;
		//size->w = 1280;
		//size->h = 720;
		//vtx->vsrc_size       = *size;
		//vtx->vsrc_prm.fps    = get_fps(vtx->video);
		//vtx->vsrc_prm.orient = VIDORIENT_PORTRAIT;
*/		
		for (le = list_tail(ua_calls(uag_cur())); le; le = le->prev) {

			struct call *call = le->data;

			a = call_video(call);
			//a->vtx->efps = 25;
			//a->vtx->vsrc_size.h = 720;
			err = video_set_source(a, name, dev);
			if (err) {
				warning("failed to set video-source (%m)\n", err);
				break;
			}
		}
		info("switch_video_source:%s,%s  \n", name, dev);
	}

	return 0;
}
#endif

#ifdef USE_VIDEO
void webapp_ws_videosrc(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;
	//(void *)arg;
	//(void *)hdr;
	
	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;
	info("webapp_ws_videosrc:%s  \n",e->u.str);
	if (!str_cmp(e->u.str, "addvideosrc")) {//添加视频源
		info("str_cmp:addvideosrc\n");
		webapp_videosrc_add(e);
		ws_send_json(WS_VIDEOSRC, webapp_videosrc_get());
		//ws_send_json(WS_CALLS, webapp_calls);
	}else if (!str_cmp(e->u.str, "deletevideosrc")) {//删除视频源
		char dev[100] = {0};

		e = odict_lookup(cmd, "dev");
		if (!e)
			goto out;
		info("deletevideosrc:%s\n",e->u.str);
		str_ncpy(dev, e->u.str, sizeof(dev));
		webapp_videosrc_delete(dev);
		ws_send_json(WS_VIDEOSRC, webapp_videosrc_get());
	}else if (!str_cmp(e->u.str, "switch")){//切换视频源
		
		char dev[100] = {0};
		e = odict_lookup(cmd, "dev");
		str_ncpy(dev, e->u.str, sizeof(dev));
		info("switch:%s\n",dev);
		switch_video_source(dev);
	}

out:

	mem_deref(cmd);
}
#endif
