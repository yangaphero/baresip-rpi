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
#include <sys/prctl.h>


/**
 * @defgroup avformat avformat
 *
 * Video source using rpi cam yuv420
 *
 *
 * Example config:
 \verbatim
  video_source            omxcam
 \endverbatim
 */




struct vidsrc_st {
	const struct vidsrc *vs;  /* inheritance */
	struct vidsrc_prm prm;
	pthread_t thread;
	pthread_t thread2;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
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
	void *arg;
};

static struct vidsrc *mod_avf;

omxcam_video_settings_t videoset;
struct vidsrc_st *st;
omxcam_yuv_planes_t planes;
omxcam_yuv_planes_t planes_slice;
uint8_t* file_buffer;
uint32_t frame_size;
uint32_t current = 0;
uint32_t offset_y;
uint32_t offset_u;
uint32_t offset_v;
int fd;

struct vidframe frame;
static void *read_thread(void *arg)
{	
	(void)arg;
	prctl(PR_SET_NAME,"omxcam_read");
	if (st->run) {
			info("omxcam: read_frame staring\n");
			omxcam_yuv_planes (videoset.camera.width, videoset.camera.height,  &planes);
			omxcam_yuv_planes_slice (videoset.camera.width, &planes_slice);
			frame_size = planes.offset_y + planes.offset_v + planes.length_v;
			offset_y = planes.offset_y;
			offset_u = planes.offset_u;
			offset_v = planes.offset_v;
			
			//info("offset_y=%d offset_u=%d offset_v=%d \n",offset_y,offset_u,offset_v);
			file_buffer = (uint8_t*)malloc (sizeof (uint8_t)*frame_size);
			
			info("frame_size=%d\n",frame_size);
			info("planes_slice.length_y=%d  planes_slice.offset_y=%d\n",planes_slice.length_y,planes_slice.offset_y);
			info("planes_slice.length_u=%d  planes_slice.offset_u=%d\n",planes_slice.length_u,planes_slice.offset_u);
			info("planes_slice.length_v=%d  planes_slice.offset_v=%d\n",planes_slice.length_v,planes_slice.offset_v);
			//fd = open ("1.yuv", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
			omxcam_video_start (&videoset, OMXCAM_CAPTURE_FOREVER);
			//if (omxcam_video_start (&videoset, OMXCAM_CAPTURE_FOREVER)) omxcam_perror ();
	}
	free (file_buffer);
	info("omxcam: read_frame stoped\n");
	return NULL;
}

/*
static void *write_frame(void *arg)
{
	(void)arg;
	while (st->run) {
		pthread_mutex_lock(&st->mutex);
		pthread_cond_wait(&st->cond, &st->mutex);
		if(st->run){ 
			st->frameh(&frame, st->arg);
		}
		pthread_mutex_unlock(&st->mutex);
	}
	return NULL;
}
*/

uint64_t ts = 0, ts_start;
static void on_data_time (omxcam_buffer_t buffer){
	//info("buffer.length:%d,w=%d,h=%d\n",buffer.length,st->sz.w,st->sz.h);
	
	current += buffer.length;

	memcpy (file_buffer + offset_y, buffer.data + planes_slice.offset_y,      planes_slice.length_y);
	offset_y += planes_slice.length_y;

	memcpy (file_buffer + offset_u, buffer.data + planes_slice.offset_u,      planes_slice.length_u);
	offset_u += planes_slice.length_u;

	memcpy (file_buffer + offset_v, buffer.data + planes_slice.offset_v,      planes_slice.length_v);
	offset_v += planes_slice.length_v;
	
	if (current == frame_size){//接收到一个完整的帧
		uint64_t now;
		uint64_t timestamp;
		now = tmr_jiffies();
		if (!ts) {
			ts = ts_start = now;
		}
		//if (ts > now) return; //有错误
		timestamp = (ts - ts_start) * VIDEO_TIMEBASE / 1000;

		offset_y = planes.offset_y;
		offset_u = planes.offset_u;
		offset_v = planes.offset_v;
		//info("1-omxcam:y=%d,u=%d,v=%d\n",offset_y,offset_u,offset_v);
		//pwrite (fd, file_buffer, frame_size, 0);//写入测试文件
		current = 0;
		//pthread_mutex_lock(&st->mutex);
		vidframe_init_buf(&frame, VID_FMT_YUV420P, &st->sz, file_buffer);
		//pthread_cond_broadcast(&st->cond);
		//pthread_mutex_unlock(&st->mutex);
		//printf("omxcam:timestamp=%llu\n",timestamp);
		st->frameh(&frame,timestamp, st->arg);//这里直接发送yuv帧，会导致帧速率下降，采用线程有一定的缓解，但是还是达不到满速率
		ts += 1000/st->prm.fps;
	}
}

static void destructor(void *arg)
{
	//struct vidsrc_st *st = arg;
	(void)arg;
	if (st->run) {
		omxcam_video_stop();
		st->run = false;
		current = 0;
		ts = 0;
		//pthread_mutex_lock(&st->mutex);
		//pthread_cond_broadcast(&st->cond);
		//pthread_mutex_unlock(&st->mutex);
		//pthread_join(st->thread, NULL);
		//pthread_join(st->thread2, NULL);

	}
	//uninit_device(st);
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

	debug("omxcam: alloc dev='%s'\n", dev);
	
	
	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;
	
	st->sz = *size;
	st->frameh = frameh;
	st->arg    = arg;
	st->prm    = *prm;
	
	//摄像头/编码器初始化
	omxcam_video_init(&videoset);
	//摄像头参数设置
	videoset.on_data = on_data_time;
	//videoset.camera.width = size->w;
	//videoset.camera.height = size->h;
	videoset.camera.width = 1280;
	videoset.camera.height = 720;//视频切换时候，引起分辨率不对
	videoset.camera.framerate = prm->fps;
	videoset.camera.rotation=180;//旋转
	videoset.format = OMXCAM_FORMAT_YUV420;

	st->sz.w = videoset.camera.width;
	st->sz.h = videoset.camera.height;
	st->pixfmt = VID_FMT_YUV420P;
	
	st->run = true;
	
	pthread_cond_init(&st->cond, NULL);
	pthread_mutex_init(&st->mutex, NULL);
	err = pthread_create(&st->thread, NULL, read_thread, st);
	if (err) {
		st->run = false;
		goto out;
	}
	//err = pthread_create(&st->thread2, NULL, write_frame, st);
	//	if (err) {
	//		st->run = false;
	//	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;

}

static int module_init(void)
{
	
	//info("omxcam_yuv inited\n");

	//vidcodec_register(baresip_vidcodecl(), &h264);

	return vidsrc_register(&mod_avf, baresip_vidsrcl(), "omxcam", alloc, NULL);
}


static int module_close(void)
{
	mod_avf = mem_deref(mod_avf);
	//vidcodec_unregister(&h264);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(omxcam) = {
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
