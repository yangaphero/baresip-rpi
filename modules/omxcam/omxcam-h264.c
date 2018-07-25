/**
 * @file avformat.c  libavformat video-source
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>


#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <omxcam.h>



/**
 * @defgroup avformat avformat
 *
 * Video source using FFmpeg/libav libavformat
 *
 *
 * Example config:
 \verbatim
  video_source            omxcam,/tmp/testfile.mp4
 \endverbatim
 */




struct vidsrc_st {
	const struct vidsrc *vs;  /* inheritance */
	pthread_t thread;
	bool run;
	struct vidsz sz;
	u_int32_t pixfmt;
	vidsrc_frame_h *frameh;
	void *arg;
};

struct videnc_state {
	struct le le;
	struct videnc_param encprm;
	videnc_packet_h *pkth;
	int64_t pts;
	void *arg;
};

static struct vidsrc *mod_avf;

	omxcam_video_settings_t videoset;
	struct vidsrc_st *st;
	struct videnc_state *st2;
	struct le *le;
	omxcam_yuv_planes_t planes;
	omxcam_yuv_planes_t planes_slice;
	uint8_t* file_buffer;
	uint32_t frame_size;
	uint32_t current = 0;
	uint32_t offset_y;
	uint32_t offset_u;
	uint32_t offset_v;

static void *read_thread(void *arg)
{
	struct vidsrc_st *st = arg;
	
	if (st->run) {
			info("omxcam: read_frame staring\n");
			omxcam_yuv_planes (videoset.camera.width, videoset.camera.height,  &planes);
			omxcam_yuv_planes_slice (videoset.camera.width, &planes_slice);
			frame_size = planes.offset_v + planes.length_v;
			
			file_buffer = (uint8_t*)malloc (sizeof (uint8_t)*frame_size);
			
			info("frame_size=%d\n",frame_size);
			info("planes_slice.length_y=%d  planes_slice.offset_y=%d\n",planes_slice.length_y,planes_slice.offset_y);
			info("planes_slice.length_u=%d  planes_slice.offset_u=%d\n",planes_slice.length_u,planes_slice.offset_u);
			info("planes_slice.length_v=%d  planes_slice.offset_v=%d\n",planes_slice.length_v,planes_slice.offset_v);
			omxcam_video_start (&videoset, OMXCAM_CAPTURE_FOREVER);
			info("omxcam: read_frame stoped\n");
	}

	return NULL;
}

uint32_t ts=0;
void on_data_time (omxcam_buffer_t buffer){
	int err;
	

	info("buffer.length:%d,w=%d,ts=%u\n",buffer.length,st->sz.w,ts);
	//err = h264_packetize(ts, buffer.data, buffer.length, st2->encprm.pktsize, st2->pkth, st2->arg);
	ts+=3000;

}

static void destructor(void *arg)
{
	struct vidsrc_st *st = arg;

	if (st->run) {
		omxcam_video_stop();
		st->run = false;
		pthread_join(st->thread, NULL);
	}
}



static int alloc(struct vidsrc_st **stp, const struct vidsrc *vs,
		 struct media_ctx **mctx, struct vidsrc_prm *prm,
		 const struct vidsz *size, const char *fmt,
		 const char *dev, vidsrc_frame_h *frameh,
		 vidsrc_error_h *errorh, void *arg)
{

	
	int err;

	(void)mctx;
	(void)errorh;
	(void)fmt;
	(void)arg;
	
	if (!stp || !vs || !prm || !size || !frameh)
		return EINVAL;

	debug("avformat: alloc dev='%s'\n", dev);
	
	
	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;
	
	st->sz = *size;
	st->frameh = frameh;
	st->arg    = arg;
	st->pixfmt = 0;
	
	
	//摄像头/编码器初始化
	omxcam_video_init(&videoset);
	//摄像头参数设置
	videoset.on_data = on_data_time;
	videoset.camera.width = 1280;
	videoset.camera.height = 720;
	videoset.camera.framerate = 30;
	videoset.camera.rotation=180;//旋转
	//videoset.format = OMXCAM_FORMAT_YUV420;
	
	videoset.h264.bitrate = 1000*1000; //12Mbps
	videoset.h264.idr_period = 10;	//30 IDR间隔
	videoset.h264.inline_headers = 1; // SPS/PPS头插入
	videoset.h264.profile=OMXCAM_H264_AVC_PROFILE_HIGH;//OMXCAM_H264_AVC_PROFILE_BASELINE;

	st->sz.w = videoset.camera.width;
	st->sz.h =  videoset.camera.height;
	st->pixfmt = VID_FMT_YUV420P;
	
	st->run = true;
	err = pthread_create(&st->thread, NULL, read_thread, st);
	if (err) {
		st->run = false;
		goto out;
	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;

}

static int module_init(void)
{
	
	info("omxcam_yuv inited\n");

	//vidcodec_register(baresip_vidcodecl(), &h264);

	return vidsrc_register(&mod_avf, baresip_vidsrcl(), "omxcam", alloc, NULL);
}


static int module_close(void)
{
	mod_avf = mem_deref(mod_avf);
	//vidcodec_unregister(&h264);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(avformat) = {
	"omxcam",
	"vidsrc",
	module_init,
	module_close
};



/*
static void enc_destructor(void *arg)
{
	struct videnc_state *st = arg;

	list_unlink(&st->le);
}

static uint32_t packetization_mode(const char *fmtp)
{
	struct pl pl, mode;

	if (!fmtp)
		return 0;

	pl_set_str(&pl, fmtp);

	if (fmt_param_get(&pl, "packetization-mode", &mode))
		return pl_u32(&mode);

	return 0;
}


static int encode_update(struct videnc_state **vesp, const struct vidcodec *vc,
			 struct videnc_param *prm, const char *fmtp,
			 videnc_packet_h *pkth, void *arg)
{
	struct videnc_state *st;
	int err = 0;


	return err;
}



static int encode_packet(struct videnc_state *st, bool update,
			 const struct vidframe *frame)
{
	(void)st;
	(void)update;
	(void)frame;
	return 0;
}



static int h264_fmtp_enc(struct mbuf *mb, const struct sdp_format *fmt,
			 bool offer, void *arg)
{
	struct vidcodec *vc = arg;
	const uint8_t profile_idc = 0x42; 
	const uint8_t profile_iop = 0x80;
	static const uint8_t h264_level_idc = 0x0c;
	(void)offer;

	if (!mb || !fmt || !vc)
		return 0;

	return mbuf_printf(mb, "a=fmtp:%s"
			   " packetization-mode=0"
			   ";profile-level-id=%02x%02x%02x"
			   "\r\n",
			   fmt->id, profile_idc, profile_iop, h264_level_idc);
}


static bool h264_fmtp_cmp(const char *fmtp1, const char *fmtp2, void *data)
{
	(void)data;

	return packetization_mode(fmtp1) == packetization_mode(fmtp2);
}



static struct vidcodec h264 = {
	LE_INIT,
	NULL,
	"H264",
	"packetization-mode=0",
	NULL,
	encode_update,
	encode_packet,
	NULL,
	NULL,
	h264_fmtp_enc,
	h264_fmtp_cmp,
};
*/
