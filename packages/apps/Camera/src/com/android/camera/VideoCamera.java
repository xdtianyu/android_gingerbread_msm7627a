/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package com.android.camera;

import com.android.camera.gallery.IImage;
import com.android.camera.gallery.IImageList;
import com.android.camera.ui.CamcorderHeadUpDisplay;
import com.android.camera.ui.GLRootView;
import com.android.camera.ui.GLView;
import com.android.camera.ui.HeadUpDisplay;
import com.android.camera.ui.RotateRecordingTime;

import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.text.format.DateFormat;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.Size;
import android.media.CamcorderProfile;
import android.media.MediaRecorder;
import android.media.ThumbnailUtils;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.os.Message;
import android.os.StatFs;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.provider.Settings;
import android.provider.MediaStore.Video;
import android.util.Log;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.OrientationEventListener;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.MotionEvent;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ZoomButtonsController;

import android.location.Location;
import android.location.LocationManager;
import android.location.LocationProvider;
import android.media.MediaRecorder.PictureCallback;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.HashMap;
import java.util.Iterator;
import java.util.StringTokenizer;
import java.util.concurrent.CountDownLatch;
import java.lang.InterruptedException;

/**
 * The Camcorder activity.
 */
public class VideoCamera extends NoSearchActivity
        implements View.OnClickListener,
        ShutterButton.OnShutterButtonListener, SurfaceHolder.Callback,
        MediaRecorder.OnErrorListener, MediaRecorder.OnInfoListener,
        Switcher.OnSwitchListener, PreviewFrameLayout.OnSizeChangedListener {

    private static final String TAG = "videocamera";

    private static final int CLEAR_SCREEN_DELAY = 4;
    private static final int UPDATE_RECORD_TIME = 5;
    private static final int ENABLE_SHUTTER_BUTTON = 6;
    private static final int DELAYED_ONRESUME_FUNCTION = 7;

    private static final int SCREEN_DELAY = 1000;

    // The brightness settings used when it is set to automatic in the system.
    // The reason why it is set to 0.7 is just because 1.0 is too bright.
    private static final float DEFAULT_CAMERA_BRIGHTNESS = 0.7f;

    private static final long NO_STORAGE_ERROR = -1L;
    private static final long CANNOT_STAT_ERROR = -2L;
    private static final long LOW_STORAGE_THRESHOLD = 512L * 1024L;

    private static final int STORAGE_STATUS_OK = 0;
    private static final int STORAGE_STATUS_LOW = 1;
    private static final int STORAGE_STATUS_NONE = 2;
    private static final int STORAGE_STATUS_FAIL = 3;

    private static final boolean SWITCH_CAMERA = true;
    private static final boolean SWITCH_VIDEO = false;

    private static final long SHUTTER_BUTTON_TIMEOUT = 500L; // 500ms
    private CountDownLatch devlatch;

    /**
     * An unpublished intent flag requesting to start recording straight away
     * and return as soon as recording is stopped.
     * TODO: consider publishing by moving into MediaStore.
     */
    private final static String EXTRA_QUICK_CAPTURE =
            "android.intent.extra.quickCapture";

    private ComboPreferences mPreferences;
    private boolean mTakingPicture = false;
    private long mJpegPictureCallbackTime;
    private int mImageWidth = 0;
    private int mImageHeight = 0;
    private boolean mRecordLocation;
    private LocationManager mLocationManager = null;

    private PreviewFrameLayout mPreviewFrameLayout;
    private SurfaceView mVideoPreview;
    private SurfaceHolder mSurfaceHolder = null;
    private ImageView mVideoFrame;
    private GLRootView mGLRootView;
    private CamcorderHeadUpDisplay mHeadUpDisplay;

    private boolean mIsVideoCaptureIntent;
    private boolean mQuickCapture;
    // mLastPictureButton and mThumbController
    // are non-null only if mIsVideoCaptureIntent is true.
    private ImageView mLastPictureButton;
    private ThumbnailController mThumbController;
    private boolean mStartPreviewFail = false;

    private int mStorageStatus = STORAGE_STATUS_OK;

    private MediaRecorder mMediaRecorder;
    private boolean mMediaRecorderRecording = false;
    private long mRecordingStartTime;
    // The video file that the hardware camera is about to record into
    // (or is recording into.)
    private String mVideoFilename;
    private ParcelFileDescriptor mVideoFileDescriptor;

    // The video file that has already been recorded, and that is being
    // examined by the user.
    private String mCurrentVideoFilename;
    private Uri mCurrentVideoUri;
    private ContentValues mCurrentVideoValues;

    private CamcorderProfile mProfile;
    private int mVideoEncoder;
    private int mAudioEncoder;
    private int mVideoWidth;
    private int mVideoHeight;
    private int mVideoBitrate;
    private String mOutputFormat = "3gp";
    private int mVideoFps = 30;

    private boolean mZooming = false;
    private boolean mSmoothZoomSupported = false;
    private int mZoomValue;  // The current zoom value.
    private int mZoomMax;
    private ZoomButtonsController mZoomButtons;
    private GestureDetector mGestureDetector;
    private final ZoomListener mZoomListener = new ZoomListener();

    //
    // DefaultHashMap is a HashMap which returns a default value if the specified
    // key is not found.
    //
    @SuppressWarnings("serial")

    static class DefaultHashMap<K, V> extends HashMap<K, V> {
        private V mDefaultValue;

        public void putDefault(V defaultValue) {
            mDefaultValue = defaultValue;
        }

        @Override
        public V get(Object key) {
            V value = super.get(key);
            return (value == null) ? mDefaultValue : value;
        }

        public K getKey(V toCheck) {
            Iterator<K> it = this.keySet().iterator();
            V val;
            K key;
            while(it.hasNext()) {
                key = it.next();
                val = this.get(key);
                if (val.equals(toCheck)) {
                    return key;
                }
            }
        return null;
        }
    }


    private static final DefaultHashMap<String, Integer>
            OUTPUT_FORMAT_TABLE = new DefaultHashMap<String, Integer>();
    private static final DefaultHashMap<String, Integer>
            VIDEO_ENCODER_TABLE = new DefaultHashMap<String, Integer>();
    private static final DefaultHashMap<String, Integer>
            AUDIO_ENCODER_TABLE = new DefaultHashMap<String, Integer>();
    private static final DefaultHashMap<String, Integer>
            VIDEOQUALITY_BITRATE_TABLE = new DefaultHashMap<String, Integer>();

    static {
        OUTPUT_FORMAT_TABLE.put("3gp", MediaRecorder.OutputFormat.THREE_GPP);
        OUTPUT_FORMAT_TABLE.put("mp4", MediaRecorder.OutputFormat.MPEG_4);
        OUTPUT_FORMAT_TABLE.putDefault(MediaRecorder.OutputFormat.DEFAULT);

        VIDEO_ENCODER_TABLE.put("h263", MediaRecorder.VideoEncoder.H263);
        VIDEO_ENCODER_TABLE.put("h264", MediaRecorder.VideoEncoder.H264);
        VIDEO_ENCODER_TABLE.put("m4v", MediaRecorder.VideoEncoder.MPEG_4_SP);
        VIDEO_ENCODER_TABLE.putDefault(MediaRecorder.VideoEncoder.DEFAULT);

        AUDIO_ENCODER_TABLE.put("amrnb", MediaRecorder.AudioEncoder.AMR_NB);
        AUDIO_ENCODER_TABLE.put("qcelp", MediaRecorder.AudioEncoder.QCELP);
        AUDIO_ENCODER_TABLE.put("evrc", MediaRecorder.AudioEncoder.EVRC);
        AUDIO_ENCODER_TABLE.put("amrwb", MediaRecorder.AudioEncoder.AMR_WB);
        AUDIO_ENCODER_TABLE.put("aac", MediaRecorder.AudioEncoder.AAC);
        AUDIO_ENCODER_TABLE.put("aacplus", MediaRecorder.AudioEncoder.AAC_PLUS);
        AUDIO_ENCODER_TABLE.put("eaacplus",
                MediaRecorder.AudioEncoder.EAAC_PLUS);
        AUDIO_ENCODER_TABLE.putDefault(MediaRecorder.AudioEncoder.DEFAULT);

        VIDEOQUALITY_BITRATE_TABLE.put("1920x1088", 20000000);
        VIDEOQUALITY_BITRATE_TABLE.put("1280x720", 14000000);
        VIDEOQUALITY_BITRATE_TABLE.put("720x480",  2000000);
        VIDEOQUALITY_BITRATE_TABLE.put("864x480",  2000000);
        VIDEOQUALITY_BITRATE_TABLE.put("800x480",  2000000);
        VIDEOQUALITY_BITRATE_TABLE.put("640x480",  2000000);
        VIDEOQUALITY_BITRATE_TABLE.put("432x240",  720000);
        VIDEOQUALITY_BITRATE_TABLE.put("352x288",  720000);
        VIDEOQUALITY_BITRATE_TABLE.put("320x240",  512000);
        VIDEOQUALITY_BITRATE_TABLE.put("176x144",  192000);
        VIDEOQUALITY_BITRATE_TABLE.putDefault(320000);


    }

    // The video duration limit. 0 menas no limit.
    private int mMaxVideoDurationInMs;

    boolean mPausing = false;
    boolean mPreviewing = false; // True if preview is started.

    private ContentResolver mContentResolver;

    private ShutterButton mShutterButton;
    private RotateRecordingTime mRecordingTimeRect;
    private TextView mRecordingTimeView;
    private Switcher mSwitcher;
    private boolean mRecordingTimeCountsDown = false;
    private boolean mUnsupportedResolution = false;
    private boolean mUnsupportedHFRVideoSize = false;
    private boolean mUnsupportedHFRVideoCodec = false;

    private final ArrayList<MenuItem> mGalleryItems = new ArrayList<MenuItem>();

    private final Handler mHandler = new MainHandler();
    private Parameters mParameters;

    // multiple cameras support
    private int mNumberOfCameras;
    private int mCameraId;
    private int mCameraMode;
    private Menu mMenu;
    private boolean mHas3DSupport;

    private MyOrientationEventListener mOrientationListener;
    // The device orientation in degrees. Default is unknown.
    private int mOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    // The orientation compensation for icons and thumbnails. Degrees are in
    // counter-clockwise
    private int mOrientationCompensation = 0;
    private int mOrientationHint; // the orientation hint for video playback

    // This Handler is used to post message back onto the main thread of the
    // application
    private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {

                case DELAYED_ONRESUME_FUNCTION: {
                    delayedOnResume();
                    break;
                }
                case ENABLE_SHUTTER_BUTTON:
                    mShutterButton.setEnabled(true);
                    break;

                case CLEAR_SCREEN_DELAY: {
                    getWindow().clearFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    break;
                }

                case UPDATE_RECORD_TIME: {
                    updateRecordingTime();
                    break;
                }

                default:
                    Log.v(TAG, "Unhandled message: " + msg.what);
                    break;
            }
        }
    }

    private BroadcastReceiver mReceiver = null;

    private class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(Intent.ACTION_MEDIA_EJECT)) {
                updateAndShowStorageHint(false);
                stopVideoRecording();
            } else if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                updateAndShowStorageHint(true);
                updateThumbnailButton();
            } else if (action.equals(Intent.ACTION_MEDIA_UNMOUNTED)) {
                // SD card unavailable
                // handled in ACTION_MEDIA_EJECT
            } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_STARTED)) {
                Toast.makeText(VideoCamera.this,
                        getResources().getString(R.string.wait), 5000);
            } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_FINISHED)) {
                updateAndShowStorageHint(true);
            }
        }
    }

    private String createImageName(long dateTaken) {
       Date date = new Date(dateTaken);
       SimpleDateFormat dateFormat = new SimpleDateFormat(
                    "'Img'_yyyyMMdd_HHmmss");
       return dateFormat.format(date);
    }


    private void showResourceErrorAndFinish() {
        Resources res = getResources();
        Util.showFatalErrorAndFinish(VideoCamera.this,
                res.getString(R.string.camera_application_stopped),
                res.getString(R.string.camera_driver_needs_reset));
    }

    private String createName(long dateTaken) {
        Date date = new Date(dateTaken);
        SimpleDateFormat dateFormat = new SimpleDateFormat(
                getString(R.string.video_file_name_format));

        return dateFormat.format(date);
    }

    private void showCameraErrorAndFinish() {
        Resources ress = getResources();
        Util.showFatalErrorAndFinish(VideoCamera.this,
                ress.getString(R.string.camera_error_title),
                ress.getString(R.string.cannot_connect_camera));
    }

    private boolean restartPreview() {
        try {
            startPreview();
        } catch (CameraHardwareException e) {
            showCameraErrorAndFinish();
            return false;
        }
        return true;
    }

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        mPreferences = new ComboPreferences(this);
        CameraSettings.upgradeGlobalPreferences(mPreferences.getGlobal());
        mCameraId = CameraSettings.readPreferredCameraId(mPreferences);
        mCameraMode = CameraSettings.readPreferredCameraMode(mPreferences);

        devlatch = new CountDownLatch(1);
        /*
         * To reduce startup time, we start the preview in another thread.
         * We make sure the preview is started at the end of onCreate.
         */
        Thread startPreviewThread = new Thread(new Runnable() {
            CountDownLatch tlatch = devlatch;
            public void run() {
                try {
                    mStartPreviewFail = false;
                    ensureCameraDevice();

                    // Wait for framework initialization to be complete before
                    // starting preview
                    try {
                        tlatch.await();
                        } catch (InterruptedException ie) {
                        mStartPreviewFail = true;
                    }
                    startPreview();
                } catch (CameraHardwareException e) {
                    // In eng build, we throw the exception so that test tool
                    // can detect it and report it
                    if ("eng".equals(Build.TYPE)) {
                        throw new RuntimeException(e);
                    }
                    mStartPreviewFail = true;
                }
            }
        });
        startPreviewThread.start();

        Window win = getWindow();

        // Overright the brightness settings if it is automatic
        int mode = Settings.System.getInt(
                getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS_MODE,
                Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
        if (mode == Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC) {
            WindowManager.LayoutParams winParams = win.getAttributes();
            winParams.screenBrightness = DEFAULT_CAMERA_BRIGHTNESS;
            win.setAttributes(winParams);
        }

        mNumberOfCameras = CameraHolder.instance().getNumberOfCameras();
        if(mCameraId > (mNumberOfCameras-1)) {
            mCameraId = 0;
            mPreferences.setLocalId(this, mCameraId);
            CameraSettings.writePreferredCameraId(mPreferences, mCameraId);
        } else
            mPreferences.setLocalId(this, mCameraId);

        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());

        readVideoPreferences();
        // Initialize location sevice.
        mLocationManager = (LocationManager)
        getSystemService(Context.LOCATION_SERVICE);
        mRecordLocation = RecordLocationPreference.get(
        mPreferences, getContentResolver());
        if (mRecordLocation) startReceivingLocationUpdates();

        // Let thread know it's ok to continue
        devlatch.countDown();

        mContentResolver = getContentResolver();

        requestWindowFeature(Window.FEATURE_PROGRESS);
        setContentView(R.layout.video_camera);

        mPreviewFrameLayout = (PreviewFrameLayout)
                findViewById(R.id.frame_layout);
        mPreviewFrameLayout.setOnSizeChangedListener(this);
        resizeForPreviewAspectRatio();

        mVideoPreview = (SurfaceView) findViewById(R.id.camera_preview);
        mVideoFrame = (ImageView) findViewById(R.id.video_frame);

        // don't set mSurfaceHolder here. We have it set ONLY within
        // surfaceCreated / surfaceDestroyed, other parts of the code
        // assume that when it is set, the surface is also set.
        SurfaceHolder holder = mVideoPreview.getHolder();
        holder.addCallback(this);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        mIsVideoCaptureIntent = isVideoCaptureIntent();
        mQuickCapture = getIntent().getBooleanExtra(EXTRA_QUICK_CAPTURE, false);
        mRecordingTimeView = (TextView) findViewById(R.id.recording_time);
        mRecordingTimeRect = (RotateRecordingTime) findViewById(R.id.recording_time_rect);

        ViewGroup rootView = (ViewGroup) findViewById(R.id.video_camera);
        LayoutInflater inflater = this.getLayoutInflater();
        if (!mIsVideoCaptureIntent) {
            View controlBar = inflater.inflate(
                    R.layout.camera_control, rootView);
            mLastPictureButton =
                    (ImageView) controlBar.findViewById(R.id.review_thumbnail);
            mThumbController = new ThumbnailController(
                    getResources(), mLastPictureButton, mContentResolver);
            mLastPictureButton.setOnClickListener(this);
            mThumbController.loadData(ImageManager.getLastVideoThumbPath());
            mSwitcher = ((Switcher) findViewById(R.id.camera_switch));
            mSwitcher.setOnSwitchListener(this);
            mSwitcher.addTouchView(findViewById(R.id.camera_switch_set));
        } else {
            View controlBar = inflater.inflate(
                    R.layout.attach_camera_control, rootView);
            controlBar.findViewById(R.id.btn_cancel).setOnClickListener(this);
            ImageView retake =
                    (ImageView) controlBar.findViewById(R.id.btn_retake);
            retake.setOnClickListener(this);
            retake.setImageResource(R.drawable.btn_ic_review_retake_video);
            controlBar.findViewById(R.id.btn_play).setOnClickListener(this);
            controlBar.findViewById(R.id.btn_done).setOnClickListener(this);
        }

        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setImageResource(R.drawable.btn_ic_video_record);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.requestFocus();

        mOrientationListener = new MyOrientationEventListener(VideoCamera.this);

        // Make sure preview is started.
        try {
            startPreviewThread.join();
            if (mStartPreviewFail) {
                showCameraErrorAndFinish();
                return;
            }
        } catch (InterruptedException ex) {
            // ignore
        }

        // Initialize the HeadUpDiplay after startPreview(). We need mParameters
        // for HeadUpDisplay and it is initialized in that function.
        mHeadUpDisplay = new CamcorderHeadUpDisplay(this);
        mHeadUpDisplay.setListener(new MyHeadUpDisplayListener());
        initializeHeadUpDisplay();
        initializeZoom();
    }

    private void initializeZoom() {
        String zoomStr = mParameters.get("video-zoom-support");
        boolean zoomSupported = "true".equalsIgnoreCase(zoomStr);
        if (!zoomSupported) return;

        mZoomMax = mParameters.getMaxZoom();
        Log.v(TAG, "Max zoom=" + mZoomMax);
        String zoomSmooth = mParameters.get("video-smooth-zoom-support");
        mSmoothZoomSupported = "true".equalsIgnoreCase(zoomSmooth);
        Log.v(TAG, "Smooth zoom supported=" + mSmoothZoomSupported);

        mGestureDetector = new GestureDetector(this, new ZoomGestureListener());
        mCameraDevice.setZoomChangeListener(mZoomListener);
        mZoomButtons = new ZoomButtonsController(mVideoPreview);
        mZoomButtons.setAutoDismissed(true);
        mZoomButtons.setZoomSpeed(100);
        mZoomButtons.setOnZoomListener(
                new ZoomButtonsController.OnZoomListener() {
            public void onVisibilityChanged(boolean visible) {
                if (visible) {
                    updateZoomButtonsEnabled();
                }
            }

            public void onZoom(boolean zoomIn) {
                if (isZooming()) return;

                if (zoomIn) {
                    if (mZoomValue < mZoomMax) {
                        if (mSmoothZoomSupported) {
                            mCameraDevice.startSmoothZoom(mZoomValue + 1);
                            mZooming = true;
                        } else {
                            mParameters.setZoom(++mZoomValue);
                            if(mMediaRecorder != null){
                                String s = mParameters.flatten();
                                mMediaRecorder.setCameraParameters(s);
                            } else if(mCameraDevice != null) {
                                mCameraDevice.setParameters(mParameters);
                            }
                            updateZoomButtonsEnabled();
                        }
                    }
                } else {
                    if (mZoomValue > 0) {
                        if (mSmoothZoomSupported) {
                            mCameraDevice.startSmoothZoom(mZoomValue - 1);
                            mZooming = true;
                        } else {
                            mParameters.setZoom(--mZoomValue);
                            if(mMediaRecorder != null){
                                String s = mParameters.flatten();
                                mMediaRecorder.setCameraParameters(s);
                            } else if(mCameraDevice != null) {
                                mCameraDevice.setParameters(mParameters);
                            }
                            updateZoomButtonsEnabled();
                        }
                    }
                }
            }
        });
    }

    private boolean isZooming() {
        Log.v(TAG, "mZooming=" + mZooming);
        return mZooming;
    }

    private void updateZoomButtonsEnabled() {
        mZoomButtons.setZoomInEnabled(mZoomValue < mZoomMax);
        mZoomButtons.setZoomOutEnabled(mZoomValue > 0);
    }

    private class ZoomGestureListener extends
            GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            if (!mPausing && mPreviewing
                    && mZoomButtons != null) {
                mZoomButtons.setVisible(true);
            }
            return true;
        }

        @Override
        public boolean onDoubleTap(MotionEvent e) {
            if (mPausing || !mPreviewing
                    || mZoomButtons == null || isZooming()) {
                return false;
            }

            if (mZoomValue < mZoomMax) {
                while (mZoomValue < mZoomMax) {
                    mParameters.setZoom(++mZoomValue);
                    if(mMediaRecorder != null){
                        String s = mParameters.flatten();
                        mMediaRecorder.setCameraParameters(s);
                    } else if(mCameraDevice != null) {
                        mCameraDevice.setParameters(mParameters);
                    }
                    try {
                        Thread.sleep(5);
                    } catch (InterruptedException ex) {
                    }
                }
            } else {
                while (mZoomValue > 0) {
                    mParameters.setZoom(--mZoomValue);
                    if(mMediaRecorder != null){
                        String s = mParameters.flatten();
                        mMediaRecorder.setCameraParameters(s);
                    } else if(mCameraDevice != null) {
                        mCameraDevice.setParameters(mParameters);
                    }
                    try {
                        Thread.sleep(5);
                    } catch (InterruptedException ex) {
                    }
                }
            }
            updateZoomButtonsEnabled();
            return true;
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent m) {
        if (!super.dispatchTouchEvent(m) && mGestureDetector != null) {
            return mGestureDetector.onTouchEvent(m);
        }
        return true;
    }

    private final class ZoomListener
            implements android.hardware.Camera.OnZoomChangeListener {
        public void onZoomChange(
                int value, boolean stopped, android.hardware.Camera camera) {
            Log.v(TAG, "Zoom changed: value=" + value + ". stopped="+ stopped);
            mZoomValue = value;
            mParameters.setZoom(value);
            if (stopped) mZooming = false;
            updateZoomButtonsEnabled();
        }
    }

    private void overrideHudSettings(final String videoEncoder,
            final String audioEncoder, final String videoDuration, final String videoSize) {
            if (mHeadUpDisplay != null && mGLRootView != null) {
            mGLRootView.queueEvent(new Runnable() {
            public void run() {
                mHeadUpDisplay.overrideSettings(
                       CameraSettings.KEY_VIDEO_ENCODER, videoEncoder);
                mHeadUpDisplay.overrideSettings(
                        CameraSettings.KEY_AUDIO_ENCODER, audioEncoder);
                mHeadUpDisplay.overrideSettings(
                        CameraSettings.KEY_VIDEO_DURATION, videoDuration);
                mHeadUpDisplay.overrideSettings(
                        CameraSettings.KEY_VIDEO_SIZE, videoSize);
            }});
          }
    }

    private void updateProfileInHud() {
        String quality = mPreferences.getString(
                CameraSettings.KEY_VIDEO_QUALITY,
                CameraSettings.DEFAULT_VIDEO_QUALITY_VALUE);
        if(!"custom".equalsIgnoreCase(quality)){
            String videoEncoder = VIDEO_ENCODER_TABLE.getKey(mProfile.videoCodec);
            String audioEncoder = AUDIO_ENCODER_TABLE.getKey(mProfile.audioCodec);
            String videoSize = mProfile.videoFrameWidth + "x" + mProfile.videoFrameHeight;
            int videoDuration = mMaxVideoDurationInMs / 1000;
            if(videoDuration >= 60)
                videoDuration = videoDuration / 60;
            else
                videoDuration = -1;
            overrideHudSettings(videoEncoder, audioEncoder, String.valueOf(videoDuration), videoSize);
       } else {
            overrideHudSettings(null, null, null, null);
       }
    }

    private void changeHeadUpDisplayState() {
        // If the camera resumes behind the lock screen, the orientation
        // will be portrait. That causes OOM when we try to allocation GPU
        // memory for the GLSurfaceView again when the orientation changes. So,
        // we delayed initialization of HeadUpDisplay until the orientation
        // becomes landscape.
        Configuration config = getResources().getConfiguration();
        if (config.orientation == Configuration.ORIENTATION_LANDSCAPE
                && !mPausing){
            if(mGLRootView == null) 
                attachHeadUpDisplay();        
        } else if (mGLRootView != null && mPausing) {
            detachHeadUpDisplay();
        }
    }

    private void initializeHeadUpDisplay() {
        CameraSettings settings = new CameraSettings(this, mParameters,
                CameraHolder.instance().getCameraInfo());

        PreferenceGroup group =
                settings.getPreferenceGroup(R.xml.video_preferences);
        if (mIsVideoCaptureIntent) {
            group = filterPreferenceScreenByIntent(group);
        }
        mHeadUpDisplay.initialize(this, group, mOrientationCompensation);
    }

    private void attachHeadUpDisplay() {
        mHeadUpDisplay.setOrientation(mOrientationCompensation);
        FrameLayout frame = (FrameLayout) findViewById(R.id.frame);
        mGLRootView = new GLRootView(this);
        frame.addView(mGLRootView);
        mGLRootView.setContentPane(mHeadUpDisplay);
        updateProfileInHud();
    }

    private void detachHeadUpDisplay() {
        mHeadUpDisplay.collapse();
        ((ViewGroup) mGLRootView.getParent()).removeView(mGLRootView);
        mGLRootView = null;
    }

    public static int roundOrientation(int orientation) {
        return ((orientation + 45) / 90 * 90) % 360;
    }

    private class MyOrientationEventListener
            extends OrientationEventListener {
        public MyOrientationEventListener(Context context) {
            super(context);
        }

        @Override
        public void onOrientationChanged(int orientation) {
            if (mMediaRecorderRecording) return;
            // We keep the last known orientation. So if the user first orient
            // the camera then point the camera to floor or sky, we still have
            // the correct orientation.
            if (orientation == ORIENTATION_UNKNOWN) return;
            mOrientation = roundOrientation(orientation);
            // When the screen is unlocked, display rotation may change. Always
            // calculate the up-to-date orientationCompensation.
            int orientationCompensation = mOrientation
                    + Util.getDisplayRotation(VideoCamera.this);
            if (mOrientationCompensation != orientationCompensation) {
                mOrientationCompensation = orientationCompensation;
                if (!mIsVideoCaptureIntent) {
                    setOrientationIndicator(mOrientationCompensation);
                }
                mHeadUpDisplay.setOrientation(mOrientationCompensation);
            }
        }
    }

    private void setOrientationIndicator(int degree) {
        ((RotateImageView) findViewById(
                R.id.review_thumbnail)).setDegree(degree);
        ((RotateImageView) findViewById(
                R.id.camera_switch_icon)).setDegree(degree);
        ((RotateImageView) findViewById(
                R.id.video_switch_icon)).setDegree(degree);
    }

    @Override
    protected void onStart() {
        super.onStart();
        if (!mIsVideoCaptureIntent) {
            mSwitcher.setSwitch(SWITCH_VIDEO);
        }
    }

    private void startPlayVideoActivity() {
        Intent intent = new Intent(Intent.ACTION_VIEW, mCurrentVideoUri);
        try {
            startActivity(intent);
        } catch (android.content.ActivityNotFoundException ex) {
            Log.e(TAG, "Couldn't view video " + mCurrentVideoUri, ex);
        }
    }

    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_retake:
                deleteCurrentVideo();
                hideAlert();
                break;
            case R.id.btn_play:
                startPlayVideoActivity();
                break;
            case R.id.btn_done:
                doReturnToCaller(true);
                break;
            case R.id.btn_cancel:
                stopVideoRecordingAndReturn(false);
                break;
            case R.id.review_thumbnail:
                if (!mMediaRecorderRecording) viewLastVideo();
                break;
        }
    }

    private final class JpegPictureCallback implements PictureCallback {
        Location mLocation;
        private Uri mLastContentUri;

        public JpegPictureCallback(Location loc) {
            mLocation = loc;
        }

        public void onPictureTaken(
                final byte [] jpegData, final android.media.MediaRecorder mediarecorder) {

            mJpegPictureCallbackTime = System.currentTimeMillis();
            storeImage(jpegData, mediarecorder, mLocation);
            Log.v(TAG, "Liveshot done");
            mTakingPicture = false;
        }

        private int storeImage(byte[] data, Location loc) {
            try {
                long dateTaken = System.currentTimeMillis();
                String ext = ".jpg";
                //String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);
                String store_location = ImageManager.CAMERA_IMAGE_BUCKET_NAME;

                String title = createImageName(dateTaken);
                String filename = title + ext;
                int[] degree = new int[1];
                mLastContentUri = ImageManager.addImage(
                        mContentResolver,
                        title,
                        dateTaken,
                        loc, // location from gps/network
                        store_location, filename,
                        null, data,
                        degree);
                return degree[0];
            } catch (Exception ex) {
                Log.e(TAG, "Exception while compressing image.", ex);
                return 0;
            }
        }

        public void storeImage(final byte[] data,
                android.media.MediaRecorder mediarecorder, Location loc) {

                int degree = storeImage(data, loc);
                sendBroadcast(new Intent(
                        "com.android.camera.NEW_PICTURE", mLastContentUri));
                setLastPictureThumb(data, degree,
                        getLastCaptureUri());
                mThumbController.updateDisplayIfNeeded();
        }

        public Uri getLastCaptureUri() {
            return mLastContentUri;
        }
    }

    private void setLastPictureThumb(byte[] data, int degree, Uri uri) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inSampleSize = 16;
        if(mThumbController != null && mImageWidth > 0 && mImageHeight > 0){
            int miniThumbHeight = mThumbController.getThumbnailHeight();
            if(miniThumbHeight > 0){
                options.inSampleSize = mImageHeight/miniThumbHeight;
            }
        }
        Bitmap lastPictureThumb =
                BitmapFactory.decodeByteArray(data, 0, data.length, options);
        lastPictureThumb = Util.rotate(lastPictureThumb, degree);
        mThumbController.setData(uri, lastPictureThumb);
    }

    public void onShutterButtonFocus(ShutterButton button, boolean pressed) {
        // Do nothing (everything happens in onShutterButtonClick).
    }

    private void onStopVideoRecording(boolean valid) {
        if (mIsVideoCaptureIntent) {
            if (mQuickCapture) {
                stopVideoRecordingAndReturn(valid);
            } else {
                stopVideoRecordingAndShowAlert();
            }
        } else {
            stopVideoRecordingAndGetThumbnail();
        }
    }

    public void onShutterButtonClick(ShutterButton button) {
        switch (button.getId()) {
            case R.id.shutter_button:
                if (mHeadUpDisplay == null || mHeadUpDisplay.collapse()) return;

                if (mMediaRecorderRecording) {
                    onStopVideoRecording(true);
                } else {
                    startVideoRecording();
                }
                mShutterButton.setEnabled(false);
                mHandler.sendEmptyMessageDelayed(
                        ENABLE_SHUTTER_BUTTON, SHUTTER_BUTTON_TIMEOUT);
                break;
        }
    }

    private class LocationListener
            implements android.location.LocationListener {
       Location mLastLocation;
       boolean mValid = false;
       String mProvider;

       public LocationListener(String provider) {
           mProvider = provider;
           mLastLocation = new Location(mProvider);
       }

       public void onLocationChanged(Location newLocation) {
           if (newLocation.getLatitude() == 0.0
                  && newLocation.getLongitude() == 0.0) {
               // Hack to filter out 0.0,0.0 locations
               return;
           }
           mLastLocation.set(newLocation);
           mValid = true;
       }

       public void onProviderEnabled(String provider) {
       }

       public void onProviderDisabled(String provider) {
           mValid = false;
       }

       public void onStatusChanged(
               String provider, int status, Bundle extras) {
           switch(status) {
              case LocationProvider.OUT_OF_SERVICE:
              case LocationProvider.TEMPORARILY_UNAVAILABLE: {
                  mValid = false;
                  break;
              }
          }
       }

       public Location current() {
           return mValid ? mLastLocation : null;
       }
   }

   LocationListener [] mLocationListeners = new LocationListener[] {
       new LocationListener(LocationManager.GPS_PROVIDER),
       new LocationListener(LocationManager.NETWORK_PROVIDER)
   };

   private void startReceivingLocationUpdates() {
       if (mLocationManager != null) {
           try {
               mLocationManager.requestLocationUpdates(
                       LocationManager.NETWORK_PROVIDER,
                       1000,
                       0F,
                       mLocationListeners[1]);
           } catch (java.lang.SecurityException ex) {
               Log.i(TAG, "fail to request location update, ignore", ex);
           } catch (IllegalArgumentException ex) {
               Log.d(TAG, "provider does not exist " + ex.getMessage());
           }
           try {
               mLocationManager.requestLocationUpdates(
                       LocationManager.GPS_PROVIDER,
                       1000,
                       0F,
                       mLocationListeners[0]);
           } catch (java.lang.SecurityException ex) {
               Log.i(TAG, "fail to request location update, ignore", ex);
           } catch (IllegalArgumentException ex) {
               Log.d(TAG, "provider does not exist " + ex.getMessage());
           }
       }
   }

   private void stopReceivingLocationUpdates() {
      if (mLocationManager != null) {
          for (int i = 0; i < mLocationListeners.length; i++) {
              try {
                  mLocationManager.removeUpdates(mLocationListeners[i]);
              } catch (Exception ex) {
                  Log.i(TAG, "fail to remove location listners, ignore", ex);
              }
          }
      }
   }

    private Location getCurrentLocation() {
       // go in best to worst order
       for (int i = 0; i < mLocationListeners.length; i++) {
           Location l = mLocationListeners[i].current();
           if (l != null) return l;
       }
       return null;
    }

    private OnScreenHint mStorageHint;

    private void updateAndShowStorageHint(boolean mayHaveSd) {
        mStorageStatus = getStorageStatus(mayHaveSd);
        showStorageHint();
    }

    private void showStorageHint() {
        String errorMessage = null;
        switch (mStorageStatus) {
            case STORAGE_STATUS_NONE:
                errorMessage = getString(R.string.no_storage);
                break;
            case STORAGE_STATUS_LOW:
                errorMessage = getString(R.string.spaceIsLow_content);
                break;
            case STORAGE_STATUS_FAIL:
                errorMessage = getString(R.string.access_sd_fail);
                break;
        }
        if (errorMessage != null) {
            if (mStorageHint == null) {
                mStorageHint = OnScreenHint.makeText(this, errorMessage);
            } else {
                mStorageHint.setText(errorMessage);
            }
            mStorageHint.show();
        } else if (mStorageHint != null) {
            mStorageHint.cancel();
            mStorageHint = null;
        }
    }

    private int getStorageStatus(boolean mayHaveSd) {
        long remaining = mayHaveSd ? getAvailableStorage() : NO_STORAGE_ERROR;
        if (remaining == NO_STORAGE_ERROR) {
            return STORAGE_STATUS_NONE;
        } else if (remaining == CANNOT_STAT_ERROR) {
            return STORAGE_STATUS_FAIL;
        }
        return remaining < LOW_STORAGE_THRESHOLD
                ? STORAGE_STATUS_LOW
                : STORAGE_STATUS_OK;
    }

    private void readVideoPreferences() {
        mRecordLocation = RecordLocationPreference.get(
                mPreferences, getContentResolver());
        String quality = mPreferences.getString(
                CameraSettings.KEY_VIDEO_QUALITY,
                CameraSettings.DEFAULT_VIDEO_QUALITY_VALUE);

        if(!"custom".equalsIgnoreCase(quality)){

            boolean videoQualityHigh = CameraSettings.getVideoQuality(quality);

            // Set video quality.
            Intent intent = getIntent();
            if (intent.hasExtra(MediaStore.EXTRA_VIDEO_QUALITY)) {
                int extraVideoQuality =
                        intent.getIntExtra(MediaStore.EXTRA_VIDEO_QUALITY, 0);
                videoQualityHigh = (extraVideoQuality > 0);
            }

            // Set video duration limit. The limit is read from the preference,
            // unless it is specified in the intent.
            if (intent.hasExtra(MediaStore.EXTRA_DURATION_LIMIT)) {
                int seconds =
                        intent.getIntExtra(MediaStore.EXTRA_DURATION_LIMIT, 0);
                mMaxVideoDurationInMs = 1000 * seconds;
            } else {
                mMaxVideoDurationInMs =
                        CameraSettings.getVidoeDurationInMillis(quality);
            }
            mProfile = CamcorderProfile.get(videoQualityHigh
                    ? CamcorderProfile.QUALITY_HIGH
                    : CamcorderProfile.QUALITY_LOW);
        } else {
            mProfile = null;

            String videoEncoder = mPreferences.getString(
                        CameraSettings.KEY_VIDEO_ENCODER,
                        getString(R.string.pref_camera_videoencoder_default));

            String audioEncoder = mPreferences.getString(
                        CameraSettings.KEY_AUDIO_ENCODER,
                        getString(R.string.pref_camera_audioencoder_default));

            String videoQuality = mPreferences.getString(
                     CameraSettings.KEY_VIDEO_SIZE,
                        getString(R.string.pref_camera_videosize_default));

            StringTokenizer tokenizer = null;
            if(videoQuality != null &&
                        ( tokenizer = new StringTokenizer(
                              videoQuality ,"x")).countTokens() == 2){
                mVideoWidth = Integer.parseInt(tokenizer.nextToken());
                mVideoHeight = Integer.parseInt(tokenizer.nextToken());
                mVideoBitrate = VIDEOQUALITY_BITRATE_TABLE.get(videoQuality);
            } else {
                mVideoWidth = 320;
                mVideoHeight = 240;
                mVideoBitrate = 320000;
            }

            mVideoEncoder = VIDEO_ENCODER_TABLE.get(videoEncoder);
            mAudioEncoder = AUDIO_ENCODER_TABLE.get(audioEncoder);

            String minutesStr = mPreferences.getString(CameraSettings.KEY_VIDEO_DURATION,
                                    getString(R.string.pref_camera_video_duration_default));

            int minutes = -1;
            try{
                minutes = Integer.parseInt(minutesStr);
            }catch(NumberFormatException npe){
                // use default value continue
                minutes = Integer.parseInt(getString(R.string.pref_camera_video_duration_default));
            }

            if (minutes == -1) {
                // This is a special case: the value -1 means we want to use the
                // device-dependent duration for MMS messages. The value is
                // represented in seconds.

                mMaxVideoDurationInMs =
                       CameraSettings.getVidoeDurationInMillis("mms");

            } else {
                // 1 minute = 60000ms
                mMaxVideoDurationInMs = 60000 * minutes;
            }
        }
        updateProfileInHud();
    }

    private void resizeForPreviewAspectRatio() {
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        int degrees = Util.getDisplayRotation(this);
        // Since ZSL mode is not supported in Camcorder, setting
        // mode as NON ZSL mode
        int mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_NONZSL;
        if(mProfile != null){
            // If Camera orientation and display Orientation is not aligned,
            // FrameLayout's Aspect Ration needs to be rotated
            if(((info.orientation - degrees + 360)%180) == 90) {
                mPreviewFrameLayout.setAspectRatio(
                    ((double) mProfile.videoFrameHeight / mProfile.videoFrameWidth),
                    mSnapshotMode);
            } else {
                mPreviewFrameLayout.setAspectRatio(
                    ((double) mProfile.videoFrameWidth / mProfile.videoFrameHeight),
                    mSnapshotMode);
            }
        } else {
            // If Camera orientation and display Orientation is not aligned,
            // FrameLayout's Aspect Ration needs to be rotated
            if(((info.orientation - degrees + 360)%180) == 90) {
                mPreviewFrameLayout.setAspectRatio( (((double) mVideoHeight) / mVideoWidth) ,
                mSnapshotMode);
            } else {
                mPreviewFrameLayout.setAspectRatio( (((double) mVideoWidth) / mVideoHeight),
                mSnapshotMode);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPausing = false;
        mZoomValue = 0;


        // Start orientation listener as soon as possible because it takes
        // some time to get first orientation.
        mOrientationListener.enable();
        mVideoPreview.setVisibility(View.VISIBLE);
        readVideoPreferences();
        resizeForPreviewAspectRatio();
        if (!mPreviewing && !mStartPreviewFail) {
            if (!restartPreview()) return;
        }
        keepScreenOnAwhile();

        /* Postpone the non-critical functionality till after the
         * activity is displayed, so that the camera frames are
         * displayed sooner on the screen.*/
        mHandler.sendEmptyMessageDelayed(DELAYED_ONRESUME_FUNCTION, 200);
    }

    private void delayedOnResume() {

        // install an intent filter to receive SD card related events.
        IntentFilter intentFilter =
                new IntentFilter(Intent.ACTION_MEDIA_MOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_EJECT);
        intentFilter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        intentFilter.addDataScheme("file");
        mReceiver = new MyBroadcastReceiver();
        registerReceiver(mReceiver, intentFilter);
        mStorageStatus = getStorageStatus(true);

        mHandler.postDelayed(new Runnable() {
            public void run() {
                showStorageHint();
            }
        }, 200);

        changeHeadUpDisplayState();

        updateThumbnailButton();
    }

    private void setPreviewDisplay(SurfaceHolder holder) {
        try {
            mCameraDevice.setPreviewDisplay(holder);
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("setPreviewDisplay failed", ex);
        }
    }

    private void startPreview() throws CameraHardwareException {
        Log.v(TAG, "startPreview");

        ensureCameraDevice();

        if (mPreviewing == true) {
            mCameraDevice.stopPreview();
            mPreviewing = false;
        }
        setPreviewDisplay(mSurfaceHolder);
        Util.setCameraDisplayOrientation(this, mCameraId, mCameraDevice);
        setCameraParameters();

        try {
            mCameraDevice.startPreview();
            mPreviewing = true;
            mZooming = false;
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("startPreview failed", ex);
        }

        mParameters = mCameraDevice.getParameters();
        mZoomMax = mParameters.getMaxZoom();

    }

    private void closeCamera() {
        Log.v(TAG, "closeCamera");
        if (mCameraDevice == null) {
            Log.d(TAG, "already stopped.");
            return;
        }
        // If we don't lock the camera, release() will fail.
        mCameraDevice.lock();
        CameraHolder.instance().release();
        mCameraDevice.setZoomChangeListener(null);
        mCameraDevice = null;
        mPreviewing = false;
    }

    private void ensureCameraDevice() throws CameraHardwareException {
        if (mCameraDevice == null) {
            // If the activity is paused and resumed, camera device has been
            // released and we need to open the camera.
            mCameraDevice = CameraHolder.instance().open(mCameraId, mCameraMode);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        mPausing = true;
        changeHeadUpDisplayState();

        // Hide the preview now. Otherwise, the preview may be rotated during
        // onPause and it is annoying to users.
        mVideoPreview.setVisibility(View.INVISIBLE);

        // This is similar to what mShutterButton.performClick() does,
        // but not quite the same.
        if (mMediaRecorderRecording) {
            if (mIsVideoCaptureIntent) {
                stopVideoRecording();
                showAlert();
            } else {
                stopVideoRecordingAndGetThumbnail();
            }
        } else {
            stopVideoRecording();
        }
        //Nullify the record-size parameter, so that it won't be
        //taken into effect when switching to camera application
        mCameraDevice.lock();
        mParameters = mCameraDevice.getParameters();
        mParameters.set("record-size", "");
        mCameraDevice.setParameters(mParameters);
        mCameraDevice.unlock();
        closeCamera();

        if (mReceiver != null) {
            unregisterReceiver(mReceiver);
            mReceiver = null;
        }
        resetScreenOn();

        if (!mIsVideoCaptureIntent) {
            mThumbController.storeData(ImageManager.getLastVideoThumbPath());
        }

        if (mStorageHint != null) {
            mStorageHint.cancel();
            mStorageHint = null;
        }
        if (mZoomButtons != null) {
            mZoomButtons.setVisible(false);
        }


        mOrientationListener.disable();
    }

    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
        if (!mMediaRecorderRecording) keepScreenOnAwhile();
    }

    @Override
    public void onBackPressed() {
        if (mPausing) return;
        if (mMediaRecorderRecording) {
            onStopVideoRecording(false);
        } else if (mHeadUpDisplay == null || !mHeadUpDisplay.collapse()) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Do not handle any key if the activity is paused.
        if (mPausing) {
            return true;
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_CAMERA:
                if (event.getRepeatCount() == 0) {
                    mShutterButton.performClick();
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                if (event.getRepeatCount() == 0) {
                    mShutterButton.performClick();
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_MENU:
                if (mMediaRecorderRecording) {
                    onStopVideoRecording(true);
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_6:
                Log.v(TAG,"This is key event for keycode_6");
                if(mMediaRecorderRecording && !mTakingPicture) {
                    takeLiveSnapshot();
                    return true;
                }
                else if(mTakingPicture)
                    Log.i(TAG,"Liveshot in progress");
                break;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_CAMERA:
                mShutterButton.setPressed(false);
                return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    private void takeLiveSnapshot() {
       Log.v(TAG, "takeLiveSnapshot");

       Location loc = null;
       if (mRecordLocation) {
           loc = getCurrentLocation();
       }

       if (loc != null) {
           double lat = loc.getLatitude();
           double lon = loc.getLongitude();
           boolean hasLatLon = (lat != 0.0d) || (lon != 0.0d);
           Log.v(TAG, "in capture lat = " + lat + " lon = " + lon);

           if (hasLatLon) {
               String latRef= "N";
               String lonRef= "E";
               if(lat < 0) {
                   latRef = "S";
                }
                if (lon < 0) {
                    lonRef = "W";
                }
                mParameters.setGpsLatitudeRef(latRef);
                mParameters.setGpsLatitude(lat);
                mParameters.setGpsLongitudeRef(lonRef);
                mParameters.setGpsLongitude(lon);
                mParameters.setGpsProcessingMethod(loc.getProvider().toUpperCase());
                if (loc.hasAltitude()) {
                    Double altitude = loc.getAltitude();
                    Double altitudeX1000 = altitude * 1000;
                    long altitudeDividend = altitudeX1000.longValue();
                    if(altitudeDividend < 0) {
                        altitudeDividend *= -1;
                        mParameters.setGpsAltitudeRef(1);
                    }
                    else
                        mParameters.setGpsAltitudeRef(0);
                    mParameters.setGpsAltitude(altitude);
                } else {
                    // for NETWORK_PROVIDER location provider, we may have
                    // no altitude information, but the driver needs it, so
                    // we fake one.
                    mParameters.setGpsAltitude(0);
                }
                if (loc.getTime() != 0) {
                    // Location.getTime() is UTC in milliseconds.
                    // gps-timestamp is UTC in seconds.
                    long utcTimeSeconds = loc.getTime() / 1000;
                    mParameters.setGpsTimestamp(utcTimeSeconds);
                }
            } else {
                loc = null;
            }
        }
        long dateTaken = System.currentTimeMillis();
        if (dateTaken != 0) {
            String datetime = DateFormat.format("yyyy:MM:dd kk:mm:ss", dateTaken).toString();
            Log.v(TAG,"datetime : " +  datetime);
            mParameters.setExifDateTime(datetime);
        }
        if(mProfile != null) {
            mImageWidth = mProfile.videoFrameWidth;
            mImageHeight = mProfile.videoFrameHeight;
        }
        else {
            mImageWidth = mVideoWidth;
            mImageHeight = mVideoWidth;
        }
        if(mMediaRecorderRecording && !mTakingPicture) {
            String s = mParameters.flatten();
            mMediaRecorder.setCameraParameters(s);

            try {
                mTakingPicture = true;
                boolean result = mMediaRecorder.takeLiveSnapshot(new JpegPictureCallback(loc));
                if(result)
                    Log.v(TAG,"Liveshot triggered");
                else {
                    mTakingPicture = false;
                    Log.v(TAG,"Liveshot trigger failed");
                }
            } catch (RuntimeException e) {
                Log.e(TAG, "Could not take LiveSnapshot ", e);
                mTakingPicture = false;
                return;
            }
        }
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        // Make sure we have a surface in the holder before proceeding.
        if (holder.getSurface() == null) {
            Log.d(TAG, "holder.getSurface() == null");
            return;
        }

        mSurfaceHolder = holder;

        if (mPausing) {
            // We're pausing, the screen is off and we already stopped
            // video recording. We don't want to start the camera again
            // in this case in order to conserve power.
            // The fact that surfaceChanged is called _after_ an onPause appears
            // to be legitimate since in that case the lockscreen always returns
            // to portrait orientation possibly triggering the notification.
            return;
        }

        // The mCameraDevice will be null if it is fail to connect to the
        // camera hardware. In this case we will show a dialog and then
        // finish the activity, so it's OK to ignore it.
        if (mCameraDevice == null) return;

        // Set preview display if the surface is being created. Preview was
        // already started.
        if (holder.isCreating()) {
            setPreviewDisplay(holder);
        } else {
            stopVideoRecording();
            // restart preview if not already previewing.
            if (!mPreviewing && !mStartPreviewFail)
                restartPreview();

        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        mSurfaceHolder = null;
    }

    private void gotoGallery() {
        MenuHelper.gotoCameraVideoGallery(this);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);

        if (mIsVideoCaptureIntent) {
            // No options menu for attach mode.
            return false;
        } else {
            addBaseMenuItems(menu);
        }
        return true;
    }

    private boolean isVideoCaptureIntent() {
        String action = getIntent().getAction();
        return (MediaStore.ACTION_VIDEO_CAPTURE.equals(action));
    }

    private void doReturnToCaller(boolean valid) {
        Intent resultIntent = new Intent();
        int resultCode;
        if (valid) {
            resultCode = RESULT_OK;
            resultIntent.setData(mCurrentVideoUri);
        } else {
            resultCode = RESULT_CANCELED;
        }
        setResult(resultCode, resultIntent);
        finish();
    }

    /**
     * Returns
     *
     * @return number of bytes available, or an ERROR code.
     */
    private static long getAvailableStorage() {
        try {
            if (!ImageManager.hasStorage()) {
                return NO_STORAGE_ERROR;
            } else {
                String storageDirectory =
                        Environment.getExternalStorageDirectory().toString();
                StatFs stat = new StatFs(storageDirectory);
                return (long) stat.getAvailableBlocks()
                        * (long) stat.getBlockSize();
            }
        } catch (Exception ex) {
            // if we can't stat the filesystem then we don't know how many
            // free bytes exist. It might be zero but just leave it
            // blank since we really don't know.
            Log.e(TAG, "Fail to access sdcard", ex);
            return CANNOT_STAT_ERROR;
        }
    }

    private void cleanupEmptyFile() {
        if (mVideoFilename != null) {
            File f = new File(mVideoFilename);
            if (f.length() == 0 && f.delete()) {
                Log.v(TAG, "Empty video file deleted: " + mVideoFilename);
                mVideoFilename = null;
            }
        }
    }

    private android.hardware.Camera mCameraDevice;

    // Prepares media recorder.
    private void initializeRecorder() {
        Log.v(TAG, "initializeRecorder");
        // If the mCameraDevice is null, then this activity is going to finish
        if (mCameraDevice == null) return;

        if (mSurfaceHolder == null) {
            Log.v(TAG, "Surface holder is null. Wait for surface changed.");
            return;
        }

        Intent intent = getIntent();
        Bundle myExtras = intent.getExtras();
        mUnsupportedResolution = false;

        if ( mVideoEncoder == MediaRecorder.VideoEncoder.H263 ) {
            if (  mVideoWidth >= 1280 && mVideoHeight >= 720 ) {
                mUnsupportedResolution = true;
                Toast.makeText(VideoCamera.this, R.string.error_app_unsupported,
                Toast.LENGTH_LONG).show();
                return;
            }
        }
        long requestedSizeLimit = 0;
        if (mIsVideoCaptureIntent && myExtras != null) {
            Uri saveUri = (Uri) myExtras.getParcelable(MediaStore.EXTRA_OUTPUT);
            if (saveUri != null) {
                try {
                    mVideoFileDescriptor =
                            mContentResolver.openFileDescriptor(saveUri, "rw");
                    mCurrentVideoUri = saveUri;
                } catch (java.io.FileNotFoundException ex) {
                    // invalid uri
                    Log.e(TAG, ex.toString());
                }
            }
            requestedSizeLimit = myExtras.getLong(MediaStore.EXTRA_SIZE_LIMIT);
        }
        mMediaRecorder = new MediaRecorder();

        // Unlock the camera object before passing it to media recorder.
        mCameraDevice.unlock();
        mMediaRecorder.setCamera(mCameraDevice);
        String hfr = mParameters.getVideoHighFrameRate();
        if((hfr == null) || ("off".equals(hfr)))
            mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.CAMCORDER);

        mMediaRecorder.setVideoSource(MediaRecorder.VideoSource.CAMERA);
        if(mProfile != null){
           try {
            mMediaRecorder.setProfile(mProfile);
            } catch (RuntimeException exception) {
              releaseMediaRecorder();
              return;
            }
            mMediaRecorder.setMaxDuration(mMaxVideoDurationInMs);
        }
        else {

            mMediaRecorder.setOutputFormat(OUTPUT_FORMAT_TABLE.get(mOutputFormat));
            mMediaRecorder.setMaxDuration(mMaxVideoDurationInMs);
            mMediaRecorder.setVideoFrameRate(mVideoFps);
            mMediaRecorder.setVideoSize(mVideoWidth, mVideoHeight);
            int Bitrate_multiplier = 1;
            if(hfr != null) {
                if ("60".equals(hfr))
                    Bitrate_multiplier = 2;
                else if ("90".equals(hfr))
                    Bitrate_multiplier = 3;
                else if ("120".equals(hfr))
                    Bitrate_multiplier = 4;
            }
            mMediaRecorder.setVideoEncodingBitRate(mVideoBitrate*Bitrate_multiplier);
            mMediaRecorder.setVideoEncoder(mVideoEncoder);
            if((hfr == null)|| ("off".equals(hfr))) {
                try {
                mMediaRecorder.setAudioEncoder(mAudioEncoder);
                } catch (RuntimeException exception) {
                  releaseMediaRecorder();
                  return;
                }
            }
            Log.v(TAG, "Custom format " +
                       " output format " + OUTPUT_FORMAT_TABLE.get(mOutputFormat) +
                       " Video Fps " +  mVideoFps +
                       " Video Bitrate " + mVideoBitrate +
                       " Max Video Duration "  + mMaxVideoDurationInMs+
                       " Video Encoder "  + mVideoEncoder +
                       " Audio Encoder "  + mAudioEncoder);
        }

        updateAndShowStorageHint(true);
        // Set output file.
        if (mStorageStatus != STORAGE_STATUS_OK) {
            mMediaRecorder.setOutputFile("/dev/null");
        } else {
            // Try Uri in the intent first. If it doesn't exist, use our own
            // instead.
            if (mVideoFileDescriptor != null) {
                mMediaRecorder.setOutputFile(mVideoFileDescriptor.getFileDescriptor());
                try {
                    mVideoFileDescriptor.close();
                } catch (IOException e) {
                    Log.e(TAG, "Fail to close fd", e);
                }
            } else {
                createVideoPath();
                mMediaRecorder.setOutputFile(mVideoFilename);
            }
        }

        mMediaRecorder.setPreviewDisplay(mSurfaceHolder.getSurface());
        mMediaRecorder.setOnInfoListener(this);

        // Set maximum file size.
        // remaining >= LOW_STORAGE_THRESHOLD at this point, reserve a quarter
        // of that to make it more likely that recording can complete
        // successfully.
        long maxFileSize = getAvailableStorage() - LOW_STORAGE_THRESHOLD / 4;
        if (requestedSizeLimit > 0 && requestedSizeLimit < maxFileSize) {
            maxFileSize = requestedSizeLimit;
        }

        try {
            mMediaRecorder.setMaxFileSize(maxFileSize);
        } catch (RuntimeException exception) {
            // We are going to ignore failure of setMaxFileSize here, as
            // a) The composer selected may simply not support it, or
            // b) The underlying media framework may not handle 64-bit range
            // on the size restriction.
        }

        // See android.hardware.Camera.Parameters.setRotation for
        // documentation.
        int rotation = 0;
        if (mOrientation != OrientationEventListener.ORIENTATION_UNKNOWN) {
            CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
            if (info.facing == CameraInfo.CAMERA_FACING_FRONT) {
                rotation = (info.orientation - mOrientation + 360) % 360;
            } else {  // back-facing camera
                rotation = (info.orientation + mOrientation) % 360;
            }
        }
        mMediaRecorder.setOrientationHint(rotation);
        mOrientationHint = rotation;

        // If store location enabled, read location details from location
        // manager
        Log.v(TAG, "Store latitude and longitude information");

        Location loc = null;
        if (mRecordLocation) {
            loc = getCurrentLocation();
        }

        if (loc != null) {
            double lat = loc.getLatitude();
            double lon = loc.getLongitude();
            boolean hasLatLon = (lat != 0.0d) || (lon != 0.0d);

            Log.e(TAG, "in capture lat = " + lat + " lon = " + lon);

            if (hasLatLon) {
                //convert floating to fixed point numbers
                int lat_fp = (int) (lat * (1 << 16));
                int lon_fp = (int) (lon * (1 << 16));

                Log.e(TAG, "fixed point in capture lat = " + lat_fp + " lon = " + lon_fp);
                Log.e(TAG, "int value in capture lat = " + (lat_fp >> 16) + " lon = " + (lon_fp >> 16));

                mMediaRecorder.setParamLatitude(lat_fp);
                mMediaRecorder.setParamLongitude(lon_fp);
             }
        }

        try {
            mMediaRecorder.prepare();
        } catch (IOException e) {
            Log.e(TAG, "prepare failed for " + mVideoFilename, e);
            releaseMediaRecorder();
            throw new RuntimeException(e);
        }

        mMediaRecorder.setOnErrorListener(this);
        mMediaRecorder.setOnInfoListener(this);
    }

    private void releaseMediaRecorder() {
        Log.v(TAG, "Releasing media recorder.");
        if (mMediaRecorder != null) {
            cleanupEmptyFile();
            mMediaRecorder.reset();
            mMediaRecorder.release();
            mMediaRecorder = null;
        }
        // Take back the camera object control from media recorder. Camera
        // device may be null if the activity is paused.
        if (mCameraDevice != null) mCameraDevice.lock();
    }

    private void createVideoPath() {
        long dateTaken = System.currentTimeMillis();
        String title = createName(dateTaken);
        String filename = title + ".3gp"; // Used when emailing.
        String cameraDirPath = ImageManager.CAMERA_IMAGE_BUCKET_NAME;
        String filePath = cameraDirPath + "/" + filename;
        File cameraDir = new File(cameraDirPath);
        cameraDir.mkdirs();
        ContentValues values = new ContentValues(7);
        values.put(Video.Media.TITLE, title);
        values.put(Video.Media.DISPLAY_NAME, filename);
        values.put(Video.Media.DATE_TAKEN, dateTaken);
        values.put(Video.Media.MIME_TYPE, "video/3gpp");
        values.put(Video.Media.DATA, filePath);
        mVideoFilename = filePath;
        Log.v(TAG, "Current camera video filename: " + mVideoFilename);
        mCurrentVideoValues = values;
    }

    private void registerVideo() {
        if (mVideoFileDescriptor == null) {
            Uri videoTable = Uri.parse("content://media/external/video/media");
            mCurrentVideoValues.put(Video.Media.SIZE,
                    new File(mCurrentVideoFilename).length());
            try {
                mCurrentVideoUri = mContentResolver.insert(videoTable,
                        mCurrentVideoValues);
            } catch (Exception e) {
                // We failed to insert into the database. This can happen if
                // the SD card is unmounted.
                mCurrentVideoUri = null;
                mCurrentVideoFilename = null;
            } finally {
                Log.v(TAG, "Current video URI: " + mCurrentVideoUri);
            }
        }
        mCurrentVideoValues = null;
    }

    private void deleteCurrentVideo() {
        if (mCurrentVideoFilename != null) {
            deleteVideoFile(mCurrentVideoFilename);
            mCurrentVideoFilename = null;
        }
        if (mCurrentVideoUri != null) {
            mContentResolver.delete(mCurrentVideoUri, null, null);
            mCurrentVideoUri = null;
        }
        updateAndShowStorageHint(true);
    }

    private void deleteVideoFile(String fileName) {
        Log.v(TAG, "Deleting video " + fileName);
        File f = new File(fileName);
        if (!f.delete()) {
            Log.v(TAG, "Could not delete " + fileName);
        }
    }

    private void addBaseMenuItems(Menu menu) {
        MenuHelper.addSwitchModeMenuItem(menu, false, new Runnable() {
            public void run() {
                switchToCameraMode();
            }
        });
        MenuItem gallery = menu.add(Menu.NONE, Menu.NONE,
                MenuHelper.POSITION_GOTO_GALLERY,
                R.string.camera_gallery_photos_text)
                .setOnMenuItemClickListener(
                    new OnMenuItemClickListener() {
                        public boolean onMenuItemClick(MenuItem item) {
                            gotoGallery();
                            return true;
                        }
                    });
        gallery.setIcon(android.R.drawable.ic_menu_gallery);
        mGalleryItems.add(gallery);

        if (mNumberOfCameras > 1) {
            menu.add(Menu.NONE, Menu.NONE,
                    MenuHelper.POSITION_SWITCH_CAMERA_ID,
                    R.string.switch_camera_id)
                    .setOnMenuItemClickListener(new OnMenuItemClickListener() {
                public boolean onMenuItemClick(MenuItem item) {
                    switchCameraId((mCameraId + 1) % mNumberOfCameras);
                    return true;
                }
            }).setIcon(android.R.drawable.ic_menu_camera);
        }
        mMenu = menu;
        addModeSwitchMenu();
    }

    private void addModeSwitchMenu() {
        mHas3DSupport = CameraHolder.instance().has3DSupport(mCameraId);
        if(mHas3DSupport == true) {
            int labelId = R.string.switch_camera_mode_lable;
            int iconId = android.R.drawable.ic_menu_camera;
            mMenu.add(Menu.NONE, 100,
                    MenuHelper.POSITION_SWITCH_CAMERA_MODES,
                    labelId)
                .setOnMenuItemClickListener(new OnMenuItemClickListener() {
                        public boolean onMenuItemClick(MenuItem item) {
                            switchCameraMode();
                            return true;
                        }
                    }).setIcon(iconId);
        }
    }

    private void switchCameraMode() {
        if(mCameraMode == CameraInfo.CAMERA_SUPPORT_MODE_3D)
            mCameraMode = CameraInfo.CAMERA_SUPPORT_MODE_2D;
        else
            mCameraMode = CameraInfo.CAMERA_SUPPORT_MODE_3D;
        switchCameraId(mCameraId);
    }

    private void switchCameraId(int cameraId) {
        if (mPausing) return;
        mCameraId = cameraId;
        CameraSettings.writePreferredCameraId(mPreferences, cameraId);
        boolean has3DSupport = CameraHolder.instance().has3DSupport(mCameraId);
        if(has3DSupport != true){
            mCameraMode = CameraInfo.CAMERA_SUPPORT_MODE_2D;
        }
        CameraSettings.writePreferredCameraMode(mPreferences, mCameraMode);

        // This is similar to what mShutterButton.performClick() does,
        // but not quite the same.
        if (mMediaRecorderRecording) {
            if (mIsVideoCaptureIntent) {
                stopVideoRecording();
                showAlert();
            } else {
                stopVideoRecordingAndGetThumbnail();
            }
        } else {
            stopVideoRecording();
        }
        closeCamera();

        if(has3DSupport == true) {
            if((mMenu != null) && (mMenu.findItem(100) == null))
                addModeSwitchMenu();
        }else{
            if((mMenu != null) && (mMenu.findItem(100) != null))
                mMenu.removeItem(100);
        }

        // Reload the preferences.
        mPreferences.setLocalId(this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        // Read media profile again because camera id is changed.
        readVideoPreferences();
        resizeForPreviewAspectRatio();
        restartPreview();

        // Reload the UI.
        initializeHeadUpDisplay();
    }

    private PreferenceGroup filterPreferenceScreenByIntent(
            PreferenceGroup screen) {
        Intent intent = getIntent();
        if (intent.hasExtra(MediaStore.EXTRA_VIDEO_QUALITY)) {
            CameraSettings.removePreferenceFromScreen(screen,
                    CameraSettings.KEY_VIDEO_QUALITY);
        }

        if (intent.hasExtra(MediaStore.EXTRA_DURATION_LIMIT)) {
            CameraSettings.removePreferenceFromScreen(screen,
                    CameraSettings.KEY_VIDEO_QUALITY);
        }
        return screen;
    }

    // from MediaRecorder.OnErrorListener
    public void onError(MediaRecorder mr, int what, int extra) {
        if (what == MediaRecorder.MEDIA_RECORDER_ERROR_UNKNOWN) {
            // We may have run out of space on the sdcard.
            stopVideoRecording();
            updateAndShowStorageHint(true);
        }
        if (what == MediaRecorder.MEDIA_RECORDER_ERROR_RESOURCE) {
            Log.e(TAG, "Resource Error, stop recording and show error notification");
            stopVideoRecording();
            showResourceErrorAndFinish();
        }
    }

    // from MediaRecorder.OnInfoListener
    public void onInfo(MediaRecorder mr, int what, int extra) {
        if (what == MediaRecorder.MEDIA_RECORDER_INFO_MAX_DURATION_REACHED) {
            if (mMediaRecorderRecording) onStopVideoRecording(true);
        } else if (what
                == MediaRecorder.MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED) {
            if (mMediaRecorderRecording) onStopVideoRecording(true);

            // Show the toast.
            Toast.makeText(VideoCamera.this, R.string.video_reach_size_limit,
                           Toast.LENGTH_LONG).show();
        }
        else if (what == MediaRecorder.MEDIA_RECORDER_UNSUPPORTED_RESOLUTION) {
            Toast.makeText(VideoCamera.this, R.string.error_app_unsupported,
                    Toast.LENGTH_LONG).show();
            mUnsupportedResolution = true;
            return;
        }
    }

    /*
     * Make sure we're not recording music playing in the background, ask the
     * MediaPlaybackService to pause playback.
     */
    private void pauseAudioPlayback() {
        // Shamelessly copied from MediaPlaybackService.java, which
        // should be public, but isn't.
        Intent i = new Intent("com.android.music.musicservicecommand");
        i.putExtra("command", "pause");

        sendBroadcast(i);
    }

    private void startVideoRecording() {
        Log.v(TAG, "startVideoRecording");
        updateAndShowStorageHint(true);
        if (mStorageStatus != STORAGE_STATUS_OK) {
            Log.v(TAG, "Storage issue, ignore the start request");
            return;
        }

        if( mUnsupportedHFRVideoSize == true) {
            Log.v(TAG, "Unsupported HFR and video size combinations");
            Toast.makeText(VideoCamera.this, R.string.error_app_unsupported_hfr,
            Toast.LENGTH_SHORT).show();
            return;
        }

        if( mUnsupportedHFRVideoCodec == true) {
            Log.v(TAG, "Unsupported HFR and video codec combinations");
            Toast.makeText(VideoCamera.this, R.string.error_app_unsupported_hfr_codec,
            Toast.LENGTH_SHORT).show();
            return;
        }

        initializeRecorder();

        if (mUnsupportedResolution == true) {
            Log.v(TAG, "Unsupported Resolution according to target");
            return;
        }

        if (mMediaRecorder == null) {
            Log.e(TAG, "Fail to initialize media recorder");
            return;
        }

        pauseAudioPlayback();

        try {
            mMediaRecorder.start(); // Recording is now started
        } catch (RuntimeException e) {
            Log.e(TAG, "Could not start media recorder. ", e);
            releaseMediaRecorder();
            return;
        }
        mHeadUpDisplay.setEnabled(false);

        mMediaRecorderRecording = true;
        mRecordingStartTime = SystemClock.uptimeMillis();
        updateRecordingIndicator(false);
        // Rotate the recording time.
        mRecordingTimeRect.setOrientation(mOrientationCompensation);
        mRecordingTimeView.setText("");
        mRecordingTimeView.setVisibility(View.VISIBLE);
        updateRecordingTime();
        keepScreenOn();
    }

    private void updateRecordingIndicator(boolean showRecording) {
        int drawableId =
                showRecording ? R.drawable.btn_ic_video_record
                        : R.drawable.btn_ic_video_record_stop;
        Drawable drawable = getResources().getDrawable(drawableId);
        mShutterButton.setImageDrawable(drawable);
    }

    private void stopVideoRecordingAndGetThumbnail() {
        stopVideoRecording();
        acquireVideoThumb();
    }

    private void stopVideoRecordingAndReturn(boolean valid) {
        stopVideoRecording();
        doReturnToCaller(valid);
    }

    private void stopVideoRecordingAndShowAlert() {
        stopVideoRecording();
        showAlert();
    }

    private void showAlert() {
        fadeOut(findViewById(R.id.shutter_button));
        if (mCurrentVideoFilename != null) {
            Bitmap src = ThumbnailUtils.createVideoThumbnail(
                    mCurrentVideoFilename, Video.Thumbnails.MINI_KIND);
            // MetadataRetriever already rotates the thumbnail. We should rotate
            // it back (and mirror if it is front-facing camera).
            CameraInfo[] info = CameraHolder.instance().getCameraInfo();
            if (info[mCameraId].facing == CameraInfo.CAMERA_FACING_BACK) {
                src = Util.rotateAndMirror(src, -mOrientationHint, false);
            } else {
                src = Util.rotateAndMirror(src, -mOrientationHint, true);
            }
            mVideoFrame.setImageBitmap(src);
            mVideoFrame.setVisibility(View.VISIBLE);
        }
        int[] pickIds = {R.id.btn_retake, R.id.btn_done, R.id.btn_play};
        for (int id : pickIds) {
            View button = findViewById(id);
            fadeIn(((View) button.getParent()));
        }
    }

    private void hideAlert() {
        mVideoFrame.setVisibility(View.INVISIBLE);
        fadeIn(findViewById(R.id.shutter_button));
        int[] pickIds = {R.id.btn_retake, R.id.btn_done, R.id.btn_play};
        for (int id : pickIds) {
            View button = findViewById(id);
            fadeOut(((View) button.getParent()));
        }
    }

    private static void fadeIn(View view) {
        view.setVisibility(View.VISIBLE);
        Animation animation = new AlphaAnimation(0F, 1F);
        animation.setDuration(500);
        view.startAnimation(animation);
    }

    private static void fadeOut(View view) {
        view.setVisibility(View.INVISIBLE);
        Animation animation = new AlphaAnimation(1F, 0F);
        animation.setDuration(500);
        view.startAnimation(animation);
    }

    private boolean isAlertVisible() {
        return this.mVideoFrame.getVisibility() == View.VISIBLE;
    }

    private void viewLastVideo() {
        Intent intent = null;
        if (mThumbController.isUriValid()) {
            intent = new Intent(Util.REVIEW_ACTION, mThumbController.getUri());
            try {
                startActivity(intent);
            } catch (ActivityNotFoundException ex) {
                try {
                    intent = new Intent(Intent.ACTION_VIEW, mThumbController.getUri());
                    startActivity(intent);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "review video fail", e);
                }
            }
        } else {
            Log.e(TAG, "Can't view last video.");
        }
    }

    private void stopVideoRecording() {
        Log.v(TAG, "stopVideoRecording");
        if (mMediaRecorderRecording) {
            boolean needToRegisterRecording = false;
            mMediaRecorder.setOnErrorListener(null);
            mMediaRecorder.setOnInfoListener(null);
            try {
                mMediaRecorder.stop();
                mCurrentVideoFilename = mVideoFilename;
                Log.v(TAG, "Setting current video filename: "
                        + mCurrentVideoFilename);
                needToRegisterRecording = true;
            } catch (RuntimeException e) {
                Log.e(TAG, "stop fail: " + e.getMessage());
                deleteVideoFile(mVideoFilename);
            }
            mMediaRecorderRecording = false;
            mHeadUpDisplay.setEnabled(true);
            updateRecordingIndicator(true);
            mRecordingTimeView.setVisibility(View.GONE);
            keepScreenOnAwhile();
            if (needToRegisterRecording && mStorageStatus == STORAGE_STATUS_OK
                        && mVideoFilename != null) {
                registerVideo();
            }
            mVideoFilename = null;
            mVideoFileDescriptor = null;
        }
        releaseMediaRecorder();  // always release media recorder
        updateAndShowStorageHint(true);
    }

    private void resetScreenOn() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void keepScreenOnAwhile() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mHandler.sendEmptyMessageDelayed(CLEAR_SCREEN_DELAY, SCREEN_DELAY);
    }

    private void keepScreenOn() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void acquireVideoThumb() {
        Bitmap videoFrame = ThumbnailUtils.createVideoThumbnail(
                mCurrentVideoFilename, Video.Thumbnails.MINI_KIND);
        mThumbController.setData(mCurrentVideoUri, videoFrame);
        mThumbController.updateDisplayIfNeeded();
    }

    private static ImageManager.DataLocation dataLocation() {
        return ImageManager.DataLocation.EXTERNAL;
    }

    private void updateThumbnailButton() {
        // Update the last video thumbnail.
        if (!mIsVideoCaptureIntent) {
            if (!mThumbController.isUriValid()) {
                updateLastVideo();
            }
            mThumbController.updateDisplayIfNeeded();
        }
    }

    private void updateLastVideo() {
        IImageList list = ImageManager.makeImageList(
                        mContentResolver,
                        dataLocation(),
                        ImageManager.INCLUDE_VIDEOS,
                        ImageManager.SORT_ASCENDING,
                        ImageManager.CAMERA_IMAGE_BUCKET_ID);
        int count = list.getCount();
        if (count > 0) {
            IImage image = list.getImageAt(count - 1);
            Uri uri = image.fullSizeImageUri();
            mThumbController.setData(uri, image.miniThumbBitmap());
        } else {
            mThumbController.setData(null, null);
        }
        list.close();
    }

    private void updateRecordingTime() {
        if (!mMediaRecorderRecording) {
            return;
        }
        long now = SystemClock.uptimeMillis();
        long delta = now - mRecordingStartTime;

        // Starting a minute before reaching the max duration
        // limit, we'll countdown the remaining time instead.
        boolean countdownRemainingTime = (mMaxVideoDurationInMs != 0
                && delta >= mMaxVideoDurationInMs - 60000);

        long next_update_delay = 1000 - (delta % 1000);
        long seconds;
        if (countdownRemainingTime) {
            delta = Math.max(0, mMaxVideoDurationInMs - delta);
            seconds = (delta + 999) / 1000;
        } else {
            seconds = delta / 1000; // round to nearest
        }

        long minutes = seconds / 60;
        long hours = minutes / 60;
        long remainderMinutes = minutes - (hours * 60);
        long remainderSeconds = seconds - (minutes * 60);

        String secondsString = Long.toString(remainderSeconds);
        if (secondsString.length() < 2) {
            secondsString = "0" + secondsString;
        }
        String minutesString = Long.toString(remainderMinutes);
        if (minutesString.length() < 2) {
            minutesString = "0" + minutesString;
        }
        String text = minutesString + ":" + secondsString;
        if (hours > 0) {
            String hoursString = Long.toString(hours);
            if (hoursString.length() < 2) {
                hoursString = "0" + hoursString;
            }
            text = hoursString + ":" + text;
        }
        mRecordingTimeView.setText(text);

        if (mRecordingTimeCountsDown != countdownRemainingTime) {
            // Avoid setting the color on every update, do it only
            // when it needs changing.
            mRecordingTimeCountsDown = countdownRemainingTime;

            int color = getResources().getColor(countdownRemainingTime
                    ? R.color.recording_time_remaining_text
                    : R.color.recording_time_elapsed_text);

            mRecordingTimeView.setTextColor(color);
        }

        mHandler.sendEmptyMessageDelayed(
                UPDATE_RECORD_TIME, next_update_delay);
    }

    private static boolean isSupported(String value, List<String> supported) {
        return supported == null ? false : supported.indexOf(value) >= 0;
    }

    private void setCameraParameters() {
        mParameters = mCameraDevice.getParameters();

        int videoWidth, videoHeight;
        if(mProfile != null) {
            videoWidth = mProfile.videoFrameWidth;
            videoHeight = mProfile.videoFrameHeight;
            mParameters.setPreviewFrameRate(mProfile.videoFrameRate);
        } else {
            videoWidth = mVideoWidth;
            videoHeight = mVideoHeight;
            mParameters.setPreviewFrameRate(mVideoFps);
        }

        mParameters.setPreviewSize(videoWidth, videoHeight);

        String recordSize = videoWidth + "x" + videoHeight;
        mParameters.set("record-size", recordSize);

        updateProfileInHud();

        mUnsupportedHFRVideoSize = false;
        mUnsupportedHFRVideoCodec = false;
        // Set High Frame Rate.
        String HighFrameRate = mPreferences.getString(
                CameraSettings.KEY_VIDEO_HIGH_FRAME_RATE,
                getString(R.string.pref_camera_hfr_default));
        if(!("off".equals(HighFrameRate))){
            mUnsupportedHFRVideoSize = true;
            String hfrsize = videoWidth+"x"+videoHeight;
            Log.v(TAG, "current set resolution is : "+hfrsize);
            try {
                for(Size size :  mParameters.getSupportedHfrSizes()){
                   if(size != null) {
                       Log.v(TAG, "supported hfr size : "+ size.width+ " "+size.height);
                       if(videoWidth == size.width && videoHeight == size.height) {
                           mUnsupportedHFRVideoSize = false;
                           Log.v(TAG,"Current hfr resolution is supported");
                           break;
                       }
                   }
               }
            } catch (NullPointerException e){
                Log.e(TAG, "supported hfr sizes is null");
            }

            if(mUnsupportedHFRVideoSize)
                Log.e(TAG,"Unsupported hfr resolution");
            if(mVideoEncoder != MediaRecorder.VideoEncoder.H264){
                mUnsupportedHFRVideoCodec = true;
            }
        }
        if (isSupported(HighFrameRate,
                mParameters.getSupportedVideoHighFrameRateModes())) {
            mParameters.setVideoHighFrameRate(HighFrameRate);
        }
        // Set flash mode.
        String flashMode = mPreferences.getString(
                CameraSettings.KEY_VIDEOCAMERA_FLASH_MODE,
                getString(R.string.pref_camera_video_flashmode_default));
        List<String> supportedFlash = mParameters.getSupportedFlashModes();
        if (isSupported(flashMode, supportedFlash)) {
            mParameters.setFlashMode(flashMode);
        } else {
            flashMode = mParameters.getFlashMode();
            if (flashMode == null) {
                flashMode = getString(
                        R.string.pref_camera_flashmode_no_flash);
            }
        }

        // Set white balance parameter.
        String whiteBalance = mPreferences.getString(
                CameraSettings.KEY_WHITE_BALANCE,
                getString(R.string.pref_camera_whitebalance_default));
        if (isSupported(whiteBalance,
                mParameters.getSupportedWhiteBalance())) {
            mParameters.setWhiteBalance(whiteBalance);
        } else {
            whiteBalance = mParameters.getWhiteBalance();
            if (whiteBalance == null) {
                whiteBalance = Parameters.WHITE_BALANCE_AUTO;
            }
        }

        // Set color effect parameter.
        String colorEffect = mPreferences.getString(
                CameraSettings.KEY_COLOR_EFFECT,
                getString(R.string.pref_camera_coloreffect_default));
        if (isSupported(colorEffect, mParameters.getSupportedColorEffects())) {
            mParameters.setColorEffect(colorEffect);
        }

        if(isSupported(Parameters.FOCUS_MODE_CONTINUOUS_VIDEO,
                        mParameters.getSupportedFocusModes())){
            String continuousAf = mPreferences.getString(
                    CameraSettings.KEY_CONTINUOUS_AF,
                    getString(R.string.pref_camera_continuousaf_default));

            if(CameraSettings.CONTINUOUS_AF_ON
                        .equalsIgnoreCase(continuousAf)){
                mParameters.setFocusMode(
                    Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
            } else {
                mParameters.setFocusMode(
                    Parameters.FOCUS_MODE_INFINITY);
            }
        }

        String zoomStr = mParameters.get("video-zoom-support");
        boolean zoomSupported = "true".equalsIgnoreCase(zoomStr);
        if (zoomSupported) {
            mParameters.setZoom(mZoomValue);
        }

        mCameraDevice.setParameters(mParameters);
        // Keep preview size up to date.
        mParameters = mCameraDevice.getParameters();
    }

    private boolean switchToCameraMode() {
        if (isFinishing() || mMediaRecorderRecording) return false;
        MenuHelper.gotoCameraMode(this);
        finish();
        return true;
    }

    public boolean onSwitchChanged(Switcher source, boolean onOff) {
        if (onOff == SWITCH_CAMERA) {
            return switchToCameraMode();
        } else {
            return true;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);

        // If the camera resumes behind the lock screen, the orientation
        // will be portrait. That causes OOM when we try to allocation GPU
        // memory for the GLSurfaceView again when the orientation changes. So,
        // we delayed initialization of HeadUpDisplay until the orientation
        // becomes landscape.
        changeHeadUpDisplayState();
    }

    private void resetCameraParameters() {

        // We need to restart the preview if preview size is changed.
        Size size = mParameters.getPreviewSize();
        boolean sizeChanged = true;
        if(mProfile != null){
            sizeChanged = size.width != mProfile.videoFrameWidth
                            || size.height != mProfile.videoFrameHeight;
        } else {
            sizeChanged = size.width != mVideoWidth || size.height != mVideoHeight;
        }
        if (sizeChanged) {
            // It is assumed media recorder is released before
            // onSharedPreferenceChanged, so we can close the camera here.
            closeCamera();
            resizeForPreviewAspectRatio();
            restartPreview(); // Parameters will be set in startPreview().
        } else {
            setCameraParameters();
        }
    }

    public void onSizeChanged() {
        // TODO: update the content on GLRootView
    }

    private class MyHeadUpDisplayListener implements HeadUpDisplay.Listener {
        public void onSharedPreferencesChanged() {
            mHandler.post(new Runnable() {
                public void run() {
                    VideoCamera.this.onSharedPreferencesChanged();
                }
            });
        }

        public void onRestorePreferencesClicked() {
            mHandler.post(new Runnable() {
                public void run() {
                    VideoCamera.this.onRestorePreferencesClicked();
                }
            });
        }

        public void onPopupWindowVisibilityChanged(final int visibility) {
        }
    }

    private void onRestorePreferencesClicked() {
        Runnable runnable = new Runnable() {
            public void run() {
                mHeadUpDisplay.restorePreferences(mParameters);
            }
        };
        MenuHelper.confirmAction(this,
                getString(R.string.confirm_restore_title),
                getString(R.string.confirm_restore_message),
                runnable);
    }

    private void onSharedPreferencesChanged() {
        // ignore the events after "onPause()" or preview has not started yet
        if (mPausing) return;
        boolean recordLocation;
        synchronized (mPreferences) {
            recordLocation = RecordLocationPreference.get(
                    mPreferences, getContentResolver());
            if (mRecordLocation != recordLocation) {
                mRecordLocation = recordLocation;
                if (mRecordLocation) {
                    startReceivingLocationUpdates();
                } else {
                    stopReceivingLocationUpdates();
                }
            }
            readVideoPreferences();
            // If mCameraDevice is not ready then we can set the parameter in
            // startPreview().
            if (mCameraDevice == null) return;

            // Check if camera id is changed.
            int cameraId = CameraSettings.readPreferredCameraId(mPreferences);
            if (mCameraId != cameraId) {
                switchCameraId(cameraId);
            } else {
                resetCameraParameters();
            }
        }
    }
}
