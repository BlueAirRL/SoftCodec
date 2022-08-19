package io.github.brucewind.softcodec;

import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import android.util.Log;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Created by bruce on 16-11-27.
 *
 * It is used to manager picture to be encode, and transfer encoded data to server in {@link ExecutorService}.
 */
public class RtmpHelper {
  private static String LOGTAG="JLiveCamera_encoder";
  private ExecutorService mRtmpExecutor = Executors.newSingleThreadExecutor();
  private Timer mTimer;
  private final AtomicInteger mFpsAtomic = new AtomicInteger(0);
  private final StreamHelper mStreamHelper = new StreamHelper();

  public interface H264BufferInterface{
    public void OnH264(int type, byte[] buffer);
  }

  private H264BufferInterface h264BufferInterface;

  /*rtmp*/
  public int rtmpOpen(final String url) {
    mTimer = new Timer();
    mTimer.schedule(new TimerTask() {
      @Override
      public void run() {
        Log.d("RtmpHelper", "fps = " + mFpsAtomic.get());
        mFpsAtomic.set(0);
      }
    }, 1000, 1000);

    return mStreamHelper.rtmpOpen(url);
  }

  public int rtmpStop() {
    if (mTimer != null) {
      mTimer.cancel();
    }

    mRtmpExecutor.execute(new Runnable() {
      @Override
      public void run() {
        mStreamHelper.rtmpStop();
        mRtmpExecutor.shutdown();
        try {
          final ExecutorService temp = mRtmpExecutor;
          temp.awaitTermination(1, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
          e.printStackTrace();
        }

        mRtmpExecutor = Executors.newSingleThreadExecutor();

      }
    });
    return 0;
  }

  /*x264 funtion*/
  public int compressBuffer(final long encoder, final byte[] NV12, final int NV12size,
      final byte[] H264) {
    mRtmpExecutor.execute(new Runnable() {
      @Override
      public void run() {
        H264Info info = mStreamHelper.compressBuffer(encoder, NV12, NV12size, H264);
        if(info == null){
          return;
        }

        Log.d(LOGTAG, "compressBuffer("+NV12size+")" + " len="+info.encodedLen);
        mFpsAtomic.incrementAndGet();
        if (h264BufferInterface != null && info.encodedLen > 0){
          byte[] buffer = new byte[info.encodedLen];
          System.arraycopy(H264, 0, buffer, 0, info.encodedLen);
//          StreamFile.writeBytes(buffer);
          h264BufferInterface.OnH264(info.frameType, buffer);
        }

        mStreamHelper.compressBuffer(encoder, NV12, NV12size, H264);
        mFpsAtomic.incrementAndGet();
      }
    });
    return 0;
  }

  public long compressBegin(final int width, int height, int bitrate, int fps, H264BufferInterface callback) {
    h264BufferInterface = callback;
    return mStreamHelper.compressBegin(width, height, bitrate, fps);
  }

  public int requestKeyFrame(long encoder){
    return mStreamHelper.requestKeyFrame(encoder);
  }

  public int setBitrate(long encoder, int bitrate){
    return mStreamHelper.setBitrate(encoder, bitrate);
  }

  public int compressEnd(long encoder) {
    return mStreamHelper.compressEnd(encoder);
  }

  public void startRecordeAudio(int currenttime) {
    AudioRecorder.startRecorde(currenttime);
  }

  public void stopRecordeAudio() {
    AudioRecorder.stopRecorde();
  }
}
