# baresip-rpi
1.安装re
  (1)make
  (2)sudo make install

2.安装rem
  (1)make
  (2)sudo make install

3.安装baresip
  (1)make
  (2)sudo make install  可省
注意配置文件在～/.baresip/config中修改
  (3)debug and release
    Build with debug enabled
    $ make
    $ sudo make install

    Build with release
    $ make RELEASE=1
    $ sudo make RELEASE=1 install

4.错误修改
（1）alsa播放程序时候出现alsa: write: wrote 80 of 160 samples
在err = alsa_reset()函数下面添加一下代码
在alsa模块中alsa_play.c中初始化部分，添加以下代码：解决
snd_pcm_set_params(st->write,
                    SND_PCM_FORMAT_S16_LE,//SND_PCM_FORMAT_U8,
                    SND_PCM_ACCESS_RW_INTERLEAVED,/* snd_pcm_readi/snd_pcm_writei access */
                    1, //Channels
                    8000, //sample rate in Hz
                    1, //soft_resample
                    500000);
(2)在vidloop停止时候
    1.出现~~~~ Videoloop summary: ~~~~~段错误 (核心已转储)
    是因为re_hprintf时候无法解析 vs->name  也就是module模块名称
    同时在vidloop时候，也出现Enable video-loop on ,: 1280 x 720   模块名称为空
    解决办法：在vidloop模块中暂时屏蔽掉vs->name，后面再处理
    2.在video_debug时候出现：
            --- Video stream ---
             started: yes
             tx: encode: H264 ?
            段错误 (核心已转储)
    解决办法：在src/video.c中的vtx_debug函数中修改：注释掉名称，同时增加了send-frames=%d=vtx->frames这个变量值显示，在debug时候查看是否25帧时候5秒钟是否可以达到125个，即为25帧速率
	//err |= re_hprintf(pf, "     source: %s %u x %u, fps=%.2f"
	//		  " frames=%llu\n",
	//		  vtx->vsrc ? vidsrc_get(vtx->vsrc)->name : "none",
	err |= re_hprintf(pf, "     source: %u x %u, fps=%.2f"
			  " frames=%llu send-frames=%d\n",
			  //vtx->vsrc ? vidsrc_get(vtx->vsrc)->name : "none",
			  vtx->vsrc_size.w,
			  vtx->vsrc_size.h, vtx->vsrc_prm.fps,
			  vtx->stats.src_frames,vtx->frames);
