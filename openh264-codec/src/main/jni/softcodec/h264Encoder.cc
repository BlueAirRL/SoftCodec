
#include "h264Encoder.h"
#include <iostream>
#include <string>
#include "print_hex.h"

static ISVCEncoder *_encoder;
static SEncParamExt param;

unsigned char *sps;
unsigned char *pps;
int sps_len;
int pps_len;
int first = 0;

/**
 *
 * @param NAL_type
 * @param buffer , must not begin with start-code such as "00 00 00 01"
 * @param len
 * @return
 */
int handle_sps_pps(int NAL_type, unsigned char *buffer, int len) {
    if (buffer[0] == 0) {
        ALOGE("buffer format is wrong.");
        return -1;
    }
    if (NAL_type == NAL_SPS) {
        sps = buffer;
        sps_len = len;
        print_hex("SPS", sps, sps_len);
    } else if (NAL_type == NAL_PPS) {
        pps = buffer;
        pps_len = len;
        send_video_sps_pps(sps, sps_len, pps, pps_len);
        print_hex("PPS", pps, pps_len);// print
    } else {
        ALOGE("It is not either SPS or PPS.");
        return -1;
    }

    return 0;
}

int handle_other_NAL(unsigned char *buffer, int len) {

    send_rtmp_video(buffer,
                    len,
                    getSystemTime());     //into.uiTimeStamp
    return 0;
}

#define RC_MARGIN 10000 /*bits per sec*/
extern "C" JNIEXPORT  jlong
Java_io_github_brucewind_softcodec_StreamHelper_compressBegin(JNIEnv *env,
                                                              jobject thiz,
                                                              jint width,
                                                              jint height,
                                                              jint bitrate,
                                                              jint fps) {

    ISVCEncoder *encoder;
    WelsCreateSVCEncoder(&encoder);
    encoder->GetDefaultParams(
            &param);//call InitializeExt must have a extension parameter with this default.
    param.iUsageType = CAMERA_VIDEO_REAL_TIME;  //tell openH264 steam type is LIVE.
    param.iRCMode = RC_BITRATE_MODE;
    param.iMaxBitrate = UNSPECIFIED_BIT_RATE;
    param.fMaxFrameRate = fps;                  //fps
    param.iPicWidth = width;
    param.iPicHeight = height;
    param.iTargetBitrate = bitrate * 1024;        //the input parameter need to * 1024
//    param.iInputCsp = videoFormatI420;          //it is stable YUV format for openh264
//    param.bEnableDenoise = 1;                   //enable eliminating noisy.
    param.iSpatialLayerNum = 1;
    //if (sliceMode != SM_SINGLE_SLICE && sliceMode != SM_DYN_SLICE) //SM_DYN_SLICE don't support multi-thread now
    param.iMultipleThreadIdc = 2;
    param.eSpsPpsIdStrategy = SPS_LISTING;
    param.uiMaxNalSize = 0;
    param.uiIntraPeriod = fps*2;
    param.iMinQp = 20;
    param.iMaxQp = 30;

    for (int i = 0; i < param.iSpatialLayerNum; i++) {
        param.sSpatialLayers[i].iVideoWidth = width >> (param.iSpatialLayerNum - 1 - i);
        param.sSpatialLayers[i].iVideoHeight = height >> (param.iSpatialLayerNum - 1 - i);
        param.sSpatialLayers[i].fFrameRate = fps;
        param.sSpatialLayers[i].iSpatialBitrate = param.iTargetBitrate;
        //param.sSpatialLayers[i].sSliceCfg.uiSliceMode = sliceMode;
    }
    param.iTargetBitrate *= param.iSpatialLayerNum;
    int inited_status = encoder->InitializeExt(&param);

    if (inited_status == 0) {
        _encoder = encoder;
    } else {
        ALOGE("encoder init failed.");
        return 0;
    }
    return (jlong) encoder;
}
extern "C" JNIEXPORT  jint
Java_io_github_brucewind_softcodec_StreamHelper_requestKeyFrame(JNIEnv * env ,
jobject thiz, jlong){
    //find initialized encode by pointer address.
    ISVCEncoder *en = (ISVCEncoder *) _encoder;
    if(!en){
        return -1;
    }
    return en->ForceIntraFrame(true);
}

