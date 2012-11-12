/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPlayerPrivateAndroidInpage_h
#define MediaPlayerPrivateAndroidInpage_h

#if ENABLE(VIDEO) && ENABLE(INPAGE_VIDEO)

#include "CString.h"
#include "ColorConverter/ColorConverterAPI.h"
#include "Deque.h"
#include "GraphicsContext.h"
#include "HashSet.h"
#include "IntRect.h"
#include "IntSize.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivate.h"
#include "MessageQueue.h"
#include "Noncopyable.h"
#include "PassOwnPtr.h"
#include "PassRefPtr.h"
#include "PlatformString.h"
#include "SkBitmap.h"
#include "TimeRanges.h"
#include "Timer.h"
#include "binder/MemoryHeapBase.h"
#include "media/mediaplayer.h"
#include "utils/Errors.h"
#include "utils/RefBase.h"

using namespace WTF;

namespace WebCore {

class MediaPlayerPrivateAndroidInpage : public MediaPlayerPrivateInterface {
public:
    class WebCoreMediaPlayerListener: public android::MediaPlayerListener {
    public:
        static android::sp<WebCoreMediaPlayerListener> create(MediaPlayerPrivateAndroidInpage* mpp);
        void notify(int msg, int ext1, int ext2);
    private:
        WebCoreMediaPlayerListener(MediaPlayerPrivateAndroidInpage* mpp)
            : m_mpp(mpp)
        {
        }
        MediaPlayerPrivateAndroidInpage* m_mpp;
    };

    class MessagePacket : public Noncopyable {
    public:
        static PassOwnPtr<MessagePacket> create(int msg, int ext1, int ext2);
        int msg() { return m_msg; }
        int ext1() { return m_ext1; }
        int ext2() { return m_ext2; }
    private:
        MessagePacket(int msg, int ext1, int ext2)
            : m_msg(msg)
            , m_ext1(ext1)
            , m_ext2(ext2)
        {
        }
        int m_msg;
        int m_ext1;
        int m_ext2;

    };

    // methods for media engine registration -- method names can be arbitrary
public:
    static void registerMediaEngine(MediaEngineRegistrar registrar);

private:
    static MediaPlayerPrivateInterface* create(MediaPlayer* player);
    static MediaPlayer::MediaEngineType getMediaEngineType() { return MediaPlayer::Inpage; }
    static void getSupportedTypes(HashSet<String>&) { }
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);

    // MediaPlayerPrivateInterface
public:
    ~MediaPlayerPrivateAndroidInpage();
    void load(const String& url);
    void cancelLoad();
    void play();
    void pause();
    bool supportsFullscreen() const { return true; }
    IntSize naturalSize() const { return IntSize(m_width, m_height); }
    bool hasVideo() const { return m_hasVideo; }
    bool hasAudio() const { return m_hasAudio; }
    void setVisible(bool b);
    float duration() const { return m_duration; }
    float currentTime() const;
    void seek(float f);
    bool seeking() const { return m_seeking; }
    void setRate(float f);
    bool paused() const;
    void setVolume(float f);
    MediaPlayer::NetworkState networkState() const { return m_networkState; }
    MediaPlayer::ReadyState readyState() const { return m_readyState; }
    float maxTimeSeekable() const;
    PassRefPtr<TimeRanges> buffered() const;
    unsigned bytesLoaded() const;
    void setSize(const IntSize& size);
    void paint(GraphicsContext* gc, const IntRect& rect);
    bool hasAvailableVideoFrame() const;
#if USE(ACCELERATED_COMPOSITING)
    virtual bool supportsAcceleratedRendering() const { return true; }
    virtual void acceleratedRenderingStateChanged();
#endif

    String url() const { return m_url; }

    // constants
private:
    // See http://www.whatwg.org/specs/web-apps/current-work/#event-media-timeupdate
    static const double TimerFrequency = 0.250; // The spec says the timer should fire every 250 ms or less.
    static const double VideoTimerFrequency = 0.033; // for 30 FPS
    static const int NumFrameBuffer = 4; // minimal of 2
    static const int SecondsToMilliseconds = 1000;

private:
    MediaPlayerPrivateAndroidInpage(MediaPlayer* player);

    void onMediaNop(MessagePacket*);
    void onMediaPrepared(MessagePacket*);
    void onMediaPlaybackComplete(MessagePacket*);
    void onMediaBufferingUpdate(MessagePacket*);
    void onMediaSeekComplete(MessagePacket*);
    void onMediaSetVideoSize(MessagePacket*);
    void onMediaError(MessagePacket*);
    void onMediaInfo(MessagePacket*);
    void onMediaFormatChanged(MessagePacket*);
    void onMediaBufferReady(MessagePacket*);
    void onUnhandledMessage(MessagePacket*);

    void timerFired(Timer<MediaPlayerPrivateAndroidInpage>*);
    void setNetworkErrorState(android::status_t status);
    void sendFrameToMediaPlayer(int index);
    void registerHeap();
    void unregisterHeap();
    void yuvToRGBFrame(char* yuvFrame, char* rgbFrame);
    void updateColorFormat();
    char* getFramePtr(int index) const
    {
        if (index >= NumFrameBuffer || !m_frameHeap.get())
            return 0;

        return static_cast<char*>(m_frameHeap->getBase()) + (index * m_frameSize);
    }

#if USE(ACCELERATED_COMPOSITING)
    void setDrawsContent();
#endif

    Timer<MediaPlayerPrivateAndroidInpage> m_timer;

    MediaPlayer* m_player;

    android::sp<android::MediaPlayer> m_mp;

    String m_url;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    bool m_hasVideo;
    bool m_hasAudio;
    int m_width;
    int m_height;
    int m_colorFormat;
    int m_frameSize;
    float m_duration;

    int m_pendingWidth;
    int m_pendingHeight;
    bool m_shouldShowPoster;
    bool m_visible;
    bool m_prepared;
    bool m_seeking;
    bool m_completed;
    bool m_realloc;
    float m_seekTime;
    int32_t m_framePercentBuffered; // Amount of frames buffered by the media player expressed in percentage

    MessageQueue<MessagePacket> m_notifyQueue;

    SkBitmap m_videoBitmap;
    android::sp<android::MemoryHeapBase> m_frameHeap;

    Deque<int> m_frameQueue;
    Deque<int> m_reallocQueue;

    static void* s_library;
    static WebTech::ColorConverter_t s_colorConverter;
    static WebTech::CalcFrameSize_t s_calcFrameSize;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(INPAGE_VIDEO)
#endif // MediaPlayerPrivateAndroidInpage_h
