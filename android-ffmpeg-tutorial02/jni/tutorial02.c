// tutorial02.c
// A pedagogical video player that will stream through every video frame as fast as it can.
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
//
// The code is modified so that it can be compiled to a shared library and run on Android
//
// The code play the video stream on your screen
//
// Feipeng Liu (http://www.roman10.net/)
// Aug 2013


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

#include <stdio.h>
#include <pthread.h>

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define LOG_TAG "android-ffmpeg-tutorial02"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

ANativeWindow* 		window;
char 				*videoFileName;
AVFormatContext 	*formatCtx = NULL;
int 				videoStream;
AVCodecContext  	*codecCtx = NULL;
AVFrame         	*decodedFrame = NULL;
AVFrame         	*frameRGBA = NULL;
jobject				bitmap;
void*				buffer;
struct SwsContext   *sws_ctx = NULL;
int 				width;
int 				height;
int					stop;

jint naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
	AVCodec         *pCodec = NULL;
	int 			i;
	AVDictionary    *optionsDict = NULL;

	videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
	LOGI("video file name is %s", videoFileName);
	// Register all formats and codecs
	av_register_all();
	// Open video file
	if(avformat_open_input(&formatCtx, videoFileName, NULL, NULL)!=0)
		return -1; // Couldn't open file
	// Retrieve stream information
	if(avformat_find_stream_info(formatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	// Dump information about file onto standard error
	av_dump_format(formatCtx, 0, videoFileName, 0);
	// Find the first video stream
	videoStream=-1;
	for(i=0; i<formatCtx->nb_streams; i++) {
		if(formatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}
	if(videoStream==-1)
		return -1; // Didn't find a video stream
	// Get a pointer to the codec context for the video stream
	codecCtx=formatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(codecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(codecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec
	// Allocate video frame
	decodedFrame=avcodec_alloc_frame();
	// Allocate an AVFrame structure
	frameRGBA=avcodec_alloc_frame();
	if(frameRGBA==NULL)
		return -1;
	return 0;
}

jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
	int i;
	//get Bitmap class and createBitmap method ID
	jclass javaBitmapClass = (jclass)(*pEnv)->FindClass(pEnv, "android/graphics/Bitmap");
	jmethodID mid = (*pEnv)->GetStaticMethodID(pEnv, javaBitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	//create Bitmap.Config
	//reference: https://forums.oracle.com/thread/1548728
	const wchar_t* configName = L"ARGB_8888";
	int len = wcslen(configName);
	jstring jConfigName;
	if (sizeof(wchar_t) != sizeof(jchar)) {
		//wchar_t is defined as different length than jchar(2 bytes)
		jchar* str = (jchar*)malloc((len+1)*sizeof(jchar));
		for (i = 0; i < len; ++i) {
			str[i] = (jchar)configName[i];
		}
		str[len] = 0;
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)str, len);
	} else {
		//wchar_t is defined same length as jchar(2 bytes)
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)configName, len);
	}
	jclass bitmapConfigClass = (*pEnv)->FindClass(pEnv, "android/graphics/Bitmap$Config");
	jobject javaBitmapConfig = (*pEnv)->CallStaticObjectMethod(pEnv, bitmapConfigClass,
			(*pEnv)->GetStaticMethodID(pEnv, bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"), jConfigName);
	//create the bitmap
	return (*pEnv)->CallStaticObjectMethod(pEnv, javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}

jintArray naGetVideoRes(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
	if (NULL == codecCtx) {
		return NULL;
	}
	lRes = (*pEnv)->NewIntArray(pEnv, 2);
	if (lRes == NULL) {
		LOGI(1, "cannot allocate memory for video size");
		return NULL;
	}
	jint lVideoRes[2];
	lVideoRes[0] = codecCtx->width;
	lVideoRes[1] = codecCtx->height;
	(*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
	return lRes;
}

void naSetSurface(JNIEnv *pEnv, jobject pObj, jobject pSurface) {
	if (0 != pSurface) {
		// get the native window reference
		window = ANativeWindow_fromSurface(pEnv, pSurface);
		// set format and size of window buffer
		ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
	} else {
		// release the native window
		ANativeWindow_release(window);
	}
}

jint naSetup(JNIEnv *pEnv, jobject pObj, int pWidth, int pHeight) {
	width = pWidth;
	height = pHeight;
	//create a bitmap as the buffer for frameRGBA
	bitmap = createBitmap(pEnv, pWidth, pHeight);
	if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0)
		return -1;
	//get the scaling context
	sws_ctx = sws_getContext (
	        codecCtx->width,
	        codecCtx->height,
	        codecCtx->pix_fmt,
	        pWidth,
	        pHeight,
	        AV_PIX_FMT_RGBA,
	        SWS_BILINEAR,
	        NULL,
	        NULL,
	        NULL
	);
	// Assign appropriate parts of bitmap to image planes in pFrameRGBA
	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)frameRGBA, buffer, AV_PIX_FMT_RGBA,
			pWidth, pHeight);
	return 0;
}

void finish(JNIEnv *pEnv) {
	//unlock the bitmap
	AndroidBitmap_unlockPixels(pEnv, bitmap);
	av_free(buffer);
	// Free the RGB image
	av_free(frameRGBA);
	// Free the YUV frame
	av_free(decodedFrame);
	// Close the codec
	avcodec_close(codecCtx);
	// Close the video file
	avformat_close_input(&formatCtx);
}

void decodeAndRender(JNIEnv *pEnv) {
	ANativeWindow_Buffer 	windowBuffer;
	AVPacket        		packet;
	int 					i=0;
	int            			frameFinished;
	int 					lineCnt;
	while(av_read_frame(formatCtx, &packet)>=0 && !stop) {
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {
			// Decode video frame
			avcodec_decode_video2(codecCtx, decodedFrame, &frameFinished,
			   &packet);
			// Did we get a video frame?
			if(frameFinished) {
				// Convert the image from its native format to RGBA
				sws_scale
				(
					sws_ctx,
					(uint8_t const * const *)decodedFrame->data,
					decodedFrame->linesize,
					0,
					codecCtx->height,
					frameRGBA->data,
					frameRGBA->linesize
				);
				// lock the window buffer
				if (ANativeWindow_lock(window, &windowBuffer, NULL) < 0) {
					LOGE("cannot lock window");
				} else {
					// draw the frame on buffer
					LOGI("copy buffer %d:%d:%d", width, height, width*height*4);
					LOGI("window buffer: %d:%d:%d", windowBuffer.width,
							windowBuffer.height, windowBuffer.stride);
					memcpy(windowBuffer.bits, buffer,  width * height * 4);
					// unlock the window buffer and post it to display
					ANativeWindow_unlockAndPost(window);
					// count number of frames
					++i;
				}
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
	LOGI("total No. of frames decoded and rendered %d", i);
	finish(pEnv);
}

/**
 * start the video playback
 */
void naPlay(JNIEnv *pEnv, jobject pObj) {
	//create a new thread for video decode and render
	pthread_t decodeThread;
	stop = 0;
	pthread_create(&decodeThread, NULL, decodeAndRender, NULL);
}

/**
 * stop the video playback
 */
void naStop(JNIEnv *pEnv, jobject pObj) {
	stop = 1;
}

jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}
	JNINativeMethod nm[8];
	nm[0].name = "naInit";
	nm[0].signature = "(Ljava/lang/String;)I";
	nm[0].fnPtr = (void*)naInit;

	nm[1].name = "naSetSurface";
	nm[1].signature = "(Landroid/view/Surface;)V";
	nm[1].fnPtr = (void*)naSetSurface;

	nm[2].name = "naGetVideoRes";
	nm[2].signature = "()[I";
	nm[2].fnPtr = (void*)naGetVideoRes;

	nm[3].name = "naSetup";
	nm[3].signature = "(II)I";
	nm[3].fnPtr = (void*)naSetup;

	nm[4].name = "naPlay";
	nm[4].signature = "()V";
	nm[4].fnPtr = (void*)naPlay;

	nm[5].name = "naStop";
	nm[5].signature = "()V";
	nm[5].fnPtr = (void*)naStop;

	jclass cls = (*env)->FindClass(env, "roman10/tutorial/android_ffmpeg_tutorial02/MainActivity");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, 6);
	return JNI_VERSION_1_6;
}