extern "C" JNIEXPORT jint
Java_io_github_brucewind_softcodec_StreamHelper_setBitrate(JNIEnv * env ,
                                                           jobject thiz, jlong, jint bitrate){
    //find initialized encode by pointer address.
    ISVCEncoder *en = (ISVCEncoder *) _encoder;
    if(!en){
        return -1;
    }
    return en->SetOption(ENCODER_OPTION_BITRATE, &bitrate);
}


extern "C" JNIEXPORT jint
Java_io_github_brucewind_softcodec_StreamHelper_setFramerate(JNIEnv * env ,
                                                             jobject thiz, jlong, jint framerate){
    //find initialized encode by pointer address.
    ISVCEncoder *en = (ISVCEncoder *) _encoder;
    if(!en){
        return -1;
    }
    return en->SetOption(ENCODER_OPTION_FRAME_RATE, &framerate);

}

/**
 * When compress end, clear some resource
 */
extern "C" JNIEXPORT  jint
Java_io_github_brucewind_softcodec_StreamHelper_compressEnd(
        JNIEnv *env,
        jobject thiz,
        jlong handle
) {
    return 0;
}

using namespace std;
/**
 * There are two steps:
 * 1. compressing the buffer data into serveral NALUs;
 * 2. transferring the NALUs to RTMP server.
 * return 0 means everything is ok.
 */
