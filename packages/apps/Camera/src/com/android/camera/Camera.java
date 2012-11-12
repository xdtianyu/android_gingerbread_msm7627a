/*

 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.camera;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.hardware.Camera.CameraInfo;
import android.graphics.drawable.Drawable;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.CameraDataCallback;
import android.hardware.Camera.CameraMetaDataCallback;
import android.hardware.Camera.Size;
import android.hardware.Camera.Coordinate;
import android.location.Location;
import android.location.LocationManager;
import android.location.LocationProvider;
import android.media.AudioManager;
import android.media.CameraProfile;
import android.media.ToneGenerator;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.provider.Settings;
import android.text.format.DateFormat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Display;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.MenuItem.OnMenuItemClickListener;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;
import android.widget.Toast;

import com.android.camera.gallery.IImage;
import com.android.camera.gallery.IImageList;
import com.android.camera.ui.CameraHeadUpDisplay;
import com.android.camera.ui.GLRootView;
import com.android.camera.ui.HeadUpDisplay;
import com.android.camera.ui.ZoomControllerListener;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.lang.InterruptedException;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.Align;



/** The Camera activity which can preview and take pictures. */
public class Camera extends NoSearchActivity implements View.OnClickListener,
        ShutterButton.OnShutterButtonListener, SurfaceHolder.Callback,
        Switcher.OnSwitchListener {

    private static final String TAG = "camera";
    private static final String TAG_PROFILE = "camera PROFILE";

/*Histogram variables*/
    private GraphView mGraphView;
    private static final int STATS_DATA = 257;
    public static int statsdata[] = new int[STATS_DATA];
    public static boolean mHiston = false;
    public static boolean mBrightnessVisible = true;

    private static final int FACE_DETECTION_OFF = 0;
    private static final int FACE_DETECTION_ON = 1;
    private static final int FACE_DETECTION_CB_DISABLED = 2;

    private static int MAX_FACES_DETECTED = 2;
    private static int ENTRIES_PER_FACE = 4;
    public static int facemetadata[] = new int[MAX_FACES_DETECTED*ENTRIES_PER_FACE+1];
    public static int mFaceDetect = FACE_DETECTION_OFF;
    public static int facesDetected;

    private static final int CROP_MSG = 1;
    private static final int FIRST_TIME_INIT = 2;
    private static final int RESTART_PREVIEW = 3;
    private static final int CLEAR_SCREEN_DELAY = 4;
    private static final int SET_CAMERA_PARAMETERS_WHEN_IDLE = 5;
    private static final int CLEAR_FOCUS_INDICATOR = 6;
    private static final int SET_SKIN_TONE_FACTOR = 7;

    // The subset of parameters we need to update in setCameraParameters().
    private static final int UPDATE_PARAM_INITIALIZE = 1;
    private static final int UPDATE_PARAM_ZOOM = 2;
    private static final int UPDATE_PARAM_PREFERENCE = 4;
    private static final int UPDATE_PARAM_ALL = -1;

    private boolean mUnsupportedJpegQuality = false;

    // When setCameraParametersWhenIdle() is called, we accumulate the subsets
    // needed to be updated in mUpdateSet.
    private int mUpdateSet;

    // The brightness settings used when it is set to automatic in the system.
    // The reason why it is set to 0.7 is just because 1.0 is too bright.
    private static final float DEFAULT_CAMERA_BRIGHTNESS = 0.7f;

    private static final int SCREEN_DELAY = 1000;
    private static final int CLEAR_DELAY = 2000;
    private static final int FOCUS_BEEP_VOLUME = 100;

    private static final int ZOOM_STOPPED = 0;
    private static final int ZOOM_START = 1;
    private static final int ZOOM_STOPPING = 2;

    private int mZoomState = ZOOM_STOPPED;

    // Constant from android.hardware.Camera.Parameters
    private static final String KEY_PICTURE_FORMAT = "picture-format";

    private static final String PIXEL_FORMAT_JPEG = "jpeg";
    private static final String PIXEL_FORMAT_RAW = "raw";

    private boolean mSmoothZoomSupported = false;
    private int mZoomValue;  // The current zoom value.
    private int mZoomMax;
    private int mTargetZoomValue;

    private Parameters mParameters;
    private Parameters mInitialParams;

    private MyOrientationEventListener mOrientationListener;
    // The device orientation in degrees. Default is unknown.
    private int mOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    // The orientation compensation for icons and thumbnails.
    private int mOrientationCompensation = 0;
    private ComboPreferences mPreferences;

    private static final int IDLE = 1;
    private static final int SNAPSHOT_IN_PROGRESS = 2;

    private static final boolean SWITCH_CAMERA = true;
    private static final boolean SWITCH_VIDEO = false;

    private int mStatus = IDLE;
    private static final String sTempCropFilename = "crop-temp";

    private android.hardware.Camera mCameraDevice;
    private ContentProviderClient mMediaProviderClient;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder = null;
    private ShutterButton mShutterButton;
    private FocusRectangle mFocusRectangle;
    public  static FaceRectangle mFaceRectangle[] = new FaceRectangle[MAX_FACES_DETECTED];
    private ToneGenerator mFocusToneGenerator;
    private GestureDetector mGestureDetector;
    private GestureDetector mFocusGestureDetector;
    private Switcher mSwitcher;
    private boolean mStartPreviewFail = false;
    private CountDownLatch devlatch;

    private GLRootView mGLRootView;

    // mPostCaptureAlert, mLastPictureButton, mThumbController
    // are non-null only if isImageCaptureIntent() is true.
    private ImageView mLastPictureButton;
    private ThumbnailController mThumbController;
    private static final int MINIMUM_BRIGHTNESS = 0;
    private static final int MAXIMUM_BRIGHTNESS = 6;
    private int mbrightness = 3;
    private int mbrightness_step = 1;
    private ProgressBar brightnessProgressBar;
    private static final int MIN_SCE_FACTOR = -10;
    private static final int MAX_SCE_FACTOR = +10;
    private int SCE_FACTOR_STEP = 10;
    private int mskinToneValue = 0;
    private SeekBar skinToneSeekBar;
    private TextView Title;
    private TextView RightValue;
    private TextView LeftValue;
    public static final String PREFS="CameraPrefs";

    private static final int MAX_SHARPNESS_LEVEL = 6;

    // mCropValue and mSaveUri are used only if isImageCaptureIntent() is true.
    private String mCropValue;
    private Uri mSaveUri;

    private ImageCapture mImageCapture = null;

    private boolean mPreviewing;
    private boolean mPausing;
    private boolean mFirstTimeInitialized;
    private static  int keypresscount = 0;
    private static  int keyup = 0;
    private boolean mIsImageCaptureIntent;
    private boolean mRecordLocation;
    private boolean mSkinToneSeekBar= false;
    private boolean mSeekBarInitialized = false;

    private static final int FOCUS_NOT_STARTED = 0;
    private static final int FOCUSING = 1;
    private static final int FOCUSING_SNAP_ON_FINISH = 2;
    private static final int FOCUS_SUCCESS = 3;
    private static final int FOCUS_FAIL = 4;
    private static final int FOCUS_SNAP_IMPENDING = 5;
    private int mFocusState = FOCUS_NOT_STARTED;

    private ContentResolver mContentResolver;
    private boolean mDidRegister = false;

    private final ArrayList<MenuItem> mGalleryItems = new ArrayList<MenuItem>();

    private LocationManager mLocationManager = null;

    private final ShutterCallback mShutterCallback = new ShutterCallback();
    private final PostViewPictureCallback mPostViewPictureCallback =
            new PostViewPictureCallback();
    private final RawPictureCallback mRawPictureCallback =
            new RawPictureCallback();
    private final AutoFocusCallback mAutoFocusCallback =
            new AutoFocusCallback();
    private final MetaDataCallback mMetaDataCallback = new MetaDataCallback();
    private final StatsCallback mStatsCallback = new StatsCallback();
    private final ZoomListener mZoomListener = new ZoomListener();
    // Use the ErrorCallback to capture the crash count
    // on the mediaserver
    private final ErrorCallback mErrorCallback = new ErrorCallback();

    private long mFocusStartTime;
    private long mFocusCallbackTime;
    private long mCaptureStartTime;
    private long mShutterCallbackTime;
    private long mPostViewPictureCallbackTime;
    private long mRawPictureCallbackTime;
    private long mJpegPictureCallbackTime;
    private long mShutterdownTime;
    private long mShutterupTime;
    private int mPicturesRemaining;

    // These latency time are for the CameraLatency test.
    public long mAutoFocusTime;
    public long mShutterLag;
    public long mShutterToPictureDisplayedTime;
    public long mPictureDisplayedToJpegCallbackTime;
    public long mJpegCallbackFinishTime;

    // Add for test
    public static boolean mMediaServerDied = false;

    // Focus mode. Options are pref_camera_focusmode_entryvalues.
    private String mFocusMode;
    private String mSceneMode;
    private final Handler mHandler = new MainHandler();
    private CameraHeadUpDisplay mHeadUpDisplay;

    // multiple cameras support
    private int mNumberOfCameras;
    private int mCameraId;
    private int mCameraMode;
    private Menu mMenu;
    private boolean mHas3DSupport;
    private int mSnapshotMode;
    private boolean mHasZSLSupport;
    private int mNumSnapshots = 1;

    private int mImageWidth = 0;
    private int mImageHeight = 0;

    /**
     * This Handler is used to post message back onto the main thread of the
     * application
     */
    private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case RESTART_PREVIEW: {
                    restartPreview();
                    if (mJpegPictureCallbackTime != 0) {
                        long now = System.currentTimeMillis();
                        mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
                        Log.v(TAG, "mJpegCallbackFinishTime = "
                                + mJpegCallbackFinishTime + "ms");
                        mJpegPictureCallbackTime = 0;
                    }
                    break;
                }

                case CLEAR_SCREEN_DELAY: {
                    getWindow().clearFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    break;
                }

                case FIRST_TIME_INIT: {
                    initializeFirstTime();
                    break;
                }

                case SET_CAMERA_PARAMETERS_WHEN_IDLE: {
                    setCameraParametersWhenIdle(0);
                    break;
                }
                case CLEAR_FOCUS_INDICATOR: {
                    if (mFocusState == FOCUS_SNAP_IMPENDING) {
                        clearFocusIndicator();
                    }
                    break;
                }
                case SET_SKIN_TONE_FACTOR: {
                     setSkinToneFactor();
                     mSeekBarInitialized = true;
                     break;
                }
            }
        }
    }

    private void resetExposureCompensation() {
        String value = mPreferences.getString(CameraSettings.KEY_EXPOSURE,
                CameraSettings.EXPOSURE_DEFAULT_VALUE);
        if (!CameraSettings.EXPOSURE_DEFAULT_VALUE.equals(value)) {
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_EXPOSURE, "0");
            editor.apply();
            if (mHeadUpDisplay != null) {
                mHeadUpDisplay.reloadPreferences();
            }
        }
    }

    private void keepMediaProviderInstance() {
        // We want to keep a reference to MediaProvider in camera's lifecycle.
        // TODO: Utilize mMediaProviderClient instance to replace
        // ContentResolver calls.
        if (mMediaProviderClient == null) {
            mMediaProviderClient = getContentResolver()
                    .acquireContentProviderClient(MediaStore.AUTHORITY);
        }
    }

    // Snapshots can only be taken after this is called. It should be called
    // once only. We could have done these things in onCreate() but we want to
    // make preview screen appear as soon as possible.
    private void initializeFirstTime() {
        if (mFirstTimeInitialized) return;

        // Create orientation listenter. This should be done first because it
        // takes some time to get first orientation.
        mOrientationListener = new MyOrientationEventListener(Camera.this);
        mOrientationListener.enable();

        // Initialize location sevice.
        mLocationManager = (LocationManager)
                getSystemService(Context.LOCATION_SERVICE);
        mRecordLocation = RecordLocationPreference.get(
                mPreferences, getContentResolver());
        if (mRecordLocation) startReceivingLocationUpdates();

        keepMediaProviderInstance();
        checkStorage();

        // Initialize last picture button.
        mContentResolver = getContentResolver();
        if (!mIsImageCaptureIntent)  {
            findViewById(R.id.camera_switch).setOnClickListener(this);
            mLastPictureButton =
                    (ImageView) findViewById(R.id.review_thumbnail);
            mLastPictureButton.setOnClickListener(this);
            mThumbController = new ThumbnailController(
                    getResources(), mLastPictureButton, mContentResolver);
            mThumbController.loadData(ImageManager.getLastImageThumbPath());
            // Update last image thumbnail.
            updateThumbnailButton();
        }

        // Initialize shutter button.
        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.setVisibility(View.VISIBLE);

        mFocusRectangle = (FocusRectangle) findViewById(R.id.focus_rectangle);
        mFaceRectangle[0] = (FaceRectangle) findViewById(R.id.face_rectangle1);
        mFaceRectangle[1] = (FaceRectangle) findViewById(R.id.face_rectangle2);

        updateFocusIndicator();

        initializeScreenBrightness();
        installIntentFilter();
        initializeFocusTone();

        mGraphView = (GraphView)findViewById(R.id.graph_view);
        mGraphView.setCameraObject(Camera.this);
        initializeZoom();
        mHeadUpDisplay = new CameraHeadUpDisplay(this);
        mHeadUpDisplay.setListener(new MyHeadUpDisplayListener());
        initializeHeadUpDisplay();
        mFirstTimeInitialized = true;
        changeHeadUpDisplayState();
        addIdleHandler();
    }

    private void addIdleHandler() {
        MessageQueue queue = Looper.myQueue();
        queue.addIdleHandler(new MessageQueue.IdleHandler() {
            public boolean queueIdle() {
                ImageManager.ensureOSXCompatibleFolder();
                return false;
            }
        });
    }

    private void updateThumbnailButton() {
        // Update last image if URI is invalid and the storage is ready.
        if (!mThumbController.isUriValid() && mPicturesRemaining >= 0) {
            updateLastImage();
        }
        mThumbController.updateDisplayIfNeeded();
    }

    // If the activity is paused and resumed, this method will be called in
    // onResume.
    private void initializeSecondTime() {
        // Start orientation listener as soon as possible because it takes
        // some time to get first orientation.
        mOrientationListener.enable();

        // Start location update if needed.
        mRecordLocation = RecordLocationPreference.get(
                mPreferences, getContentResolver());
        if (mRecordLocation) startReceivingLocationUpdates();

        installIntentFilter();
        initializeFocusTone();
        initializeZoom();
        changeHeadUpDisplayState();

        keepMediaProviderInstance();
        checkStorage();
        // If onResume initializeSecondtime is called we reset camera object
        if(mGraphView != null)
          mGraphView.setCameraObject(Camera.this);


        if (!mIsImageCaptureIntent) {
            updateThumbnailButton();
        }
    }

    private void initializeZoom() {
        if (!mParameters.isZoomSupported()) return;

        mZoomMax = mParameters.getMaxZoom();
        mSmoothZoomSupported = mParameters.isSmoothZoomSupported();
        mGestureDetector = new GestureDetector(this, new ZoomGestureListener());

        mCameraDevice.setZoomChangeListener(mZoomListener);
    }

    private void onZoomValueChanged(int index) {
        if (mSmoothZoomSupported) {
            if (mTargetZoomValue != index && mZoomState != ZOOM_STOPPED) {
                mTargetZoomValue = index;
                if (mZoomState == ZOOM_START) {
                    mZoomState = ZOOM_STOPPING;
                    mCameraDevice.stopSmoothZoom();
                }
            } else if (mZoomState == ZOOM_STOPPED && mZoomValue != index) {
                mTargetZoomValue = index;
                mCameraDevice.startSmoothZoom(index);
                mZoomState = ZOOM_START;
            }
        } else {
            mZoomValue = index;
            setCameraParametersWhenIdle(UPDATE_PARAM_ZOOM);
        }
    }

    private float[] getZoomRatios() {
        if(!mParameters.isZoomSupported()) return null;
        List<Integer> zoomRatios = mParameters.getZoomRatios();
        float result[] = new float[zoomRatios.size()];
        for (int i = 0, n = result.length; i < n; ++i) {
            result[i] = (float) zoomRatios.get(i) / 100f;
        }
        return result;
    }

    private class ZoomGestureListener extends
            GestureDetector.SimpleOnGestureListener {

        @Override
        public boolean onDoubleTap(MotionEvent e) {

            // Perform zoom only when preview is started and snapshot is not in
            // progress.

            if (mPausing || !isCameraIdle() || !mPreviewing
                    || mZoomState != ZOOM_STOPPED) {
                return false;
            }

            if (mZoomValue < mZoomMax) {
                // Zoom in to the maximum.
                mZoomValue = mZoomMax;
            } else {
                mZoomValue = 0;
            }

            setCameraParametersWhenIdle(UPDATE_PARAM_ZOOM);

            mHeadUpDisplay.setZoomIndex(mZoomValue);

            return true;
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent m) {
        if (!super.dispatchTouchEvent(m)) {
        if (mGestureDetector != null) {
             mGestureDetector.onTouchEvent(m);
        }

        if (mFocusGestureDetector != null) {
            mFocusGestureDetector.onTouchEvent(m);
        }
        return true;
        }

        return true;
    }

    LocationListener [] mLocationListeners = new LocationListener[] {
            new LocationListener(LocationManager.GPS_PROVIDER),
            new LocationListener(LocationManager.NETWORK_PROVIDER)
    };

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(Intent.ACTION_MEDIA_MOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_UNMOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_CHECKING)) {
                checkStorage();
            } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_FINISHED)) {
                checkStorage();
                if (!mIsImageCaptureIntent)  {
                    updateThumbnailButton();
                }
            }
        }
    };

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
            // If GPS is available before start camera, we won't get status
            // update so update GPS indicator when we receive data.
            if (mRecordLocation
                    && LocationManager.GPS_PROVIDER.equals(mProvider)) {
                if (mHeadUpDisplay != null) {
                    mHeadUpDisplay.setGpsHasSignal(true);
                }
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
                    if (mRecordLocation &&
                            LocationManager.GPS_PROVIDER.equals(provider)) {
                        if (mHeadUpDisplay != null) {
                            mHeadUpDisplay.setGpsHasSignal(false);
                        }
                    }
                    break;
                }
            }
        }

        public Location current() {
            return mValid ? mLastLocation : null;
        }
    }

    private final class ShutterCallback
            implements android.hardware.Camera.ShutterCallback {
        public void onShutter() {
            mShutterCallbackTime = System.currentTimeMillis();
            mShutterLag = mShutterCallbackTime - mCaptureStartTime;
            Log.v(TAG, "mShutterLag = " + mShutterLag + "ms");
            clearFocusState();
        }
    }

    private final class StatsCallback
           implements CameraDataCallback {
        public void onCameraData(int [] data, android.hardware.Camera camera) {
            if(!mPreviewing || !mHiston || !mFirstTimeInitialized){
                return;
            }
            /*The first element in the array stores max hist value . Stats data begin from second value*/
            synchronized(statsdata) {
                System.arraycopy(data,0,statsdata,0,STATS_DATA);
            }
            if(mGraphView != null)
                mGraphView.PreviewChanged();
        }
    }
    private final class MetaDataCallback
           implements CameraMetaDataCallback {
        public void onCameraMetaData(final int [] metaData, android.hardware.Camera camera) {
            if(!mPreviewing || mFaceDetect != FACE_DETECTION_ON){
                return;
            }

            /* The integer array has face meta data store as x,y,dx,dy consecutively for MAX_FACES_DETECTED */
            synchronized(facemetadata) {
                if(metaData != null)
                    System.arraycopy(metaData,0,facemetadata,0,(MAX_FACES_DETECTED*ENTRIES_PER_FACE+1));
            }

            if(isFaceRectangleValid()) {
                facesDetected = facemetadata[0]/ENTRIES_PER_FACE;
                clearFaceRectangles();
                redrawFaceRectangles(facemetadata);
            }
            mCameraDevice.sendMetaData();
        }
    }

    private final class PostViewPictureCallback implements PictureCallback {
        public void onPictureTaken(
                byte [] data, android.hardware.Camera camera) {
            mPostViewPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToPostViewCallbackTime = "
                    + (mPostViewPictureCallbackTime - mShutterCallbackTime)
                    + "ms");
        }
    }

    private final class RawPictureCallback implements PictureCallback {
        public void onPictureTaken(
                byte [] rawData, android.hardware.Camera camera) {
            mRawPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToRawCallbackTime = "
                    + (mRawPictureCallbackTime - mShutterCallbackTime) + "ms");

            if (mShutterupTime != 0)
                Log.e(TAG,"<PROFILE> Snapshot to Thumb Latency = "
                        + (mRawPictureCallbackTime - mShutterupTime) + " ms");


        }
    }

    private final class JpegPictureCallback implements PictureCallback {
        Location mLocation;

        public JpegPictureCallback(Location loc) {
            mLocation = loc;
        }

        public void onPictureTaken(
                final byte [] jpegData, final android.hardware.Camera camera) {
            if (mPausing) {
                return;
            }
            Log.v(TAG, "onPictureTaken: snapshot number received: " + mNumSnapshots);
            if(mNumSnapshots > 0) mNumSnapshots--;
            Log.v(TAG, "onPictureTaken: snapshots pending: " + mNumSnapshots);

            mJpegPictureCallbackTime = System.currentTimeMillis();
            // If postview callback has arrived, the captured image is displayed
            // in postview callback. If not, the captured image is displayed in
            // raw picture callback.
            if (mPostViewPictureCallbackTime != 0) {
                mShutterToPictureDisplayedTime =
                        mPostViewPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mPostViewPictureCallbackTime;
            } else {
                mShutterToPictureDisplayedTime =
                        mRawPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mRawPictureCallbackTime;
            }
            Log.v(TAG, "mPictureDisplayedToJpegCallbackTime = "
                    + mPictureDisplayedToJpegCallbackTime + "ms");
            if(mNumSnapshots == 0 && mHeadUpDisplay != null)
                mHeadUpDisplay.setEnabled(true);

            if (mShutterupTime != 0)
                Log.e(TAG,"<PROFILE> Snapshot to Snapshot Latency = "
                        + (mJpegPictureCallbackTime - mShutterupTime) + " ms");


            if(mNumSnapshots == 0) {
                if (!mIsImageCaptureIntent) {
                    String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);

                    if((pictureFormat == null) ||
                        PIXEL_FORMAT_JPEG.equalsIgnoreCase(pictureFormat)){
                        // We want to show the taken picture for a while, so we wait
                        // for at least 1.2 second before restarting the preview.
                       long delay = 1200 - mPictureDisplayedToJpegCallbackTime;
                       if (delay < 0) {
                            restartPreview();
                        } else {
                            mHandler.sendEmptyMessageDelayed(RESTART_PREVIEW, delay);
                        }
                    }
                    if(PIXEL_FORMAT_RAW.equalsIgnoreCase(pictureFormat)){
                        mHandler.sendEmptyMessage(RESTART_PREVIEW);
                    }
                }
            }

            if(jpegData != null) {
                mImageCapture.storeImage(jpegData, camera, mLocation);
            } else {
                Log.e(TAG, "null jpeg data, not storing");
            }

            // Calculate this in advance of each shot so we don't add to shutter
            // latency. It's true that someone else could write to the SD card in
            // the mean time and fill it, but that could have happened between the
            // shutter press and saving the JPEG too.
            calculatePicturesRemaining();

            if (mPicturesRemaining < 1) {
                updateStorageHint(mPicturesRemaining);
            }

            if (!mHandler.hasMessages(RESTART_PREVIEW)) {
                long now = System.currentTimeMillis();
                mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
                Log.v(TAG, "mJpegCallbackFinishTime = "
                        + mJpegCallbackFinishTime + "ms");
                mJpegPictureCallbackTime = 0;
            }

            if(mNumSnapshots == 0)
                decrementkeypress();
        }
        }
     private OnSeekBarChangeListener mSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
        // no support
        }
        public void onProgressChanged(SeekBar bar, int progress, boolean fromtouch) {
        }
        public void onStopTrackingTouch(SeekBar bar) {
        }
    };

     private OnSeekBarChangeListener mskinToneSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
        // no support
        }

        public void onProgressChanged(SeekBar bar, int progress, boolean fromtouch) {
            int value = (progress + MIN_SCE_FACTOR) * SCE_FACTOR_STEP;
            if(progress > (MAX_SCE_FACTOR - MIN_SCE_FACTOR)/2){
                RightValue.setText(String.valueOf(value));
                LeftValue.setText("");
            } else if (progress < (MAX_SCE_FACTOR - MIN_SCE_FACTOR)/2){
                LeftValue.setText(String.valueOf(value));
                RightValue.setText("");
            } else {
                LeftValue.setText("");
                RightValue.setText("");
            }
            if(value != mskinToneValue) {
                mskinToneValue = value;
                mParameters = mCameraDevice.getParameters();
                mParameters.set("skinToneEnhancement", String.valueOf(mskinToneValue));
                mCameraDevice.setParameters(mParameters);
            }
        }

        public void onStopTrackingTouch(SeekBar bar) {
        }
    };

    private final class AutoFocusCallback
            implements android.hardware.Camera.AutoFocusCallback {
        public void onAutoFocus(
                boolean focused, android.hardware.Camera camera) {
            mFocusCallbackTime = System.currentTimeMillis();
            mAutoFocusTime = mFocusCallbackTime - mFocusStartTime;

            Coordinate touchIndex = mParameters.getTouchIndexAec();
            int x = touchIndex.xCoordinate;
            int y = touchIndex.yCoordinate;

            String touchAfAec = currentTouchAfAecMode();

            Log.v(TAG, "<PROFILE> mAutoFocusTime = " + mAutoFocusTime + "ms");
            if (mFocusState == FOCUSING_SNAP_ON_FINISH) {
                // Take the picture no matter focus succeeds or fails. No need
                // to play the AF sound if we're about to play the shutter
                // sound.
                if (focused) {
                    mFocusState = FOCUS_SUCCESS;
                } else {
                    mFocusState = FOCUS_FAIL;
                }
                if (touchAfAec != null) {
                    if (touchAfAec.equals(Parameters.TOUCH_AF_AEC_OFF)) {
                        mImageCapture.onSnap();
                    } else {
                        if ((x < 0) && (y < 0)) {
                            mImageCapture.onSnap();
                        }
                        else {
                            mFocusState = FOCUS_SNAP_IMPENDING;
                            mHeadUpDisplay.setEnabled(true);
                        }
                    }
                }
                else {
                    mImageCapture.onSnap();
                }
            } else if (mFocusState == FOCUSING) {
                // User is half-pressing the focus key. Play the focus tone.
                // Do not take the picture now.

                ToneGenerator tg = mFocusToneGenerator;
                if (tg != null) {
                    tg.startTone(ToneGenerator.TONE_PROP_BEEP2);
                }
                if (focused) {
                    mFocusState = FOCUS_SUCCESS;
                } else {
                    mFocusState = FOCUS_FAIL;
                }

                if (touchAfAec != null) {
                    if(touchAfAec.equals(Parameters.TOUCH_AF_AEC_ON)
                            && (x >=0 && y >= 0))  {
                        mFocusState = FOCUS_SNAP_IMPENDING;
                        mHeadUpDisplay.setEnabled(true);
                    }
                }
            } else if (mFocusState == FOCUS_NOT_STARTED) {
                // User has released the focus key before focus completes.
                // Do nothing.
            }
            if(mFaceDetect == FACE_DETECTION_OFF)
                updateFocusIndicator();
            mHandler.sendEmptyMessageDelayed(CLEAR_FOCUS_INDICATOR, CLEAR_DELAY);
        }
    }

    private final class ErrorCallback
        implements android.hardware.Camera.ErrorCallback {
        public void onError(int error, android.hardware.Camera camera) {
            switch (error) {
                case android.hardware.Camera.CAMERA_ERROR_SERVER_DIED:
                     mMediaServerDied = true;
                     Log.v(TAG, "media server died");
                     break;
                case android.hardware.Camera.CAMERA_ERROR_UNKNOWN:
                     Log.d(TAG, "Camera Driver Error");
                     showCameraStoppedAndFinish();
                     break;
            }
        }
    }

    private final class ZoomListener
            implements android.hardware.Camera.OnZoomChangeListener {
        public void onZoomChange(
                int value, boolean stopped, android.hardware.Camera camera) {
            Log.v(TAG, "Zoom changed: value=" + value + ". stopped="+ stopped);
            mZoomValue = value;
            // Keep mParameters up to date. We do not getParameter again in
            // takePicture. If we do not do this, wrong zoom value will be set.
            mParameters.setZoom(value);
            // We only care if the zoom is stopped. mZooming is set to true when
            // we start smooth zoom.
            if (stopped && mZoomState != ZOOM_STOPPED) {
                if (value != mTargetZoomValue) {
                    mCameraDevice.startSmoothZoom(mTargetZoomValue);
                    mZoomState = ZOOM_START;
                } else {
                    mZoomState = ZOOM_STOPPED;
                }
            }
        }
    }

    private class ImageCapture {

        private Uri mLastContentUri;

        byte[] mCaptureOnlyData;

        // Returns the rotation degree in the jpeg header.
        private int storeImage(byte[] data, Location loc) {
            try {
                long dateTaken = System.currentTimeMillis();
                String ext = null;
                String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);
                String store_location = ImageManager.CAMERA_IMAGE_BUCKET_NAME;

                if(pictureFormat == null ||
                        PIXEL_FORMAT_JPEG.equalsIgnoreCase(pictureFormat)){
                    ext = ".jpg";
                }

                if(PIXEL_FORMAT_RAW.equalsIgnoreCase(pictureFormat)){
                    ext = ".raw";
                    store_location = ImageManager.CAMERA_RAW_IMAGE_BUCKET_NAME;
                }
                if(mCameraMode == CameraInfo.CAMERA_SUPPORT_MODE_3D){
                    ext = ".mpo";
                    store_location = ImageManager.CAMERA_MPO_IMAGE_BUCKET_NAME;
                }


                String title = createName(dateTaken);
                String filename = title + "_" + mNumSnapshots + ext;
                Log.v(TAG,"storeImage: filename = " + filename);
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
                android.hardware.Camera camera, Location loc) {
            if (!mIsImageCaptureIntent) {
                int degree = storeImage(data, loc);
                sendBroadcast(new Intent(
                        "com.android.camera.NEW_PICTURE", mLastContentUri));
                setLastPictureThumb(data, degree,
                        mImageCapture.getLastCaptureUri());
                mThumbController.updateDisplayIfNeeded();
            } else {
                mCaptureOnlyData = data;
                showPostCaptureAlert();
            }
        }

        /**
         * Initiate the capture of an image.
         */
        public void initiate() {
            if (mCameraDevice == null) {
                return;
            }

            capture();
        }

        public Uri getLastCaptureUri() {
            return mLastContentUri;
        }

        public byte[] getLastCaptureData() {
            return mCaptureOnlyData;
        }

        private void capture() {
            mCaptureOnlyData = null;

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
            mParameters.setRotation(rotation);

            // Clear previous GPS location from the parameters.
            mParameters.removeGpsData();

            // We always encode GpsTimeStamp
            mParameters.setGpsTimestamp(System.currentTimeMillis() / 1000);
            String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);

            // Set GPS location.
            Location loc = null;
            if (mRecordLocation && (pictureFormat != null
                &&  PIXEL_FORMAT_JPEG.equalsIgnoreCase(pictureFormat))) {
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
            cameraUtilProfile("pre set exif");
            if (dateTaken != 0) {
                String datetime = DateFormat.format("yyyy:MM:dd kk:mm:ss", dateTaken).toString();
                Log.e(TAG,"datetime : " +  datetime);
                mParameters.setExifDateTime(datetime);
            }

            cameraUtilProfile("pre set parm");
            mCameraDevice.setParameters(mParameters);

            incrementkeypress();
            cameraUtilProfile("pre set view");
            Size pictureSize = mParameters.getPictureSize();
            mImageWidth = pictureSize.width;
            mImageHeight = pictureSize.height;
            if(mHiston) {
                mHiston = false;
                mCameraDevice.setHistogramMode(null);
                if(mGraphView != null)
                    mGraphView.setVisibility(View.INVISIBLE);
            }

            cameraUtilProfile("pre-snap");
            mNumSnapshots = 1;
            if(mSnapshotMode == CameraInfo.CAMERA_SUPPORT_MODE_ZSL){
                mNumSnapshots = mParameters.getInt("num-snaps-per-shutter");
            }
            Log.v(TAG, "takePicture for " + mNumSnapshots + " number of snapshots");
            mCameraDevice.takePicture(mShutterCallback, mRawPictureCallback,
                    mPostViewPictureCallback, new JpegPictureCallback(loc));
            mPreviewing = false;
        }

        public void onSnap() {
            // If we are already in the middle of taking a snapshot then ignore.
            if (mPausing || mStatus == SNAPSHOT_IN_PROGRESS || !mPreviewing
                || (mSmoothZoomSupported && (mZoomState != ZOOM_STOPPED))) {
                return;
            }
            mCaptureStartTime = System.currentTimeMillis();
            cameraUtilProfile("Snap start at");

            mPostViewPictureCallbackTime = 0;
            mHeadUpDisplay.setEnabled(false);
            mStatus = SNAPSHOT_IN_PROGRESS;

            mImageCapture.initiate();
        }

        private void clearLastData() {
            mCaptureOnlyData = null;
        }
    }

    private boolean saveDataToFile(String filePath, byte[] data) {
        FileOutputStream f = null;
        try {
            f = new FileOutputStream(filePath);
            f.write(data);
        } catch (IOException e) {
            return false;
        } finally {
            MenuHelper.closeSilently(f);
        }
        return true;
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

    private String createName(long dateTaken) {
        Date date = new Date(dateTaken);
        SimpleDateFormat dateFormat = new SimpleDateFormat(
                getString(R.string.image_file_name_format));

        return dateFormat.format(date);
    }

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        cameraUtilProfile("Create camera");
        mPreferences = new ComboPreferences(this);
        CameraSettings.upgradeGlobalPreferences(mPreferences.getGlobal());
        mCameraId = CameraSettings.readPreferredCameraId(mPreferences);
        mCameraMode = CameraSettings.readPreferredCameraMode(mPreferences);
        mSnapshotMode = CameraSettings.readPreferredSnapshotMode(mPreferences);
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

        setContentView(R.layout.camera);

        mSurfaceView = (SurfaceView) findViewById(R.id.camera_preview);

        mNumberOfCameras = CameraHolder.instance().getNumberOfCameras();
        if(mCameraId > (mNumberOfCameras-1)) {
            mCameraId = 0;
            mPreferences.setLocalId(this, mCameraId);
            CameraSettings.writePreferredCameraId(mPreferences, mCameraId);
        } else
            mPreferences.setLocalId(this, mCameraId);

        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        SharedPreferences ShPrefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        mbrightness = ShPrefs.getInt("pref_camera_brightness_key",mbrightness);
        mskinToneValue = ShPrefs.getInt("pref_camera_skin_tone_enhancement_key",mskinToneValue);

        mShutterdownTime = 0;
        mShutterupTime = 0;


        // we need to reset exposure for the preview
        resetExposureCompensation();

        // Let thread know it's ok to continue
        devlatch.countDown();

        // don't set mSurfaceHolder here. We have it set ONLY within
        // surfaceChanged / surfaceDestroyed, other parts of the code
        // assume that when it is set, the surface is also set.
        SurfaceHolder holder = mSurfaceView.getHolder();
        holder.addCallback(this);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        mIsImageCaptureIntent = isImageCaptureIntent();
        if (mIsImageCaptureIntent) {
            setupCaptureParams();
        }

        LayoutInflater inflater = getLayoutInflater();

        ViewGroup rootView = (ViewGroup) findViewById(R.id.camera);
        if (mIsImageCaptureIntent) {
            View controlBar = inflater.inflate(
                    R.layout.attach_camera_control, rootView);
            controlBar.findViewById(R.id.btn_cancel).setOnClickListener(this);
            controlBar.findViewById(R.id.btn_retake).setOnClickListener(this);
            controlBar.findViewById(R.id.btn_done).setOnClickListener(this);
        } else {
            inflater.inflate(R.layout.camera_control, rootView);
            mSwitcher = ((Switcher) findViewById(R.id.camera_switch));
            mSwitcher.setOnSwitchListener(this);
            mSwitcher.addTouchView(findViewById(R.id.camera_switch_set));
        }
        brightnessProgressBar = (ProgressBar) findViewById(R.id.progress);
        if (brightnessProgressBar instanceof SeekBar) {
            SeekBar seeker = (SeekBar) brightnessProgressBar;
            seeker.setOnSeekBarChangeListener(mSeekListener);
        }
        brightnessProgressBar.setMax(MAXIMUM_BRIGHTNESS);
        brightnessProgressBar.setProgress(mbrightness);
        skinToneSeekBar = (SeekBar) findViewById(R.id.skintoneseek);
        skinToneSeekBar.setOnSeekBarChangeListener(mskinToneSeekListener);
        skinToneSeekBar.setVisibility(View.INVISIBLE);
        Title = (TextView)findViewById(R.id.skintonetitle);
        RightValue = (TextView)findViewById(R.id.skintoneright);
        LeftValue = (TextView)findViewById(R.id.skintoneleft);

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
        devlatch = null;
    }

    private void changeHeadUpDisplayState() {
        // If the camera resumes behind the lock screen, the orientation
        // will be portrait. That causes OOM when we try to allocation GPU
        // memory for the GLSurfaceView again when the orientation changes. So,
        // we delayed initialization of HeadUpDisplay until the orientation
        // becomes landscape.
        Configuration config = getResources().getConfiguration();
        if (config.orientation == Configuration.ORIENTATION_LANDSCAPE
                && !mPausing && mFirstTimeInitialized) {
            if (mGLRootView == null) attachHeadUpDisplay();
        } else if (mGLRootView != null) {
            detachHeadUpDisplay();
        }
    }

    private void overrideSkinToneHudSettings(final String skinToneMode) {
        mHeadUpDisplay.overrideSettings(
                CameraSettings.KEY_SKIN_TONE_ENHANCEMENT, skinToneMode);
    }

    private void overrideHudSettings(final String flashMode,
            final String whiteBalance, final String focusMode,
            final String sceneDetect, final String exposureCompensation) {
        mHeadUpDisplay.overrideSettings(
                CameraSettings.KEY_FLASH_MODE, flashMode,
                CameraSettings.KEY_WHITE_BALANCE, whiteBalance,
                CameraSettings.KEY_FOCUS_MODE, focusMode,
                CameraSettings.KEY_SCENE_DETECT, sceneDetect,
                CameraSettings.KEY_EXPOSURE, exposureCompensation);
    }

    private void updateSceneModeInHud() {
        // If scene mode is set, we cannot set flash mode, white balance, and
        // focus mode, instead, we read it from driver
        if (!Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            mParameters.setSceneDetectMode(getString(R.string.pref_camera_scenedetect_default));
            overrideHudSettings(getString(R.string.pref_camera_flashmode_default),
                                getString(R.string.pref_camera_whitebalance_default),
                                getString(R.string.pref_camera_focusmode_default),
                                getString(R.string.pref_camera_scenedetect_default),
                                getString(R.string.pref_exposure_default));
            if(brightnessProgressBar != null) {
                brightnessProgressBar.setVisibility(View.INVISIBLE);
                mBrightnessVisible = false;
            }
        } else {
            overrideHudSettings(null, null, null, null, null);
            if(brightnessProgressBar != null) {
                brightnessProgressBar.setVisibility(View.VISIBLE);
                mBrightnessVisible = true;
            }
        }
          if(!Parameters.SCENE_MODE_AUTO.equals(mSceneMode) &&
             !Parameters.SCENE_MODE_PARTY.equals(mSceneMode)&&
             !Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode)) {
             mParameters.set("skinToneEnhancement","disable");
             overrideSkinToneHudSettings(getString
                     (R.string.pref_camera_skinToneEnhancement_default));
            if(skinToneSeekBar != null)
              disableSkinToneSeekBar();
           } else {
             overrideSkinToneHudSettings(null);

           }
    }


    private void updateColorEffectSettingsInHud() {
        String colorEffect = mPreferences.getString(
                CameraSettings.KEY_COLOR_EFFECT,
                getString(R.string.pref_camera_coloreffect_default));
        if (!Parameters.EFFECT_NONE.equals(colorEffect)) {
            mParameters.set("skinToneEnhancement","disable");
            overrideSkinToneHudSettings(getString
                        (R.string.pref_camera_skinToneEnhancement_default));
            if (skinToneSeekBar != null)
            disableSkinToneSeekBar();
        } else {
            if(!Parameters.SCENE_MODE_AUTO.equals(mSceneMode) &&
               !Parameters.SCENE_MODE_PARTY.equals(mSceneMode)&&
               !Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode)) {
               mParameters.set("skinToneEnhancement","disable");
               overrideSkinToneHudSettings(getString
                        (R.string.pref_camera_skinToneEnhancement_default));
               if (skinToneSeekBar != null)
                  disableSkinToneSeekBar();
            } else {
               overrideSkinToneHudSettings(null);
            }
        }
    }

    private void overrideAfHudSettings(final String selectableZoneAf, final String faceDetection) {
                mHeadUpDisplay.overrideSettings(
                        CameraSettings.KEY_SELECTABLE_ZONE_AF, selectableZoneAf);
                mHeadUpDisplay.overrideSettings(
                        CameraSettings.KEY_FACE_DETECTION, faceDetection);
    }

    private void updateAfSettingsInHud() {
         String faceDetection = mPreferences.getString(
                CameraSettings.KEY_FACE_DETECTION,
                getString(R.string.pref_camera_facedetection_default));
         String touchAfAec = mPreferences.getString(
                CameraSettings.KEY_TOUCH_AF_AEC,
                getString(R.string.pref_camera_touchafaec_default));
         if (!Parameters.TOUCH_AF_AEC_OFF.equals(touchAfAec)) {
            mParameters.setSelectableZoneAf(getString(R.string.pref_camera_selectablezoneaf_default));
            mParameters.setFaceDetectionMode(getString(R.string.pref_camera_facedetection_default));
            mCameraDevice.setParameters(mParameters);

            if (isSupported(faceDetection, mParameters.getSupportedFaceDetectionModes())) {
                mCameraDevice.setFaceDetectionCb(null);
                clearFaceRectangles();
                mFaceDetect = FACE_DETECTION_OFF;
            }
            overrideAfHudSettings(getString(R.string.pref_camera_selectablezoneaf_default),
                                  getString(R.string.pref_camera_facedetection_default));
         } else {
             String selectableZonAf  = mPreferences.getString(
                     CameraSettings.KEY_SELECTABLE_ZONE_AF,
                     getString(R.string.pref_camera_selectablezoneaf_default));

             if (!Parameters.FACE_DETECTION_OFF.equals(faceDetection)) {
                 mParameters.setSelectableZoneAf(getString(R.string.pref_camera_selectablezoneaf_default));
                 mCameraDevice.setParameters(mParameters);
                 overrideAfHudSettings(getString(R.string.pref_camera_selectablezoneaf_default),
                         null);
             } else if (!Parameters.SELECTABLE_ZONE_AF_AUTO.equals(selectableZonAf)) {
                 mParameters.setFaceDetectionMode(getString(R.string.pref_camera_facedetection_default));
                 mCameraDevice.setParameters(mParameters);

                 if (isSupported(faceDetection, mParameters.getSupportedFaceDetectionModes())) {
                     mCameraDevice.setFaceDetectionCb(null);
                     clearFaceRectangles();
                     mFaceDetect = FACE_DETECTION_OFF;
                 }
                 overrideAfHudSettings(null,
                         getString(R.string.pref_camera_facedetection_default));

             } else {
                 overrideAfHudSettings(null, null);
             }
         }
    }

    private void overrideSkinToneSaturationHudSettings(final String saturation) {
        mHeadUpDisplay.overrideSettings(
                CameraSettings.KEY_SATURATION, saturation);
    }
    private void overrideSkinToneWhiteBalanceHudSettings(final String whiteBalance) {
        mHeadUpDisplay.overrideSettings(
                CameraSettings.KEY_WHITE_BALANCE, whiteBalance);
    }

   private void updateSkinToneSettingsInHud() {
       if (mSkinToneSeekBar == true) {
            overrideSkinToneWhiteBalanceHudSettings(getString
                       (R.string.pref_camera_whitebalance_default));
            overrideSkinToneSaturationHudSettings(getString
                       (R.string.pref_camera_saturation_default));
        } else {
           if (Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
               overrideSkinToneWhiteBalanceHudSettings(null);
            overrideSkinToneSaturationHudSettings(null);
           }else{
             overrideSkinToneWhiteBalanceHudSettings(getString
                        (R.string.pref_camera_whitebalance_default));
            overrideSkinToneSaturationHudSettings(null);
           }
        }
    }

    private void initializeHeadUpDisplay() {
        CameraSettings settings = new CameraSettings(this, mInitialParams,
                CameraHolder.instance().getCameraInfo());
        mHeadUpDisplay.initialize(this,
                settings.getPreferenceGroup(R.xml.camera_preferences),
                getZoomRatios(), mOrientationCompensation);
        if (mParameters.isZoomSupported()) {
            mHeadUpDisplay.setZoomListener(new ZoomControllerListener() {
                public void onZoomChanged(
                        int index, float ratio, boolean isMoving) {
                    onZoomValueChanged(index);
                }
            });
        }
        updateSceneModeInHud();
        updateAfSettingsInHud();
        updateColorEffectSettingsInHud();
        updateSkinToneSettingsInHud();
    }

    private void attachHeadUpDisplay() {
        mHeadUpDisplay.setOrientation(mOrientationCompensation);
        if (mParameters.isZoomSupported()) {
            mHeadUpDisplay.setZoomIndex(mZoomValue);
        }
        FrameLayout frame = (FrameLayout) findViewById(R.id.frame);
        mGLRootView = new GLRootView(this);
        mGLRootView.setContentPane(mHeadUpDisplay);
        frame.addView(mGLRootView);
    }

    private void detachHeadUpDisplay() {
        mHeadUpDisplay.setGpsHasSignal(false);
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
            // We keep the last known orientation. So if the user first orient
            // the camera then point the camera to floor or sky, we still have
            // the correct orientation.
            if (orientation == ORIENTATION_UNKNOWN) return;
            mOrientation = roundOrientation(orientation);
            // When the screen is unlocked, display rotation may change. Always
            // calculate the up-to-date orientationCompensation.
            int orientationCompensation = mOrientation
                    + Util.getDisplayRotation(Camera.this);
            if (mOrientationCompensation != orientationCompensation) {
                mOrientationCompensation = orientationCompensation;
                if (!mIsImageCaptureIntent) {
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
    public void onStart() {
        super.onStart();
        if (!mIsImageCaptureIntent) {
            mSwitcher.setSwitch(SWITCH_CAMERA);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mMediaProviderClient != null) {
            mMediaProviderClient.release();
            mMediaProviderClient = null;
        }
    }

    private void checkStorage() {
        calculatePicturesRemaining();
        updateStorageHint(mPicturesRemaining);
    }

    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_retake:
                hidePostCaptureAlert();
                restartPreview();
                break;
            case R.id.review_thumbnail:
                if (isCameraIdle()) {
                    viewLastImage();
                }
                break;
            case R.id.btn_done:
                doAttach();
                break;
            case R.id.btn_cancel:
                doCancel();
        }
    }

    private Bitmap createCaptureBitmap(byte[] data) {
        // This is really stupid...we just want to read the orientation in
        // the jpeg header.
        String filepath = ImageManager.getTempJpegPath();
        int degree = 0;
        if (saveDataToFile(filepath, data)) {
            degree = ImageManager.getExifOrientation(filepath);
            new File(filepath).delete();
        }

        // Limit to 50k pixels so we can return it in the intent.
        Bitmap bitmap = Util.makeBitmap(data, 50 * 1024);
        bitmap = Util.rotate(bitmap, degree);
        return bitmap;
    }

    private void doAttach() {
        if (mPausing) {
            return;
        }

        byte[] data = mImageCapture.getLastCaptureData();

        if (mCropValue == null) {
            // First handle the no crop case -- just return the value.  If the
            // caller specifies a "save uri" then write the data to it's
            // stream. Otherwise, pass back a scaled down version of the bitmap
            // directly in the extras.
            if (mSaveUri != null) {
                OutputStream outputStream = null;
                try {
                    outputStream = mContentResolver.openOutputStream(mSaveUri);
                    outputStream.write(data);
                    outputStream.close();

                    String pictureFormat = mParameters.get(KEY_PICTURE_FORMAT);
                    setResult(RESULT_OK, (new Intent()).putExtra("picFormat",pictureFormat));
                    finish();
                } catch (IOException ex) {
                    // ignore exception
                } finally {
                    Util.closeSilently(outputStream);
                }
            } else {
                Bitmap bitmap = createCaptureBitmap(data);
                setResult(RESULT_OK,
                        new Intent("inline-data").putExtra("data", bitmap));
                finish();
            }
        } else {
            // Save the image to a temp file and invoke the cropper
            Uri tempUri = null;
            FileOutputStream tempStream = null;
            try {
                File path = getFileStreamPath(sTempCropFilename);
                path.delete();
                tempStream = openFileOutput(sTempCropFilename, 0);
                tempStream.write(data);
                tempStream.close();
                tempUri = Uri.fromFile(path);
            } catch (FileNotFoundException ex) {
                setResult(Activity.RESULT_CANCELED);
                finish();
                return;
            } catch (IOException ex) {
                setResult(Activity.RESULT_CANCELED);
                finish();
                return;
            } finally {
                Util.closeSilently(tempStream);
            }

            Bundle newExtras = new Bundle();
            if (mCropValue.equals("circle")) {
                newExtras.putString("circleCrop", "true");
            }
            if (mSaveUri != null) {
                newExtras.putParcelable(MediaStore.EXTRA_OUTPUT, mSaveUri);
            } else {
                newExtras.putBoolean("return-data", true);
            }

            Intent cropIntent = new Intent("com.android.camera.action.CROP");

            cropIntent.setData(tempUri);
            cropIntent.putExtras(newExtras);

            startActivityForResult(cropIntent, CROP_MSG);
        }
    }

    private void doCancel() {
        setResult(RESULT_CANCELED, new Intent());
        finish();
    }

    private synchronized void incrementkeypress() {
        if(keypresscount == 0)
            keypresscount++;
    }

    private synchronized void decrementkeypress() {
        if(keypresscount > 0)
            keypresscount--;
    }
    private synchronized int keypressvalue() {
        return keypresscount;
    }


    public void onShutterButtonFocus(ShutterButton button, boolean pressed) {
        if (mPausing) {
            return;
        }
       int keydown =  keypressvalue();
        if(keydown==0 && pressed)
         {
            keyup = 1;
            Log.v(TAG, "the keydown is  pressed first time");
            mShutterdownTime = System.currentTimeMillis();
         }
         else if(keyup==1 && !pressed)
         {
          Log.v(TAG, "the keyup is pressed first time ");
          keyup = 0;
         }
        switch (button.getId()) {
            case R.id.shutter_button:

                if ((mPreviewing) && (mFocusState != FOCUS_SNAP_IMPENDING)) {
                    cameraUtilProfile("focus button");
                    resetFocusIndicator();
                    doFocus(pressed);
                }
                break;
        }
    }

    public void onShutterButtonClick(ShutterButton button) {
        mShutterupTime = System.currentTimeMillis();
        if (mPausing) {
            return;
        }
        switch (button.getId()) {
            case R.id.shutter_button:
                doSnap();
                break;
        }
    }

    private OnScreenHint mStorageHint;

    private void updateStorageHint(int remaining) {
        String noStorageText = null;

        if (remaining == MenuHelper.NO_STORAGE_ERROR) {
            String state = Environment.getExternalStorageState();
            if (state == Environment.MEDIA_CHECKING) {
                noStorageText = getString(R.string.preparing_sd);
            } else {
                noStorageText = getString(R.string.no_storage);
            }
        } else if (remaining == MenuHelper.CANNOT_STAT_ERROR) {
            noStorageText = getString(R.string.access_sd_fail);
        } else if (remaining < 1) {
            noStorageText = getString(R.string.not_enough_space);
        }

        if (noStorageText != null) {
            if (mStorageHint == null) {
                mStorageHint = OnScreenHint.makeText(this, noStorageText);
            } else {
                mStorageHint.setText(noStorageText);
            }
            mStorageHint.show();
        } else if (mStorageHint != null) {
            mStorageHint.cancel();
            mStorageHint = null;
        }
    }

    private void installIntentFilter() {
        // install an intent filter to receive SD card related events.
        IntentFilter intentFilter =
                new IntentFilter(Intent.ACTION_MEDIA_MOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        intentFilter.addAction(Intent.ACTION_MEDIA_CHECKING);
        intentFilter.addDataScheme("file");
        registerReceiver(mReceiver, intentFilter);
        mDidRegister = true;
    }

    private void initializeFocusTone() {
        // Initialize focus tone generator.
        try {
            mFocusToneGenerator = new ToneGenerator(
                    AudioManager.STREAM_SYSTEM, FOCUS_BEEP_VOLUME);
        } catch (Throwable ex) {
            Log.w(TAG, "Exception caught while creating tone generator: ", ex);
            mFocusToneGenerator = null;
        }
    }

    private void initializeScreenBrightness() {
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
    }

    private void setSkinToneFactor(){
       if (skinToneSeekBar == null) return;

       String skinToneEnhancementPref = mPreferences.getString(
                CameraSettings.KEY_SKIN_TONE_ENHANCEMENT,
       getString(R.string.pref_camera_skinToneEnhancement_default));
       if(isSupported(skinToneEnhancementPref,
               mParameters.getSupportedSkinToneEnhancementModes())){
         if(skinToneEnhancementPref.equals("enable")) {
               enableSkinToneSeekBar();
          } else {
               disableSkinToneSeekBar();
          }
       } else {
          skinToneSeekBar.setVisibility(View.INVISIBLE);
       }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPausing = false;
        mJpegPictureCallbackTime = 0;
        mZoomValue = 0;
        mImageCapture = new ImageCapture();

        // Start the preview if it is not started.
        if (!mPreviewing && !mStartPreviewFail) {
            resetExposureCompensation();
            if (!restartPreview()) return;
        }

       //Check if the skinTone SeekBar could not be enabled during updateCameraParametersPreference()
       //due to the finite latency of loading the seekBar layout when switching modes
       // for same Camera Device instance
        if (mSkinToneSeekBar != true)
            mHandler.sendEmptyMessage(SET_SKIN_TONE_FACTOR);

        if (mSurfaceHolder != null) {
            // If first time initialization is not finished, put it in the
            // message queue.
            if (!mFirstTimeInitialized) {
                mHandler.sendEmptyMessage(FIRST_TIME_INIT);
            } else {
                initializeSecondTime();
            }
        }
        keepScreenOnAwhile();
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        changeHeadUpDisplayState();
    }

    private static ImageManager.DataLocation dataLocation() {
        return ImageManager.DataLocation.EXTERNAL;
    }

    @Override
    protected void onPause() {
        mPausing = true;
        SharedPreferences ShPrefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        SharedPreferences.Editor editor = ShPrefs.edit();
        editor.putInt("pref_camera_brightness_key", mbrightness);
        editor.putInt("pref_camera_skin_tone_enhancement_key", mskinToneValue);
        editor.commit();
        if(mGraphView != null)
          mGraphView.setCameraObject(null);
        stopPreview();
        // Close the camera now because other activities may need to use it.
        closeCamera();
        resetScreenOn();
        changeHeadUpDisplayState();

        if (mFirstTimeInitialized) {
            mOrientationListener.disable();
            if (!mIsImageCaptureIntent) {
                mThumbController.storeData(
                        ImageManager.getLastImageThumbPath());
            }
            hidePostCaptureAlert();
        }
        keypresscount = 0;
        if (mDidRegister) {
            unregisterReceiver(mReceiver);
            mDidRegister = false;
        }
        stopReceivingLocationUpdates();

        if (mFocusToneGenerator != null) {
            mFocusToneGenerator.release();
            mFocusToneGenerator = null;
        }

        if (mStorageHint != null) {
            mStorageHint.cancel();
            mStorageHint = null;
        }

        // If we are in an image capture intent and has taken
        // a picture, we just clear it in onPause.
        mImageCapture.clearLastData();
        mImageCapture = null;

        // Remove the messages in the event queue.
        mHandler.removeMessages(RESTART_PREVIEW);
        mHandler.removeMessages(FIRST_TIME_INIT);
        mHandler.removeMessages(CLEAR_FOCUS_INDICATOR);
        super.onPause();
    }

    @Override
    protected void onActivityResult(
            int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case CROP_MSG: {
                Intent intent = new Intent();
                if (data != null) {
                    Bundle extras = data.getExtras();
                    if (extras != null) {
                        intent.putExtras(extras);
                    }
                }
                setResult(resultCode, intent);
                finish();

                File path = getFileStreamPath(sTempCropFilename);
                path.delete();

                break;
            }
        }
    }

    private boolean canTakePicture() {
        return isCameraIdle() && mPreviewing && (mPicturesRemaining > 0)
            && (!mSmoothZoomSupported || (mSmoothZoomSupported && (mZoomState == ZOOM_STOPPED)));
    }

    protected android.hardware.Camera getCamera() {
        return mCameraDevice;

    }
    private void autoFocus() {
        // Initiate autofocus only when preview is started and snapshot is not
        // in progress.
        if (canTakePicture()) {
            mHeadUpDisplay.setEnabled(false);
            Log.v(TAG, "Start autofocus.");
            mFocusStartTime = System.currentTimeMillis();
            mFocusState = FOCUSING;
            cameraUtilProfile("Start autofocus");
            if(mFaceDetect == FACE_DETECTION_OFF)
                updateFocusIndicator();
            mCameraDevice.autoFocus(mAutoFocusCallback);
            cameraUtilProfile("post autofocus");
        }
    }

    private void cancelAutoFocus() {
        // User releases half-pressed focus key.
        if (mStatus != SNAPSHOT_IN_PROGRESS && (mFocusState == FOCUSING
                || mFocusState == FOCUS_SUCCESS || mFocusState == FOCUS_FAIL
                || mFocusState == FOCUS_SNAP_IMPENDING)) {
            Log.v(TAG, "Cancel autofocus.");
            mHeadUpDisplay.setEnabled(true);
            mCameraDevice.cancelAutoFocus();
        }
        if (mFocusState != FOCUSING_SNAP_ON_FINISH) {
            clearFocusState();
        }
    }

    private void clearFocusState() {
        mFocusState = FOCUS_NOT_STARTED;
        if(mFaceDetect == FACE_DETECTION_OFF)
            updateFocusIndicator();
    }

    private void clearFocusIndicator() {
        if(mFocusRectangle != null) {
            mFocusRectangle.clear();
        }
    }

    private String currentTouchAfAecMode() {
        if ( mCameraDevice == null) {
           return null;
        }

        mParameters = mCameraDevice.getParameters();
        String touchAfAec = mPreferences.getString(
                CameraSettings.KEY_TOUCH_AF_AEC,
                getString(R.string.pref_camera_touchafaec_default));
        if (isSupported(touchAfAec, mParameters.getSupportedTouchAfAec()))
            return touchAfAec;
        else
            return null;
    }

    private void clearFaceRectangles() {
        for (int i=0; i< MAX_FACES_DETECTED; i++) {
            if(mFaceRectangle[i] != null) {
                mFaceRectangle[i].clear();
            }
        }
    }

    private boolean isFaceRectangleValid() {
        for (int i=0; i< MAX_FACES_DETECTED; i++) {
            if(mFaceRectangle[i] == null) {
                return false;
            }
        }
        return true;
    }

    void transformFaceCoordinate(int []FaceCoordinate) {
        int x = FaceCoordinate[0]; //TBD may not need this added factor
        int y = FaceCoordinate[1];
        int x1 = FaceCoordinate[0] + FaceCoordinate[2];
        int y1 = FaceCoordinate[1] + FaceCoordinate[3];
        int SurfaceViewWidth = mSurfaceView.getWidth();
        int SurfaceViewHeight = mSurfaceView.getHeight();
        Size s = mParameters.getPreviewSize();

        FaceCoordinate[0] = (SurfaceViewWidth * x) / s.width;
        FaceCoordinate[1] = (SurfaceViewHeight * y) / s.height;
        FaceCoordinate[2] = (SurfaceViewWidth * x1) / s.width;
        FaceCoordinate[3] = (SurfaceViewHeight * y1) / s.height;
    }

    private void redrawFaceRectangles(int facemetadata[]){
        for (int i=0, j = 1; i< facesDetected; i++, j = j+ENTRIES_PER_FACE) {
                int [] FaceCoordinate = new int[ENTRIES_PER_FACE];
                FaceCoordinate[0] = facemetadata[j];
                FaceCoordinate[1] = facemetadata[j+1];
                FaceCoordinate[2] = facemetadata[j+2];
                FaceCoordinate[3] = facemetadata[j+3];
                transformFaceCoordinate(FaceCoordinate);
                mFaceRectangle[i].drawFaceRectangle(FaceCoordinate[0], FaceCoordinate[1],
                FaceCoordinate[2], FaceCoordinate[3]);
                mFaceRectangle[i].showSuccess();
        }
    }
    private void resetFocusIndicator() {
        if (mFocusRectangle == null) return;

        if(mFirstTimeInitialized) {
            PreviewFrameLayout frameLayout =
                     (PreviewFrameLayout) findViewById(R.id.frame_layout);
            int frameWidth = frameLayout.getActualWidth();
            int frameHeight = frameLayout.getActualHeight();
            String touchAfAec = currentTouchAfAecMode();

            if (touchAfAec!= null) {
                if (touchAfAec.equals(Parameters.TOUCH_AF_AEC_ON)) {
                    mFocusRectangle.setPosition(frameWidth/2, frameHeight/2);
                    mParameters.setTouchIndexAec(-1, -1);
                    mParameters.setTouchIndexAf(-1, -1);
                    mParameters.setTouchAfAec(touchAfAec);
                    mCameraDevice.setParameters(mParameters);
                }
                else {
                    mFocusRectangle.setPosition(frameWidth/2, frameHeight/2);
                }
            }
            else {
                    mFocusRectangle.setPosition(frameWidth/2, frameHeight/2);
            }
        }
    }

    private void updateFocusIndicator() {
        if (mFocusRectangle == null) return;

        if (mFocusState == FOCUSING || mFocusState == FOCUSING_SNAP_ON_FINISH) {
            mFocusRectangle.showStart();
        } else if (mFocusState == FOCUS_SUCCESS || mFocusState == FOCUS_SNAP_IMPENDING) {
            mFocusRectangle.showSuccess();
        } else if (mFocusState == FOCUS_FAIL || mFocusState == FOCUS_SNAP_IMPENDING) {
            mFocusRectangle.showFail();
        } else {
           mFocusRectangle.clear();
        }
    }

    @Override
    public void onBackPressed() {
        if (!isCameraIdle()) {
            // ignore backs while we're taking a picture
            return;
        } else if (mHeadUpDisplay == null || !mHeadUpDisplay.collapse()) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {

                    if (mFocusState != FOCUS_SNAP_IMPENDING) {
                        resetFocusIndicator();
                        doFocus(true);
                    }
                }
                return true;
            case KeyEvent.KEYCODE_CAMERA:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    doSnap();
                }
                return true;
        case KeyEvent.KEYCODE_DPAD_LEFT:
            if ( (mPreviewing) &&
                 (mFocusState != FOCUSING) &&
                 (mFocusState != FOCUSING_SNAP_ON_FINISH) &&
                 (mSkinToneSeekBar != true)) {
                if (mbrightness > MINIMUM_BRIGHTNESS) {
                    mbrightness-=mbrightness_step;

                    /* Set the "luma-adaptation" parameter */
                    mParameters = mCameraDevice.getParameters();
                    mParameters.set("luma-adaptation", String.valueOf(mbrightness));
                    mCameraDevice.setParameters(mParameters);
                }

                brightnessProgressBar.setProgress(mbrightness);
                brightnessProgressBar.setVisibility(View.VISIBLE);

            }
            break;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                if ( (mPreviewing) &&
                     (mFocusState != FOCUSING) &&
                     (mFocusState != FOCUSING_SNAP_ON_FINISH) &&
                     (mSkinToneSeekBar != true)) {
                    if (mbrightness < MAXIMUM_BRIGHTNESS) {
                        mbrightness+=mbrightness_step;

                        /* Set the "luma-adaptation" parameter */
                        mParameters = mCameraDevice.getParameters();
                        mParameters.set("luma-adaptation", String.valueOf(mbrightness));
                        mCameraDevice.setParameters(mParameters);
                }
                    brightnessProgressBar.setProgress(mbrightness);
                    brightnessProgressBar.setVisibility(View.VISIBLE);

                }
                break;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If we get a dpad center event without any focused view, move
                // the focus to the shutter button and press it.
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    // Start auto-focus immediately to reduce shutter lag. After
                    // the shutter button gets the focus, doFocus() will be
                    // called again but it is fine.
                    if (mHeadUpDisplay.collapse()) return true;

                    if (mFocusState != FOCUS_SNAP_IMPENDING) {
                        resetFocusIndicator();
                        doFocus(true);
                    }
                    if (mShutterButton.isInTouchMode()) {
                        mShutterButton.requestFocusFromTouch();
                    } else {
                        mShutterButton.requestFocus();
                    }
                    mShutterButton.setPressed(true);
                }
                return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized) {
                    doFocus(false);
                }
                return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    private void doSnap() {
        // Check if the storage is available before do snapshot
        if (mPicturesRemaining <= 0) return;
        if (mHeadUpDisplay.collapse()) return;
        if(mUnsupportedJpegQuality == true) {
            Toast toast = Toast.makeText(this, R.string.error_app_unsupported_jpeg,
                    Toast.LENGTH_SHORT);
            toast.show();
            Log.v(TAG, "Unsupported jpeg quality for this picture size");
            return;
        }
        Log.v(TAG, "doSnap: mFocusState=" + mFocusState);
        // If the user has half-pressed the shutter and focus is completed, we
        // can take the photo right away. If the focus mode is infinity, we can
        // also take the photo.
        if (mFocusMode.equals(Parameters.FOCUS_MODE_INFINITY)
                || mFocusMode.equals(Parameters.FOCUS_MODE_FIXED)
                || mFocusMode.equals(Parameters.FOCUS_MODE_EDOF)
                || (mFocusState == FOCUS_SUCCESS
                || mFocusState == FOCUS_FAIL || mFocusState == FOCUS_SNAP_IMPENDING)) {
            mImageCapture.onSnap();
        } else if (mFocusState == FOCUSING) {
            // Half pressing the shutter (i.e. the focus button event) will
            // already have requested AF for us, so just request capture on
            // focus here.
            mFocusState = FOCUSING_SNAP_ON_FINISH;
        } else if (mFocusState == FOCUS_NOT_STARTED) {
            // Focus key down event is dropped for some reasons. Just ignore.
        }
    }

    private void doFocus(boolean pressed) {
        // Do the focus if the mode is not infinity.
        if (mHeadUpDisplay.collapse()) return;

        if(mFaceDetect == FACE_DETECTION_ON) {
            mFaceDetect = FACE_DETECTION_CB_DISABLED;
            mCameraDevice.setFaceDetectionCb(null);
            clearFaceRectangles();
        }

        if (!(mFocusMode.equals(Parameters.FOCUS_MODE_INFINITY)
                  || mFocusMode.equals(Parameters.FOCUS_MODE_FIXED)
                  || mFocusMode.equals(Parameters.FOCUS_MODE_EDOF))) {
            if (pressed) {  // Focus key down.
                autoFocus();
            } else {  // Focus key up.
                cancelAutoFocus();
            }
        }
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        // Make sure we have a surface in the holder before proceeding.
        if (holder.getSurface() == null) {
            Log.d(TAG, "holder.getSurface() == null");
            return;
        }

        // We need to save the holder for later use, even when the mCameraDevice
        // is null. This could happen if onResume() is invoked after this
        // function.
        mSurfaceHolder = holder;

        // The mCameraDevice will be null if it fails to connect to the camera
        // hardware. In this case we will show a dialog and then finish the
        // activity, so it's OK to ignore it.
        if (mCameraDevice == null) return;

        // Sometimes surfaceChanged is called after onPause or before onResume.
        // Ignore it.
        if (mPausing || isFinishing()) return;

        if (mPreviewing && holder.isCreating()) {
            // Set preview display if the surface is being created and preview
            // was already started. That means preview display was set to null
            // and we need to set it now.
            setPreviewDisplay(holder);
        } else {
            // 1. Restart the preview if the size of surface was changed. The
            // framework may not support changing preview display on the fly.
            // 2. Start the preview now if surface was destroyed and preview
            // stopped.
            restartPreview();
        }

        // If first time initialization is not finished, send a message to do
        // it later. We want to finish surfaceChanged as soon as possible to let
        // user see preview first.
        if (!mFirstTimeInitialized) {
            mHandler.sendEmptyMessage(FIRST_TIME_INIT);
        } else {
            initializeSecondTime();
        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        stopPreview();
        mSurfaceHolder = null;
    }

    private void closeCamera() {
        if (mCameraDevice != null) {
            CameraHolder.instance().release();
            mCameraDevice.setZoomChangeListener(null);
            mCameraDevice = null;
            mPreviewing = false;
        }
    }

    private void ensureCameraDevice() throws CameraHardwareException {
        if (mCameraDevice == null) {
            cameraUtilProfile("open camera");
            int cameraMode = mCameraMode | mSnapshotMode;
            Log.v(TAG, "opening camera device with mode: " + cameraMode);
            mCameraDevice = CameraHolder.instance().open(mCameraId, cameraMode);
            cameraUtilProfile("get params");
            mInitialParams = mCameraDevice.getParameters();
        }
        cameraUtilProfile("camera opended");
    }

    private void updateLastImage() {
        IImageList list = ImageManager.makeImageList(
            mContentResolver,
            dataLocation(),
            ImageManager.INCLUDE_IMAGES,
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

    private void showCameraErrorAndFinish() {
        Resources ress = getResources();
        Util.showFatalErrorAndFinish(Camera.this,
                ress.getString(R.string.camera_error_title),
                ress.getString(R.string.cannot_connect_camera));
    }

    private void showCameraStoppedAndFinish() {
        Resources ress = getResources();
        Util.showFatalErrorAndFinish(Camera.this,
                ress.getString(R.string.camera_application_stopped),
                ress.getString(R.string.camera_driver_needs_reset));
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

    private void setPreviewDisplay(SurfaceHolder holder) {
        try {
            mCameraDevice.setPreviewDisplay(holder);
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("setPreviewDisplay failed", ex);
        }
    }

    private void startPreview() throws CameraHardwareException {
        if (mPausing || isFinishing()) return;

        cameraUtilProfile("start preview & set parms");
        ensureCameraDevice();

        // If we're previewing already, stop the preview first (this will blank
        // the screen).
        if (mPreviewing) stopPreview();

        setPreviewDisplay(mSurfaceHolder);
        Util.setCameraDisplayOrientation(this, mCameraId, mCameraDevice);
        setCameraParameters(UPDATE_PARAM_ALL);

        mCameraDevice.setErrorCallback(mErrorCallback);

        try {
            cameraUtilProfile("start preview");
            Log.v(TAG, "startPreview");
            mCameraDevice.startPreview();
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("startPreview failed", ex);
        }
        mPreviewing = true;
        mZoomState = ZOOM_STOPPED;
        mStatus = IDLE;

        /* Get the correct max zoom value, as this varies with
        * preview size/picture resolution
        */
        mParameters = mCameraDevice.getParameters();
        mZoomMax = mParameters.getMaxZoom();
    }

    private void stopPreview() {
        if (mCameraDevice != null) {
            Log.v(TAG, "Stop Histogram if enabled");
            if(mHiston)
                mCameraDevice.setHistogramMode(null);
            if(mFaceDetect == FACE_DETECTION_ON) {
                mCameraDevice.setFaceDetectionCb(null);
                clearFaceRectangles();
            }
            Log.v(TAG, "stopPreview");
            if(mPreviewing)
                mCameraDevice.stopPreview();
        }
        mPreviewing = false;
        mHiston = false;
        mFaceDetect = FACE_DETECTION_OFF;
        // If auto focus was in progress, it would have been canceled.
        clearFocusState();
    }

    private Size getOptimalPreviewSize(List<Size> sizes, double targetRatio) {
        final double ASPECT_TOLERANCE = 0.05;
        if (sizes == null) return null;

        Size optimalSize = null;
        double minDiff = Double.MAX_VALUE;

        // Because of bugs of overlay and layout, we sometimes will try to
        // layout the viewfinder in the portrait orientation and thus get the
        // wrong size of mSurfaceView. When we change the preview size, the
        // new overlay will be created before the old one closed, which causes
        // an exception. For now, just get the screen size

        Display display = getWindowManager().getDefaultDisplay();
        int targetHeight = Math.min(display.getHeight(), display.getWidth());

        if (targetHeight <= 0) {
            // We don't know the size of SurefaceView, use screen height
            WindowManager windowManager = (WindowManager)
                    getSystemService(Context.WINDOW_SERVICE);
            targetHeight = windowManager.getDefaultDisplay().getHeight();
        }

        // Try to find an size match aspect ratio and size
        for (Size size : sizes) {
            double ratio = (double) size.width / size.height;
            if (Math.abs(ratio - targetRatio) > ASPECT_TOLERANCE) continue;
            if (Math.abs(size.height - targetHeight) < minDiff) {
                optimalSize = size;
                minDiff = Math.abs(size.height - targetHeight);
            }
        }

        // Cannot find the one match the aspect ratio, ignore the requirement
        if (optimalSize == null) {
            Log.v(TAG, "No preview size match the aspect ratio");
            minDiff = Double.MAX_VALUE;
            for (Size size : sizes) {
                if (Math.abs(size.height - targetHeight) < minDiff) {
                    optimalSize = size;
                    minDiff = Math.abs(size.height - targetHeight);
                }
            }
        }
        return optimalSize;
    }

    private static boolean isSupported(String value, List<String> supported) {
        return supported == null ? false : supported.indexOf(value) >= 0;
    }

    private void updateCameraParametersInitialize() {
        // Reset preview frame rate to the maximum because it may be lowered by
        // video camera application.
        List<Integer> frameRates = mParameters.getSupportedPreviewFrameRates();
        if (frameRates != null) {
            Integer max = Collections.max(frameRates);
            mParameters.setPreviewFrameRate(max);
        }

    }

    private void updateCameraParametersZoom() {
        // Set zoom.
        if (mParameters.isZoomSupported()) {
            mParameters.setZoom(mZoomValue);
        }
    }

    private void enableSkinToneSeekBar() {
        int progress;
        if(brightnessProgressBar != null)
           brightnessProgressBar.setVisibility(View.INVISIBLE);
        skinToneSeekBar.setMax(MAX_SCE_FACTOR-MIN_SCE_FACTOR);
        skinToneSeekBar.setVisibility(View.VISIBLE);
        skinToneSeekBar.requestFocus();
        if (mskinToneValue != 0) {
            progress = (mskinToneValue/SCE_FACTOR_STEP)-MIN_SCE_FACTOR;
            mskinToneSeekListener.onProgressChanged(skinToneSeekBar,progress,false);
        }else {
            progress = (MAX_SCE_FACTOR-MIN_SCE_FACTOR)/2;
            RightValue.setText("");
            LeftValue.setText("");
        }
        skinToneSeekBar.setProgress(progress);
        Title.setText("Skin Tone Enhancement");
        Title.setVisibility(View.VISIBLE);
        RightValue.setVisibility(View.VISIBLE);
        LeftValue.setVisibility(View.VISIBLE);
        mSkinToneSeekBar = true;
    }

    private void disableSkinToneSeekBar() {
         skinToneSeekBar.setVisibility(View.INVISIBLE);
         Title.setVisibility(View.INVISIBLE);
         RightValue.setVisibility(View.INVISIBLE);
         LeftValue.setVisibility(View.INVISIBLE);
         mskinToneValue = 0;
         mSkinToneSeekBar = false;
        if(brightnessProgressBar != null && mBrightnessVisible)
           brightnessProgressBar.setVisibility(View.VISIBLE);
    }
    private void updateCameraParametersPreference() {
        // Set picture size.
        String pictureSize = mPreferences.getString(
                CameraSettings.KEY_PICTURE_SIZE, null);
        if (pictureSize == null) {
            CameraSettings.initialCameraPictureSize(this, mParameters);
        } else {
            List<Size> supported = mParameters.getSupportedPictureSizes();
            CameraSettings.setCameraPictureSize(
                    pictureSize, supported, mParameters);
        }

        // Set the preview frame aspect ratio according to the picture size.
        Size size = mParameters.getPictureSize();
        PreviewFrameLayout frameLayout =
                (PreviewFrameLayout) findViewById(R.id.frame_layout);
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        int degrees = Util.getDisplayRotation(this);

        // If Camera orientation and display Orientation is not aligned,
        // FrameLayout's Aspect Ration needs to be rotated
        if(((info.orientation - degrees + 360)%180) == 90) {
            frameLayout.setAspectRatio(((double) size.height / size.width), mSnapshotMode);
        } else {
            frameLayout.setAspectRatio(((double) size.width / size.height), mSnapshotMode);
        }

        // Set a preview size that is closest to the viewfinder height and has
        // the right aspect ratio.
        List<Size> sizes = mParameters.getSupportedPreviewSizes();
        Size optimalSize = getOptimalPreviewSize(
                sizes, (double) size.width / size.height);
        if (optimalSize != null) {
            Size original = mParameters.getPreviewSize();
            if (!original.equals(optimalSize)) {
                mParameters.setPreviewSize(optimalSize.width, optimalSize.height);

                // Zoom related settings will be changed for different preview
                // sizes, so set and read the parameters to get lastest values
                mCameraDevice.setParameters(mParameters);
                mParameters = mCameraDevice.getParameters();
            }
        }

        // Since change scene mode may change supported values,
        // Set scene mode first,
        mSceneMode = mPreferences.getString(
                CameraSettings.KEY_SCENE_MODE,
                getString(R.string.pref_camera_scenemode_default));
        if (isSupported(mSceneMode, mParameters.getSupportedSceneModes())) {
            if (!mParameters.getSceneMode().equals(mSceneMode)) {
                mParameters.setSceneMode(mSceneMode);
                mCameraDevice.setParameters(mParameters);

                // Setting scene mode will change the settings of flash mode,
                // white balance, and focus mode. Here we read back the
                // parameters, so we can know those settings.
                mParameters = mCameraDevice.getParameters();
            }
        } else {
            mSceneMode = mParameters.getSceneMode();
            if (mSceneMode == null) {
                mSceneMode = Parameters.SCENE_MODE_AUTO;
            }
        }

        // Set JPEG quality.
        String jpegQuality = mPreferences.getString(
                CameraSettings.KEY_JPEG_QUALITY,
                getString(R.string.pref_camera_jpegquality_default));

        mUnsupportedJpegQuality = false;
        Size pic_size = mParameters.getPictureSize();
        if("100".equals(jpegQuality) && (pic_size.width >= 3200)){
            mUnsupportedJpegQuality = true;
        }else {
            mParameters.setJpegQuality(JpegEncodingQualityMappings.getQualityNumber(jpegQuality));
        }


        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

         // Set ISO parameter.
        String iso = mPreferences.getString(
                CameraSettings.KEY_ISO,
                getString(R.string.pref_camera_iso_default));
        if (isSupported(iso,
                mParameters.getSupportedIsoValues())) {
                mParameters.setISOValue(iso);
         }

        //Set LensShading
        String lensshade = mPreferences.getString(
                CameraSettings.KEY_LENSSHADING,
                getString(R.string.pref_camera_lensshading_default));
        if (isSupported(lensshade,
                mParameters.getSupportedLensShadeModes())) {
                mParameters.setLensShade(lensshade);
        }

        //Set Memory Color Enhancement
        String mce = mPreferences.getString(
                CameraSettings.KEY_MEMORY_COLOR_ENHANCEMENT,
                getString(R.string.pref_camera_mce_default));
        if (isSupported(mce,
                mParameters.getSupportedMemColorEnhanceModes())) {
                mParameters.setMemColorEnhance(mce);
        }

        // Set Redeye Reduction
        String redeyeReduction = mPreferences.getString(
                CameraSettings.KEY_REDEYE_REDUCTION,
                getString(R.string.pref_camera_redeyereduction_default));
        if (isSupported(redeyeReduction,
            mParameters.getSupportedRedeyeReductionModes())) {
            mParameters.setRedeyeReductionMode(redeyeReduction);
        }

        //Set Histogram
        String histogram = mPreferences.getString(
                CameraSettings.KEY_HISTOGRAM,
                getString(R.string.pref_camera_histogram_default));
        if (isSupported(histogram,
                mParameters.getSupportedHistogramModes()) && mCameraDevice != null) {
                // Call for histogram
                if(histogram.equals("enable")) {
                    if(mGraphView != null) {
                        mGraphView.setVisibility(View.VISIBLE);
                    }
                    mCameraDevice.setHistogramMode(mStatsCallback);
                    mHiston = true;

                } else {
                    mHiston = false;
                    if(mGraphView != null)
                        mGraphView.setVisibility(View.INVISIBLE);
                     mCameraDevice.setHistogramMode(null);
                }
        }
        //Set Brightness.
        mParameters.set("luma-adaptation", String.valueOf(mbrightness));

         // Set auto exposure parameter.
         String autoExposure = mPreferences.getString(
                 CameraSettings.KEY_AUTOEXPOSURE,
                 getString(R.string.pref_camera_autoexposure_default));
         if (isSupported(autoExposure, mParameters.getSupportedAutoexposure())) {
             mParameters.setAutoExposure(autoExposure);
         }

         // Set Touch AF/AEC parameter.
         String touchAfAec = mPreferences.getString(
                 CameraSettings.KEY_TOUCH_AF_AEC,
                 getString(R.string.pref_camera_touchafaec_default));
         if (isSupported(touchAfAec, mParameters.getSupportedTouchAfAec())) {
             if (touchAfAec.equals(Parameters.TOUCH_AF_AEC_ON)) {
                 if (mFocusGestureDetector == null) {
                     mFocusGestureDetector = new GestureDetector(this, new FocusGestureListener(), mHandler);

                     //Setting the touch index to negative value to
                     //indicate that a touch event has not occured yet
                     mParameters.setTouchIndexAec(-1,-1);
                     mParameters.setTouchIndexAf(-1,-1);
                     mParameters.set("touchAfAec-dx",String.valueOf(100));
                     mParameters.set("touchAfAec-dy",String.valueOf(100));
                 }
             }
             else {
                 if (mFocusRectangle != null)
                     mFocusRectangle.clear();
                 mFocusGestureDetector = null;
             }
             mParameters.setTouchAfAec(touchAfAec);
         }

         // Set Selectable Zone Af parameter.
         String selectableZoneAf = mPreferences.getString(
             CameraSettings.KEY_SELECTABLE_ZONE_AF,
             getString(R.string.pref_camera_selectablezoneaf_default));
         List<String> str = mParameters.getSupportedSelectableZoneAf();
         if (isSupported(selectableZoneAf, mParameters.getSupportedSelectableZoneAf())) {
             mParameters.setSelectableZoneAf(selectableZoneAf);
         }

         // Set face detetction parameter.
         String faceDetection = mPreferences.getString(
             CameraSettings.KEY_FACE_DETECTION,
             getString(R.string.pref_camera_facedetection_default));

         if (isSupported(faceDetection, mParameters.getSupportedFaceDetectionModes())) {
             mParameters.setFaceDetectionMode(faceDetection);
             if(faceDetection.equals(Parameters.FACE_DETECTION_ON)) {
                 mCameraDevice.setFaceDetectionCb(mMetaDataCallback);
                 mFaceDetect = FACE_DETECTION_ON;
             } else {
                 mCameraDevice.setFaceDetectionCb(null);
                 clearFaceRectangles();
                 mFaceDetect = FACE_DETECTION_OFF;
             }
         }

        // Set sharpness parameter.
        String sharpnessStr = mPreferences.getString(
                CameraSettings.KEY_SHARPNESS,
                getString(R.string.pref_camera_sharpness_default));
        int sharpness = Integer.parseInt(sharpnessStr) *
                (mParameters.getMaxSharpness()/MAX_SHARPNESS_LEVEL);
        if((0 <= sharpness) &&
                (sharpness <= mParameters.getMaxSharpness()))
            mParameters.setSharpness(sharpness);


        // Set contrast parameter.
        String contrastStr = mPreferences.getString(
                CameraSettings.KEY_CONTRAST,
                getString(R.string.pref_camera_contrast_default));
        int contrast = Integer.parseInt(contrastStr);
        if((0 <= contrast) &&
                (contrast <= mParameters.getMaxContrast()))
            mParameters.setContrast(contrast);



         // Set anti banding parameter.
         String antiBanding = mPreferences.getString(
                 CameraSettings.KEY_ANTIBANDING,
                 getString(R.string.pref_camera_antibanding_default));
         if (isSupported(antiBanding, mParameters.getSupportedAntibanding())) {
             mParameters.setAntibanding(antiBanding);
         }

        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

        // Set color effect parameter.
        String colorEffect = mPreferences.getString(
                CameraSettings.KEY_COLOR_EFFECT,
                getString(R.string.pref_camera_coloreffect_default));
        if (isSupported(colorEffect, mParameters.getSupportedColorEffects())) {
            mParameters.setColorEffect(colorEffect);
        }

        // Set exposure compensation
        String exposure = mPreferences.getString(
                CameraSettings.KEY_EXPOSURE,
                getString(R.string.pref_exposure_default));
        try {
            int value = Integer.parseInt(exposure);
            int max = mParameters.getMaxExposureCompensation();
            int min = mParameters.getMinExposureCompensation();
            if (value >= min && value <= max) {
                mParameters.setExposureCompensation(value);
            } else {
                Log.w(TAG, "invalid exposure range: " + exposure);
            }
        } catch (NumberFormatException e) {
            Log.w(TAG, "invalid exposure: " + exposure);
        }

        //Clearing previous GPS data if any
        if(mRecordLocation) {
            //Reset the values when store location is selected
            mParameters.setGpsLatitude(0);
            mParameters.setGpsLongitude(0);
            mParameters.setGpsAltitude(0);
            mParameters.setGpsTimestamp(0);
        }

        // Set Picture Format
        // Picture Formats specified in UI should be consistent with
        // PIXEL_FORMAT_JPEG and PIXEL_FORMAT_RAW constants
        String pictureFormat = mPreferences.getString(
                CameraSettings.KEY_PICTURE_FORMAT,
                getString(R.string.pref_camera_picture_format_default));
        mParameters.set(KEY_PICTURE_FORMAT, pictureFormat);

        if (mHeadUpDisplay != null) {
            updateSceneModeInHud();
            updateAfSettingsInHud();
            updateColorEffectSettingsInHud();
        }

        if (Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            // Set flash mode.
            String flashMode = mPreferences.getString(
                    CameraSettings.KEY_FLASH_MODE,
                    getString(R.string.pref_camera_flashmode_default));
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
            // Set Wavelet denoise mode
            if (mParameters.getSupportedDenoiseModes() != null) {
                String Denoise = mPreferences.getString( CameraSettings.KEY_DENOISE,
                                 getString(R.string.pref_camera_denoise_default));
                                 mParameters.setDenoise(Denoise);
            }
            // Set focus mode.
            mFocusMode = mPreferences.getString(
                    CameraSettings.KEY_FOCUS_MODE,
                    getString(R.string.pref_camera_focusmode_default));
            if (isSupported(mFocusMode, mParameters.getSupportedFocusModes())) {
                mParameters.setFocusMode(mFocusMode);
            } else {
                mFocusMode = mParameters.getFocusMode();
                if (mFocusMode == null) {
                    mFocusMode = Parameters.FOCUS_MODE_AUTO;
                }
            }

            // Set auto scene detect.
            String sceneDetect = mPreferences.getString(
                    CameraSettings.KEY_SCENE_DETECT,
                    getString(R.string.pref_camera_scenedetect_default));
            if (isSupported(sceneDetect, mParameters.getSupportedSceneDetectModes())) {
                mParameters.setSceneDetectMode(sceneDetect);
            } else {
                sceneDetect = mParameters.getSceneDetectMode();
                if (sceneDetect == null) {
                    sceneDetect = Parameters.SCENE_DETECT_OFF;
                }
            }

        } else {
            mFocusMode = mParameters.getFocusMode();
        }

        // skin tone ie enabled only for auto,party and portrait BSM
        // when color effects are not enabled
        if((Parameters.SCENE_MODE_AUTO.equals(mSceneMode) ||
            Parameters.SCENE_MODE_PARTY.equals(mSceneMode) |
            Parameters.SCENE_MODE_PORTRAIT.equals(mSceneMode))&&
            (Parameters.EFFECT_NONE.equals(colorEffect))) {
            //Set Skin Tone Correction factor
            if(mSeekBarInitialized == true)
            setSkinToneFactor();
        }
        if (mGLRootView != null)
            updateSkinToneSettingsInHud();

        if (mSkinToneSeekBar == false) {
           // Set saturation parameter.
            String saturationStr = mPreferences.getString(
                CameraSettings.KEY_SATURATION,
                getString(R.string.pref_camera_saturation_default));
            int saturation = Integer.parseInt(saturationStr);
            if((0 <= saturation) &&
                (saturation <= mParameters.getMaxSaturation()))
            mParameters.setSaturation(saturation);
        }

    }

    // We separate the parameters into several subsets, so we can update only
    // the subsets actually need updating. The PREFERENCE set needs extra
    // locking because the preference can be changed from GLThread as well.
    private void setCameraParameters(int updateSet) {
        mParameters = mCameraDevice.getParameters();

        if ((updateSet & UPDATE_PARAM_INITIALIZE) != 0) {
            updateCameraParametersInitialize();
        }

        if ((updateSet & UPDATE_PARAM_ZOOM) != 0) {
            updateCameraParametersZoom();
        }

        if ((updateSet & UPDATE_PARAM_PREFERENCE) != 0) {
            updateCameraParametersPreference();
        }
        mCameraDevice.setParameters(mParameters);
    }

    // If the Camera is idle, update the parameters immediately, otherwise
    // accumulate them in mUpdateSet and update later.
    private void setCameraParametersWhenIdle(int additionalUpdateSet) {
        mUpdateSet |= additionalUpdateSet;
        if (mCameraDevice == null) {
            // We will update all the parameters when we open the device, so
            // we don't need to do anything now.
            mUpdateSet = 0;
            return;
        } else if (isCameraIdle()) {
            setCameraParameters(mUpdateSet);
            mUpdateSet = 0;
        } else {
            if (!mHandler.hasMessages(SET_CAMERA_PARAMETERS_WHEN_IDLE)) {
                mHandler.sendEmptyMessageDelayed(
                        SET_CAMERA_PARAMETERS_WHEN_IDLE, 1000);
            }
        }
    }

    private void gotoGallery() {
        MenuHelper.gotoCameraImageGallery(this);
    }

    private void viewLastImage() {
        if (mThumbController.isUriValid()) {
            Intent intent = new Intent(Util.REVIEW_ACTION, mThumbController.getUri());
            try {
                startActivity(intent);
            } catch (ActivityNotFoundException ex) {
                try {
                    intent = new Intent(Intent.ACTION_VIEW, mThumbController.getUri());
                    startActivity(intent);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "review image fail", e);
                }
            }
        } else {
            Log.e(TAG, "Can't view last image.");
        }
    }

    void transformCoordinate(float []Coordinate, int []SurfaceViewLocation) {
        float x = Coordinate[0] - SurfaceViewLocation[0];
        float y = Coordinate[1] - SurfaceViewLocation[1];

        int SurfaceViewWidth = mSurfaceView.getWidth();
        int SurfaceViewHeight = mSurfaceView.getHeight();
        Size s = mParameters.getPreviewSize();

        Coordinate[0] = (s.width * x) / SurfaceViewWidth;
        Coordinate[1] = (s.height * y) / SurfaceViewHeight;
    }

    private class FocusGestureListener extends
            GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            if (!mPausing && isCameraIdle() && mPreviewing
                    && mFocusRectangle != null) {
                 float [] Coordinate = new float[2];
                 Coordinate[0] = e.getX();
                 Coordinate[1] = e.getY();

                 int [] SurfaceViewLocation = new int[2];
                 mSurfaceView.getLocationOnScreen(SurfaceViewLocation);

                 if (Coordinate[0] <= SurfaceViewLocation[0]
                     || Coordinate[1] <= SurfaceViewLocation[1]
                     || Coordinate[0] >= mSurfaceView.getWidth()
                     || Coordinate[1] >= mSurfaceView.getHeight()) {
                     return true;
                 }

                 //Scale the touch co-ordinates to match the current preview size
                 transformCoordinate(Coordinate, SurfaceViewLocation);

                 //Pass the actual touch co-ordinated to display focus rectangle
                 mFocusRectangle.setPosition((int)e.getX() - SurfaceViewLocation[0],
                                             (int)e.getY() - SurfaceViewLocation[1]);
                 mFocusRectangle.showStart();

                 mParameters = mCameraDevice.getParameters();
                 mParameters.setTouchIndexAec((int)Coordinate[0], (int)Coordinate[1]);
                 mParameters.setTouchIndexAf((int)Coordinate[0], (int)Coordinate[1]);
                 mCameraDevice.setParameters(mParameters);

                 mFocusRectangle.showSuccess();
                 doFocus(true);
           }
           return true;
        }
    }

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

    private boolean isCameraIdle() {
        return (mStatus == IDLE)
               && (mFocusState == FOCUS_NOT_STARTED
                   || mFocusState == FOCUS_SNAP_IMPENDING);
    }

    private boolean isImageCaptureIntent() {
        String action = getIntent().getAction();
        return (MediaStore.ACTION_IMAGE_CAPTURE.equals(action));
    }

    private void setupCaptureParams() {
        Bundle myExtras = getIntent().getExtras();
        if (myExtras != null) {
            mSaveUri = (Uri) myExtras.getParcelable(MediaStore.EXTRA_OUTPUT);
            mCropValue = myExtras.getString("crop");
        }
    }

    private void showPostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            findViewById(R.id.shutter_button).setVisibility(View.INVISIBLE);
            int[] pickIds = {R.id.btn_retake, R.id.btn_done};
            for (int id : pickIds) {
                View button = findViewById(id);
                ((View) button.getParent()).setVisibility(View.VISIBLE);
            }
        }
    }

    private void hidePostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            findViewById(R.id.shutter_button).setVisibility(View.VISIBLE);
            int[] pickIds = {R.id.btn_retake, R.id.btn_done};
            for (int id : pickIds) {
                View button = findViewById(id);
                ((View) button.getParent()).setVisibility(View.GONE);
            }
        }
    }

    private int calculatePicturesRemaining() {
        mPicturesRemaining = MenuHelper.calculatePicturesRemaining();
        return mPicturesRemaining;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        // Only show the menu when camera is idle.
        for (int i = 0; i < menu.size(); i++) {
            menu.getItem(i).setVisible(isCameraIdle());
        }

        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);

        if (mIsImageCaptureIntent) {
            // No options menu for attach mode.
            return false;
        } else {
            addBaseMenuItems(menu);
        }
        return true;
    }

    private void addBaseMenuItems(Menu menu) {
        MenuHelper.addSwitchModeMenuItem(menu, true, new Runnable() {
            public void run() {
                switchToVideoMode();
            }
        });
        MenuItem gallery = menu.add(Menu.NONE, Menu.NONE,
                MenuHelper.POSITION_GOTO_GALLERY,
                R.string.camera_gallery_photos_text)
                .setOnMenuItemClickListener(new OnMenuItemClickListener() {
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
        addSnapshotModeSwitchMenu();
    }

    private void addSnapshotModeSwitchMenu() {
        mHasZSLSupport = CameraHolder.instance().hasZSLSupport(mCameraId);
        if(mHasZSLSupport == true) {
            int labelId = R.string.switch_snapshot_mode_lable;
            int iconId = android.R.drawable.ic_menu_camera;
            mMenu.add(Menu.NONE, 200,
                    MenuHelper.POSITION_SWITCH_SNAPSHOT_MODES,
                    labelId)
                    .setOnMenuItemClickListener(new OnMenuItemClickListener() {
                public boolean onMenuItemClick(MenuItem item) {
                    switchSnapshotMode();
                    return true;
                }
            }).setIcon(iconId);
        }
    }

    private void switchSnapshotMode() {
        if(mSnapshotMode == CameraInfo.CAMERA_SUPPORT_MODE_ZSL)
            mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_NONZSL;
        else
            mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_ZSL;
        switchCameraId(mCameraId);

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
        if (mPausing || !isCameraIdle()) return;
        mCameraId = cameraId;
        CameraSettings.writePreferredCameraId(mPreferences, cameraId);
        boolean has3DSupport = CameraHolder.instance().has3DSupport(mCameraId);
        if(has3DSupport != true){
            mCameraMode = CameraInfo.CAMERA_SUPPORT_MODE_2D;
        }
        CameraSettings.writePreferredCameraMode(mPreferences, mCameraMode);

        boolean hasZSLSupport = CameraHolder.instance().hasZSLSupport(mCameraId);
        if(hasZSLSupport != true){
            mSnapshotMode = CameraInfo.CAMERA_SUPPORT_MODE_NONZSL;
        }
        CameraSettings.writePreferredSnapshotMode(mPreferences, mSnapshotMode);

        stopPreview();
        closeCamera();

        // Remove the messages in the event queue.
        mHandler.removeMessages(RESTART_PREVIEW);

        // Reset variables
        mJpegPictureCallbackTime = 0;
        mZoomValue = 0;

        if(has3DSupport == true) {
            if((mMenu != null) && (mMenu.findItem(100) == null))
                addModeSwitchMenu();
        }else{
            if((mMenu != null) && (mMenu.findItem(100) != null))
                mMenu.removeItem(100);
        }

        if(hasZSLSupport == true) {
            if((mMenu != null) && (mMenu.findItem(200) == null))
                addSnapshotModeSwitchMenu();
        }else{
            if((mMenu != null) && (mMenu.findItem(200) != null))
                mMenu.removeItem(200);
        }

        // Reload the preferences.
        mPreferences.setLocalId(this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());

        // Restart the preview.
        resetExposureCompensation();
        if (!restartPreview()) return;

        initializeZoom();

        // Reload the UI.
        if (mFirstTimeInitialized) {
            initializeHeadUpDisplay();
        }
    }

    private boolean switchToVideoMode() {
        if (isFinishing() || !isCameraIdle()) return false;
        MenuHelper.gotoVideoMode(this);
        mHandler.removeMessages(FIRST_TIME_INIT);
        finish();
        return true;
    }

    public boolean onSwitchChanged(Switcher source, boolean onOff) {
        if (onOff == SWITCH_VIDEO) {
            return switchToVideoMode();
        } else {
            return true;
        }
    }

    private void onSharedPreferenceChanged() {
        // ignore the events after "onPause()"
        if (mPausing) return;

        boolean recordLocation;

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
        int cameraId = CameraSettings.readPreferredCameraId(mPreferences);
        if (mCameraId != cameraId) {
            switchCameraId(cameraId);
        } else {
            setCameraParametersWhenIdle(UPDATE_PARAM_PREFERENCE);
        }
    }

    private void cameraUtilProfile( String msg) {
        Log.e(TAG_PROFILE, " "+ msg +":" + System.currentTimeMillis()/1000.0);
    }
    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
        keepScreenOnAwhile();
        mHandler.sendEmptyMessageDelayed(CLEAR_FOCUS_INDICATOR, CLEAR_DELAY);
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

    private class MyHeadUpDisplayListener implements HeadUpDisplay.Listener {

        public void onSharedPreferencesChanged() {
            Camera.this.onSharedPreferenceChanged();
        }

        public void onRestorePreferencesClicked() {
            Camera.this.onRestorePreferencesClicked();
        }

        public void onPopupWindowVisibilityChanged(int visibility) {
        }
    }

    protected void onRestorePreferencesClicked() {
        if (mPausing) return;
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
}
//Graph View Class

class GraphView extends View {
    private Bitmap  mBitmap;
    private Paint   mPaint = new Paint();
    private Canvas  mCanvas = new Canvas();
    private float   mScale = (float)3;
    private float   mWidth;
    private float   mHeight;
    private Camera mCamera;
    private android.hardware.Camera mGraphCameraDevice;
    private float scaled;
    private static final int STATS_SIZE = 256;
    private static final String TAG = "GraphView";


    public GraphView(Context context, AttributeSet attrs) {
        super(context,attrs);

        mPaint.setFlags(Paint.ANTI_ALIAS_FLAG);
    }
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.RGB_565);
        mCanvas.setBitmap(mBitmap);
        mWidth = w;
        mHeight = h;
        super.onSizeChanged(w, h, oldw, oldh);
    }
    @Override
    protected void onDraw(Canvas canvas) {
        if(!Camera.mHiston ) {
            return;
         }
         if (mBitmap != null) {
             final Paint paint = mPaint;
             final Canvas cavas = mCanvas;
             final float border = 5;
             float graphheight = mHeight - (2 * border);
             float graphwidth = mWidth - (2 * border);
             float left,top,right,bottom;
             float bargap = 0.0f;
             float barwidth = 1.0f;

             cavas.drawColor(0xFFAAAAAA);
             paint.setColor(Color.BLACK);

             for (int k = 0; k <= (graphheight /32) ; k++) {
                 float y = (float)(32 * k)+ border;
                 cavas.drawLine(border, y, graphwidth + border , y, paint);
             }
             for (int j = 0; j <= (graphwidth /32); j++) {
                 float x = (float)(32 * j)+ border;
                 cavas.drawLine(x, border, x, graphheight + border, paint);
              }
              paint.setColor(0xFFFFFFFF);
              synchronized(Camera.statsdata) {
                  for(int i=1 ; i<=STATS_SIZE ; i++)  {
                      scaled = Camera.statsdata[i]/mScale;
                      if(scaled >= (float)STATS_SIZE)
                          scaled = (float)STATS_SIZE;
                       left = (bargap * (i+1)) + (barwidth * i) + border;
                       top = graphheight + border;
                       right = left + barwidth;
                       bottom = top - scaled;
                       cavas.drawRect(left, top, right, bottom, paint);
                  }
              }
              canvas.drawBitmap(mBitmap, 0, 0, null);

         }
         if(Camera.mHiston && mCamera!= null) {
             mGraphCameraDevice = mCamera.getCamera();
             if(mGraphCameraDevice != null)
                 mGraphCameraDevice.sendHistogramData();
             }
    }
    public void PreviewChanged() {
        invalidate();
    }
    public void setCameraObject(Camera camera) {
        mCamera = camera;
    }
}


