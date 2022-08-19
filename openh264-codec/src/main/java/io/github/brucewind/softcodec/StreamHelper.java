package io.github.brucewind.softcodec;

/**
 * Created by bruce on 16-11-24.
 * It is used to connect server and push stream.
 */
class StreamHelper {


    static {
        System.loadLibrary("softcodec");
    }

    /**
     * connect rtmp server
     */
    public native int rtmpOpen(String url);

    public native int rtmpStop();

    /**
     * x264 function
     * @param encoder
     * @param I420
     * @param I420Size
     * @param H264, it is unused.
     * @return
     */
    public native H264Info compressBuffer(long encoder, byte[] I420, int I420Size, byte[] H264);

    public native long compressBegin(int width, int height, int bitrate, int fps);

    public native int requestKeyFrame(long encoder);

    public native int setBitrate(long encoder, int bitrate);

    public native int setFramerate(long encoder, int framerate);

    public native int compressEnd(long encoder);

}
