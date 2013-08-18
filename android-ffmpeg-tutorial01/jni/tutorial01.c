// tutorial01.c
//
// This tutorial was based on the code written by Stephen Dranger (dranger@gmail.com).
//
// The code is modified so that it can be compiled to a shared library and run on Android
//
// The code dumps first 5 fives of an input video file to /sdcard/android-ffmpeg-tutorial01
// folder of the external storage of your Android device
//
// Feipeng Liu (http://www.roman10.net/)
// Aug 2013

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

#include <stdio.h>
#include <wchar.h>

#include <jni.h>

/*for android logs*/
#include <android/log.h>

#define LOG_TAG "android-ffmpeg-tutorial01"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

void SaveFrame(JNIEnv *pEnv, jobject pObj, jobject pBitmap, int width, int height, int iFrame) {
	char szFilename[200];
	jmethodID sSaveFrameMID;
	jclass mainActCls;
	sprintf(szFilename, "/sdcard/android-ffmpeg-tutorial01/frame%d.jpg", iFrame);
	mainActCls = (*pEnv)->GetObjectClass(pEnv, pObj);
	sSaveFrameMID = (*pEnv)->GetMethodID(pEnv, mainActCls, "saveFrameToPath", "(Landroid/graphics/Bitmap;Ljava/lang/String;)V");
	LOGI("call java method to save frame %d", iFrame);
	jstring filePath = (*pEnv)->NewStringUTF(pEnv, szFilename);
	(*pEnv)->CallVoidMethod(pEnv, pObj, sSaveFrameMID, pBitmap, filePath);
	LOGI("call java method to save frame %d done", iFrame);
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

jint naMain(JNIEnv *pEnv, jobject pObj, jobject pMainAct, jstring pFileName, jint pNumOfFrames) {
	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	AVFrame         *pFrameRGBA = NULL;
	AVPacket        packet;
	int             frameFinished;
	jobject			bitmap;
	void* 			buffer;

	AVDictionary    *optionsDict = NULL;
	struct SwsContext      *sws_ctx = NULL;
	char *videoFileName;

	// Register all formats and codecs
	av_register_all();

	//get C string from JNI jstring
	videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);

	// Open video file
	if(avformat_open_input(&pFormatCtx, videoFileName, NULL, NULL)!=0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, videoFileName, 0);

	// Find the first video stream
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}
	if(videoStream==-1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameRGBA=avcodec_alloc_frame();
	if(pFrameRGBA==NULL)
		return -1;

	//create a bitmap as the buffer for pFrameRGBA
	bitmap = createBitmap(pEnv, pCodecCtx->width, pCodecCtx->height);
	if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0)
		return -1;
	//get the scaling context
	sws_ctx = sws_getContext
    (
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

	// Assign appropriate parts of bitmap to image planes in pFrameRGBA
	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGBA, buffer, AV_PIX_FMT_RGBA,
		 pCodecCtx->width, pCodecCtx->height);

	// Read frames and save first five frames to disk
	i=0;
	while(av_read_frame(pFormatCtx, &packet)>=0) {
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
			   &packet);
			// Did we get a video frame?
			if(frameFinished) {
				// Convert the image from its native format to RGBA
				sws_scale
				(
					sws_ctx,
					(uint8_t const * const *)pFrame->data,
					pFrame->linesize,
					0,
					pCodecCtx->height,
					pFrameRGBA->data,
					pFrameRGBA->linesize
				);

				// Save the frame to disk
				if(++i<=pNumOfFrames) {
					SaveFrame(pEnv, pMainAct, bitmap, pCodecCtx->width, pCodecCtx->height, i);
					LOGI("save frame %d", i);
				}
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	//unlock the bitmap
	AndroidBitmap_unlockPixels(pEnv, bitmap);

	// Free the RGB image
	av_free(pFrameRGBA);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
}

jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}
	JNINativeMethod nm[1];
	nm[0].name = "naMain";
	nm[0].signature = "(Lroman10/tutorial/android_ffmpeg_tutorial01/MainActivity;Ljava/lang/String;I)I";
	nm[0].fnPtr = (void*)naMain;
	jclass cls = (*env)->FindClass(env, "roman10/tutorial/android_ffmpeg_tutorial01/MainActivity");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, 1);
	return JNI_VERSION_1_6;
}