(3)使用omx硬件编码器的问题
修改avcodec模块中的encode.c，添加sps pps发送
	(1)修改avcodec模块module.mk，注释掉#USE_X264，这样才能使用omx硬件编码，
    重要：omx编码器阻塞的问题，即avcodec_send_frame()后阻塞进程，不往下走
    根据2018-3-9:记录：关于编码器阻塞的问题：　基本解决,分2部分：（搞死我啦！！！！！！！！！！！！）
    a.在打开解码器中添加：open_encoder函数中
    在#if LIBAVCODEC_VERSION_INT >= ((53<<16)+(5<<8)+0)
	st->pict->format = pix_fmt;
	st->pict->width = size->w;
	st->pict->height = size->h; 下面添加：
        ret = av_frame_get_buffer(st->pict, 32);
        if (ret < 0)    debug("avcodec: Could not allocate the video frame data\n");
    b.在循环编码中添加：在encode函数中，在	for (i=0; i<4; i++) {之前添加：
        ret = av_frame_make_writable(st->pict);
        if (ret < 0)  debug("avcodec: Could not av_frame_make_writable\n");

	(2)添加cbr固定速率控制  在open_encoder函数中
		av_opt_set_defaults(st->ctx);
		//cbr固定速率
		st->ctx->bit_rate  = prm->bitrate;
		st->ctx->rc_min_rate =prm->bitrate;
		st->ctx->rc_max_rate = prm->bitrate; 
		st->ctx->bit_rate_tolerance = prm->bitrate;
		st->ctx->rc_buffer_size=prm->bitrate;
		st->ctx->rc_initial_buffer_occupancy = st->ctx->rc_buffer_size*3/4;
		//st->ctx->rc_buffer_aggressivity= (float)1.0;
		//st->ctx->rc_initial_cplx= 0.5; 
	(3)添加编码质量控制：不知道起不起作用 在open_encoder函数中
		st->ctx->max_qdiff = 4;
		//by aphero
		//av_opt_set(st->ctx, "preset", "veryslow", 0); 
		av_opt_set(st->ctx, "crf", "18.000", 0); 
		//av_opt_set(st->ctx, "preset", "slow", 0); 
		av_opt_set(st->ctx, "preset", "superfast", 0); 
		av_opt_set(st->ctx, "tune", "zerolatency", 0);
		//av_opt_set(st->ctx, "profile", "baseline", 0);   //有错误	

	(4)添加extradata信息：下一步发送sps pps要用到
		st->ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//by aphero,添加后ffmpeg会将sps pps存入到extradata中，
	
	(5)添加关键帧判断，主要是判断Ｉ帧，然后在Ｉ帧前发送sps pps:
		(a):在encode函数开头申明部分添加
			int key_frame=0;//by aphero
		(b):在这个
			err = mbuf_write_mem(st->mb, pkt->data, pkt->size);
			st->mb->pos = 0;下面添加
			if(pkt->flags & AV_PKT_FLAG_KEY){//by aphero
				key_frame=1;
			}
		(c):在
			case AV_CODEC_ID_H264:下面添加
				if((st->ctx->extradata)&&(key_frame==1)){//by aphero
					err = h264_packetize(ts, st->ctx->extradata, st->ctx->extradata_size, st->encprm.pktsize, st->pkth, st->arg);
					key_frame=0;
		(6)修改
            enum {
	            DEFAULT_GOP_SIZE =   25,//by aphero
            };
        (7)修改
if (st->codec_id == AV_CODEC_ID_H264) {
		//av_opt_set(st->ctx->priv_data, "profile", "baseline", 0);//by aphero 注释掉
（5）修改src/video.c文件，里面的发送过程分析，见原0.5.8版本中的video.c文件中有分析过程
    1.enum {
	    //	MEDIA_POLL_RATE = 250,                 /**< in [Hz]             */
		MEDIA_POLL_RATE = 200,                 /**< in [Hz]             */每隔4ms调度
	    BURST_MAX       = 81920*2,                /**< in bytes            *///by aphero    添加
	2.在vidqueue_poll函数中
        注释掉下面部分，添加突发为最大
	        //bandwidth_kbps = vtx->video->cfg.bitrate / 1000;
	        //burst = (1 + jfs - prev_jfs) * bandwidth_kbps / 4;
	        //burst = min(burst, BURST_MAX);
	        burst = BURST_MAX;//by aphero,重要
    3.在video_encoder_set函数中修改
        prm.pktsize = 1400;//by aphero 原来是:1024
    4.在encode_rtp_send()函数中修改
    	if (!sendq_empty) {//sendq_empty=0跳过帧,队列不为空就跳过此帧 by aphero
			//info("video:send frame skipc=%d sendq=%d \n",vtx->skipc,list_count(&vtx->sendq));
			++vtx->skipc;
			//return;//这里为什么要返回，继续发送不行吗＊＊＊＊暂时禁止，继续编码，存入队列＊＊＊＊＊＊＊这里意思应该是，上一包（整个帧）未发送完成，就跳过本包＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
		}

（6）修改omx＿render不能切换视频问题
        在omx_display_enable函数中
	        (1):运行中切换视频，需要先关闭端口，再释放内存
                在OMX_ERRORTYPE err = 0;下面添加
	            OMX_STATETYPE eState;//by ahpero
	            info("omx:omx_update_size %d %d\n", width, height);
	            
	            //by aphero 2018-2-23	
	            OMX_GetState(st->video_render, &eState);
	            if (eState == OMX_StateExecuting) {
		            err = OMX_SendCommand(st->video_render,OMX_CommandStateSet, OMX_StateIdle, NULL);//可以不用设置
		            //info("OMX_SendCommand  OMX_StateIdle err=%d\n",err);
		            err = OMX_SendCommand(st->video_render,OMX_CommandStateSet, OMX_StateLoaded, NULL);//可以不用设置
		            //info("OMX_SendCommand  err=%d\n",err);
		            err = OMX_SendCommand(st->video_render,OMX_CommandFlush, VIDEO_RENDER_PORT, NULL);
		            //info("OMX_CommandFlush  err=%d\n",err);
		            err = OMX_SendCommand(st->video_render,OMX_CommandPortDisable, VIDEO_RENDER_PORT, NULL);//必须关闭端口使能，否则无法设置端口
		            //if (err) warning("OMX_CommandPortDisable: err\n");
		            
		            //释放渲染器的缓存空间，这里必须释放，否则vidframe_init_buf出现段错误
		            if (st->buffers) {
			            OMX_FreeBuffer(st->video_render, VIDEO_RENDER_PORT,st->buffers[0]); //默认是3个，portdef.nBufferCountActual＝３
			            OMX_FreeBuffer(st->video_render, VIDEO_RENDER_PORT,st->buffers[1]); 
			            OMX_FreeBuffer(st->video_render, VIDEO_RENDER_PORT,st->buffers[2]); 
			            free(st->buffers);
			            st->buffers=NULL;
			            st->num_buffers = 0;
			            st->current_buffer = 0;
		            }
	            }
	            info("omx_update_size %d %d\n", width, height);
	        (2):如果端口关闭，需再次打开
		        info("1-omx port definition: h=%d w=%d s=%d sh=%d　bEnabled＝%d\n",	portdef.format.video.nFrameWidth,portdef.format.video.nFrameHeight,			   portdef.format.video.nStride,portdef.format.video.nSliceHeight,portdef.bEnabled);

			        if(portdef.bEnabled != 1){//如果端口关闭就打开
				        err |=OMX_SendCommand(st->video_render,OMX_CommandPortEnable, VIDEO_RENDER_PORT, NULL);//前面关闭了，一定要打开,aphero
			        }
	        (3):1080P不能播放问题：出现端口定义出错
		        修改端口定义，但是视频下面有一道蓝边
		        portdef.format.video.nStride = 0;//设置为０,1080p不出错 原portdef.format.video.nStride = stride;
		        portdef.format.video.nSliceHeight = 0;//设置为０，原portdef.format.video.nSliceHeight = height;
            (4)也可以修改显示窗口大小，好像有问题
                #ifdef RASPBERRY_PI
	                memset(&config, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
	                config.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
	                config.nVersion.nVersion = OMX_VERSION;
	                config.nPortIndex = VIDEO_RENDER_PORT;
	                config.fullscreen = 1;
	                config.set = OMX_DISPLAY_SET_FULLSCREEN;
	                //config.fullscreen = OMX_FALSE;
	                //config.noaspect   = OMX_TRUE;
	                //config.set                 = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_DEST_RECT|OMX_DISPLAY_SET_SRC_RECT|OMX_DISPLAY_SET_FULLSCREEN|OMX_DISPLAY_SET_NOASPECT);
	                //config.dest_rect.x_offset  = 0;
	                //config.dest_rect.y_offset  = 0;
	                //config.dest_rect.width     = 1920;
	                //config.dest_rect.height    = 1080;
（7）avformat修改
    1.在alloc()函数中：int input_fps = 0;下面添加：
	char h264dec[64];
	AVDictionary* options = NULL;
	av_dict_set(&options, "buffer_size", "4096000", 0);
	av_dict_set(&options, "max_delay", "500000", 0);
	av_dict_set(&options, "stimeout", "20000000", 0);  //设置超时断开连接时间
	//av_dict_set(&options, "rtsp_transport", "tcp", 0);  //以udp方式打开，如果以tcp方式打开将udp替换为tcp  
	//av_dict_set(&options, "probesize", "4096", 0);
    2.在alloc()函数st->fps    = prm->fps;下面添加	
	avformat_network_init(); //by aphero
    3.在alloc()函数中添加硬件解码，因为AVformat是先解码成yuv格式后，在编码，不管以前是不是h264还是MP4等
    修改这部分代码：if (ctx->codec_id != AV_CODEC_ID_NONE) {
			st->codec = avcodec_find_decoder(ctx->codec_id);
    修改如下：
        if (0 == conf_get_str(conf_cur(), "avcodec_h264dec", h264dec, sizeof(h264dec))) {
				info("avformat: using h264 decoder by name (%s)\n", h264dec);
			 st->codec = avcodec_find_decoder_by_name("h264_mmal");//by aphero  直接指定了解码器，不妥
		}else{
				        st->codec = avcodec_find_decoder(ctx->codec_id);
	   }
    4.	添加参数
        //ret = avformat_open_input(&st->ic, dev, NULL, NULL);
	    ret = avformat_open_input(&st->ic, dev, NULL, &options);//by phero
    5.显示文件信息
        #if 1 //by aphero 改为1
	        av_dump_format(st->ic, 0, dev, 0);
        #endif
（8）baresip-0.5.10版本中已添加视频源切换功能，/vidsrc ....
    /vidsrc avformat,/home/pi/aidedaijia.mkv

(9)修改src/stream.c文件
	1.修改接收缓存大小
	enum {
		//RTP_RECV_SIZE = 8192,
		RTP_RECV_SIZE = 81920,//by aphero
	2.在stream_sock_alloc（）函数中添加
	//udp_sockbuf_set(rtp_sock(s->rtp), 65536);
	udp_sockbuf_set(rtp_sock(s->rtp), 20*1024*1024);//by aphero 
5.安装自定义模块
(1)安装omxcam摄像头模块
1.安装libomxcam库
   a.make -f Makefile-shared
   用动态连接库，静态或直接编译有问题，注意关闭调试ｄｅｂｕｇ
   b.复制so文件和h文件到/usr/local/lib include
2.在modules.mk中添加2部分内容
USE_OMXCAM := $(shell [ -f $(SYSROOT)/include/omxcam.h ] || \
	[ -f $(SYSROOT)/local/include/omxcam.h ] || \
	[ -f $(SYSROOT)/include/$(MACHINE)/omxcam.h ] || \
	[ -f $(SYSROOT_ALT)/include/omxcam.h ] && echo "yes")

    ifneq ($(USE_OMXCAM),)
    MODULES   += omxcam
    endif
3.修改代码，在0.5.10版本中发送帧函数参数发生变化
    st->frameh(&frame,timestamp, st->arg);增加了timestamp参数
    参考cairo模块代码改写
    开头定义uint64_t ts = 0, ts_start;
    在on_data_time函数中
	if (current == frame_size){//接收到一个完整的帧
		uint64_t now;
		uint64_t timestamp;
		now = tmr_jiffies();
		if (!ts) {
			ts = ts_start = now;
		}
		if (ts > now) return;
		timestamp = (ts - ts_start) * VIDEO_TIMEBASE / 1000;
        timestamp = (ts - ts_start) * 90000 / 1000;//应该是这个
        

        在结束处修改：
		st->frameh(&frame,timestamp, st->arg);//这里直接发送yuv帧，会导致帧速率下降，采用线程有一定的缓解，但是还是达不到满速率
		ts += 1000/st->prm.fps;

(2)安装webapp控制界面，生成的配置文件在~/.baresip/下json文件
   1. 在mk/modules.mk中MODULES   += menu contact vumeter mwi account natpmp httpd 添加
    MODULES   += menu contact vumeter mwi account natpmp httpd webapp
    2.网页文件采用16进制方式，在module-aphero/webapp-html文件夹下使用htmltohex.py或html-hex.py将原html文件转换成hex，然后复制到modules/webapp/assets/相应的h文件中，需手动计算长度len

----------------------------2018年7月18日-上午-----------------
程序在切换视频源的时候还有问题，1是切换花屏，2是有时候切换后半天不出图像，卡住不动,真妈的！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
问题原因：因为新版改写了发送时候使用采集时候的时间戳，导致切换视频后，时间戳重新开始计数，导致freeswitch不能及时回传给客户端。
原来版本0.5.8中切换视频时间戳继续增量，不重新计数。
解决办法：使用0.5.8版本中的方法，不使用采集时间戳，采用编码器中pts自增量
在avcodec中encode.c
1.添加
struct videnc_state {
	AVCodec *codec;
	AVCodecContext *ctx;
	AVFrame *pict;
	struct mbuf *mb;
	size_t sz_max; /* todo: figure out proper buffer size */
	int64_t pts;//by aphero   增加
2.	不使用采集时间戳
    //st->pict->pts = timestamp;
	st->pict->pts = st->pts++;//by aphero 
3.计算时间戳
	//ts = video_calc_rtp_timestamp_fix(pts);
	ts = video_calc_rtp_timestamp(pts, st->encprm.fps);//by aphero
这个baresip-aphero-0.5.10-20180718.tar.gz为修改后版本，可正常使用

-----------------------------------------2018年7月18日下午-------------------------------------------
1.添加vidosd模块，用于显示台标----目前使用cairo现实，cpu有点高
        1.复制vidosd文件夹到modules目录下，修改mk/modules.mk文件添加
        ifneq ($(USE_CAIRO),)
        MODULES   += cairo
        MODULES   += vidinfo
        MODULES   += vidosd
        2.修改中文字体 ，在panel_osd.c文件中
	        cairo_select_font_face (panel->cr, "WenQuanYi Micro Hei Mono",//下载中文字体，使用 "fc-list" 命令查看系统所安装字体
				        CAIRO_FONT_SLANT_NORMAL,
				        CAIRO_FONT_WEIGHT_NORMAL);
        需下载中文字体，使用 "fc-list" 命令查看系统所安装字体https://www.libsdl.org/projects/SDL_image/release/SDL2_image-2.0.3.tar.gz
        sudo apt-get install ttf-wqy-microhei  #文泉驿-微米黑
        sudo apt-get install ttf-wqy-zenhei  #文泉驿-正黑
        sudo apt-get install xfonts-wqy #文泉驿-点阵宋体
        3.设置osd台标，在vidosd.c中encode()函数中
        err = panel_alloc_osd(&st->panel, "武警青海总队", 0, width, height);
        4.在config中设置
        # Video filter Modules (in encoding order)
        module			vidosd.so
        module			selfview.so
2.树莓派支持omx和sdl2两种显示模式
    sudo apt-get install libsdl2-dev
    有问题，暂时不管了，提示找不到设备
-----------------------------------------2018年7月24日晚-------------------------------------------
关于SDL2的问题
树莓派中默认是SDL1.2,即使sudo apt-get install libsdl2-dev，没有找到libsdl2.so这个文件，可能有。
SDL2已经支持树莓派了，使用opengles，可实现硬件加速，但是没有直接使用omx显示的速率高，有点卡顿，vidloop h264自环时候fps为15左右
安装方法：不能使用apt-get安装
1.下载4个源码包，只要SDL2这个包也行
SDL2：https://www.libsdl.org/release/SDL2-2.0.8.tar.gz
SDL_mixer：https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-2.0.2.tar.gz
SDL2_ttf：https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.14.tar.gz
SDL2_net：https://www.libsdl.org/projects/SDL_net/release/SDL2_net-2.0.1.tar.gz
SDL2_image：https://www.libsdl.org/projects/SDL_image/release/SDL2_image-2.0.3.tar.gz
2.编译 
  ./autogen.sh   ./configure  能够自动识别树莓派  make   sudo make install
3.修改baresip的SDL2的mk文件，如下：
	MOD		:= sdl2
	$(MOD)_SRCS	+= sdl.c
	$(MOD)_CFLAGS	:= -g -O2 -D_REENTRANT -I/usr/local/include/SDL2 -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux  -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_OPENGL -DHAVE_SDL_TTF -g
	$(MOD)_LFLAGS	+= -L/usr/local/lib -Wl,-rpath,/usr/local/lib -Wl,--enable-new-dtags -lSDL2

	include mk/mod.mk
就不会出现编译sdl2.so成功,但出现（没有有效的视频设备）
但是有点卡

SDL2隐藏鼠标：在display函数中添加SDL_ShowCursor(0);



