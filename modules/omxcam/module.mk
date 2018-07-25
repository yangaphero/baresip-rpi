#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

#使用动态库时候
#----------------
MOD		:= omxcam
$(MOD)_SRCS	+= omxcam.c 

$(MOD)_CFLAGS	:= -DRASPBERRY_PI -DOMX_SKIP64BIT \
		-isystem /opt/vc/include \
		-isystem /opt/vc/include/interface/vmcs_host/linux \
		-isystem /opt/vc/include/interface/vcos/pthreads
$(MOD)_LFLAGS	+= -lvcos -lbcm_host -lopenmaxil -L /opt/vc/lib -lomxcam
include mk/mod.mk
#----------------------------------

#使用静态编译时候
#----------------
#MOD		:= omxcam
#$(MOD)_SRCS	+= omxcam.c camera.c  core.c  debug.c  dump_omx.c  error.c  event.c  h264.c  jpeg.c   still.c  utils.c  version.c  video.c
#
#$(MOD)_CFLAGS	:= -DRASPBERRY_PI -DOMX_SKIP64BIT -Wno-strict-prototypes\
#		-isystem /opt/vc/include \
#		-isystem /opt/vc/include/interface/vmcs_host/linux \
#		-isystem /opt/vc/include/interface/vcos/pthreads \
#		-isystem /opt/vc/src/hello_pi/libs/ilclient
#$(MOD)_LFLAGS	+= -lvcos -lbcm_host -lopenmaxil -L /opt/vc/lib  -lcamkit
#include mk/mod.mk