class FocusRectangle extends View {

    @SuppressWarnings("unused")
    private static final String TAG = "FocusRectangle";
    private static final int SIZE = 50;
    private int xActual, yActual;

    public FocusRectangle(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    private void setDrawable(int resid) {
        setBackgroundDrawable(getResources().getDrawable(resid));
    }

    public void showStart() {
        setDrawable(R.drawable.focus_focusing);
    }

    public void showSuccess() {
        setDrawable(R.drawable.focus_focused);
    }

    public void showFail() {
        setDrawable(R.drawable.focus_focus_failed);
    }

    public void clear() {
        setBackgroundDrawable(null);
    }

    public void setPosition(int x, int y) {
       if (x >= 0 && y >= 0) {
           xActual = x;
           yActual = y;
           this.layout(x - SIZE, y - SIZE, x + SIZE, y + SIZE);
       }
    }
    public int getTouchIndexX() {
        return xActual;
    }

    public int getTouchIndexY() {
        return yActual;
    }
}

class FaceRectangle extends FocusRectangle {
    @SuppressWarnings("unused")
    private int faceData[] = new int[4];

    public FaceRectangle(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public int[] getFaceData() {
        return this.faceData;
    }

    public void drawFaceRectangle(int x, int y, int x1, int y1) {
        this.faceData[0] = x;
        this.faceData[1] = y;
        this.faceData[2] = x1;
        this.faceData[3] = y1;

        //Face co-ordinates passed by facial framework correspond to top-left corner
        if (x!= -1 && y != -1) {
            this.layout(x, y, x1, y1);
        }
    }
}

/*
 * Provide a mapping for Jpeg encoding quality levels
 * from String representation to numeric representation.
 */
class JpegEncodingQualityMappings {
    private static final String TAG = "JpegEncodingQualityMappings";
    private static final int DEFAULT_QUALITY = 85;
    private static HashMap<String, Integer> mHashMap =
            new HashMap<String, Integer>();

    static {
        mHashMap.put("normal",    CameraProfile.QUALITY_LOW);
        mHashMap.put("fine",      CameraProfile.QUALITY_MEDIUM);
        mHashMap.put("superfine", CameraProfile.QUALITY_HIGH);
    }

    // Retrieve and return the Jpeg encoding quality number
    // for the given quality level.
    public static int getQualityNumber(String jpegQuality) {
        try{
            int qualityPercentile = Integer.parseInt(jpegQuality);
            if(qualityPercentile >= 0 && qualityPercentile <=100)
                return qualityPercentile;
            else
                return DEFAULT_QUALITY;
        } catch(NumberFormatException nfe){
            //chosen quality is not a number, continue
        }
        Integer quality = mHashMap.get(jpegQuality);
        if (quality == null) {
            Log.w(TAG, "Unknown Jpeg quality: " + jpegQuality);
            return DEFAULT_QUALITY;
        }
        return CameraProfile.getJpegEncodingQualityParameter(quality.intValue());
    }
}