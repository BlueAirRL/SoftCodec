package io.github.brucewind.softcodec;

public class H264Info {

    public int frameType;
    public int encodedLen;

    public int getFrameType() {
        return frameType;
    }

    public void setFrameType(int frameType) {
        this.frameType = frameType;
    }

    public int getEncodedLen() {
        return encodedLen;
    }

    public void setEncodedLen(int encodedLen) {
        this.encodedLen = encodedLen;
    }
}