LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := tutorial01
LOCAL_SRC_FILES := tutorial01.c
LOCAL_LDLIBS := -llog -ljnigraphics -lz 
LOCAL_SHARED_LIBRARIES := libavformat libavcodec libswscale libavutil

include $(BUILD_SHARED_LIBRARY)
$(call import-module,ffmpeg-2.0.1/android/arm)
