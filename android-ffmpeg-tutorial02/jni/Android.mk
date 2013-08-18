LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := tutorial02
LOCAL_SRC_FILES := tutorial02.c
LOCAL_LDLIBS := -llog -ljnigraphics -lz -landroid
LOCAL_SHARED_LIBRARIES := libavformat libavcodec libswscale libavutil

include $(BUILD_SHARED_LIBRARY)
$(call import-module,ffmpeg-2.0.1/android/arm)