extern "C" JNIEXPORT  jobject Java_io_github_brucewind_softcodec_StreamHelper_compressBuffer(
        JNIEnv *env,
        jobject thiz,
        jlong handler,//it is a pointer.
        jbyteArray in,
        jint input_size,
        jbyteArray out) {


//    if (!isConnected())
//        return 0;


    int width = param.iPicWidth;
    int height = param.iPicHeight;


    //find initialized encode by pointer address.
    ISVCEncoder *en = (ISVCEncoder *) _encoder;

    int i_data = 0;
    int count_of_NALU = -1;//A frame may be separated into several NALUs.
    int result = 0;
    int i = 0;
    int i_frame_size = 0;
    int j = 0;
    int nPix = 0;
//  jbyte *c_array = env->GetByteArrayElements(j_array, 0);
    jbyte *nv12_buf = (jbyte *) env->GetByteArrayElements(in, 0);
    jbyte *h264_buf = (jbyte *) env->GetByteArrayElements(out, 0);
    unsigned char *pTemp = (unsigned char *) h264_buf;
    int nPicSize = width * height;

    jclass cls = env->FindClass("io/github/brucewind/softcodec/H264Info");
    jmethodID jmi = env->GetMethodID(cls,"<init>", "()V");
    jmethodID  jsetFrameType = env->GetMethodID(cls,"setFrameType","(I)V");
    jmethodID  jsetEncodedLen = env->GetMethodID(cls,"setEncodedLen","(I)V");
    jobject jh264 = env->NewObject(cls, jmi);




    //1. the first step: encode header with PPS & SPS.
//    if (first == 0) {
        SFrameBSInfo fbi = {0};
        int pps_suc = _encoder->EncodeParameterSets(&fbi);
        if (pps_suc != 0) {
            ALOGE("PPS & SPS encoding got failed.");
            return nullptr;
        }
//    }

//        handle_sps_pps(fbi);


    //encode and  store ouput bistream
    int frameSize = width * height * 3 / 2;

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));
    SSourcePicture pic;
    memset(&pic, 0, sizeof(SSourcePicture));
    pic.iPicWidth = width;
    pic.iPicHeight = height;
    pic.iColorFormat = videoFormatI420;//OpenH264 encoder only support this format.
    pic.iStride[0] = pic.iPicWidth;
    pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
    pic.pData[0] = reinterpret_cast<unsigned char *>(nv12_buf);
    pic.pData[1] = pic.pData[0] + width * height;
    pic.pData[2] = pic.pData[1] + (width * height >> 2);

    //prepare input data
    int rv = _encoder->EncodeFrame(&pic, &info);
    if (rv != cmResultSuccess) {//failed.
        if (rv == cmInitParaError) {
            ALOGE("openh264 parameter is wrong.");
        } else {
            ALOGE("encodeFrame falied, the result is %d.", rv);
        }
        return nullptr;
    }
    if (info.iLayerNum <= 0) {
        ALOGE("something wrong in \"i_frame_size < 0\".");
        return nullptr;
    }
    if (info.iLayerNum > 1) {
        ALOGI("data lost due to that iLayerNum=%d.", info.iLayerNum);
    }
    if (info.eFrameType == videoFrameTypeInvalid) {
        ALOGE("videoFrameTypeInvalid");
        return nullptr;
    } else if (info.eFrameType == videoFrameTypeSkip) {//it need to skip.
        ALOGW("skip this frame");
        return nullptr;
    } else {
        ALOGD("FrameType : %d, layertype: %d.", info.sLayerInfo[i].eFrameType,
              info.sLayerInfo[i].uiLayerType);
    }


    /**
     * 2. The second step:
     * transmit serveral NALUs to RTMP server.
     * Two NAL types : SPS & PPS are very important.
     */
     int frameType;
     int spsLen = 0, ppsLen = 0;
    for (i = 0; i < info.iLayerNum; i++) {
//        int fType = info.sLayerInfo[i].eFrameType;//it is NON_VIDEO_CODING_LAYER
//        if (info.sLayerInfo[i].uiLayerType == NON_VIDEO_CODING_LAYER) {//this NAL type is SPS.
//        }
        const SLayerBSInfo layerInfo = info.sLayerInfo[i];
        frameType = layerInfo.eFrameType;
        env->CallVoidMethod(jh264,jsetFrameType,frameType);
        unsigned char *buffer = layerInfo.pBsBuf;


        for (auto index_nal = 0; index_nal < layerInfo.iNalCount; index_nal++) {

            const int buf_len = layerInfo.pNalLengthInByte[index_nal];


            //detect NAL
            int NAL_type = 0;
            int start_len = 0;
            if (buffer[2] == 0x00) { /*00 00 00 01*/
                NAL_type = buffer[4] & 0x1f;
                start_len = 4;
            } else if (buffer[2] == 0x01) { /*00 00 01*/
                NAL_type = buffer[3] & 0x1f;
                start_len = 3;
            } else {
                if (buffer[0] != 0) {
                    NAL_type = buffer[0] & 0x1f;
                } else {
                    result = 1;
                    ALOGE("start code not found.");
                }
            }
            ALOGD("PPPPPPPPP NAL type : %d. buf_len=%d frameType=%d",NAL_type, buf_len, frameType);
            //begin to handle one of NALs.
            if (NAL_type == NAL_SPS){
                spsLen = buf_len;
                memcpy(pTemp, buffer, buf_len);
            }
            if (NAL_type == NAL_PPS){
                ppsLen = buf_len;
                memcpy(pTemp + spsLen, buffer, buf_len);
            }

            if (NAL_type == NAL_SPS || NAL_type == NAL_PPS) {
//                buffer += start_len;

//                handle_sps_pps(NAL_type, buffer, buf_len - start_len);
//                buffer+=(buf_len - start_len);
            } else {
                memcpy(pTemp + spsLen + ppsLen, buffer, buf_len);
//                handle_other_NAL(buffer, buf_len);
//                buffer += buf_len;
            }

            env->CallVoidMethod(jh264, jsetEncodedLen,buf_len + spsLen+ ppsLen );
        }

        ALOGD("send_rtmp_video() end");

        //release buffers.
        env->ReleaseByteArrayElements(in, nv12_buf, 0);
        env->ReleaseByteArrayElements(out, h264_buf, 0);
    }
    return jh264;
}
