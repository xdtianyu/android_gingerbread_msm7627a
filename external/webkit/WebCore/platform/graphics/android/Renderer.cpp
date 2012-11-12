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

#include "config.h"

#if ENABLE(ACCELERATED_SCROLLING)
#define LOG_TAG "renderer"
#define LOG_NDEBUG 0

#include "Renderer.h"

#include "BackingStore.h"
#include "CurrentTime.h"
#if ENABLE(GPU_ACCELERATED_SCROLLING)
#include "Renderer2D.h"
#endif
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPaintFlagsDrawFilter.h"
#include "SkPoint.h"
#include "SkProxyCanvas.h"
#include "SkShader.h"
#include "SkTypeface.h"
#include "SkXfermode.h"
#include "WebViewCore.h"
#include <cutils/log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <stdio.h>

#if ENABLE(GPU_ACCELERATED_SCROLLING)
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <surfaceflinger/IGraphicBufferAlloc.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif // GPU_ACCELERATED_SCROLLING

#define DO_LOG_PERF
#define DO_LOG_RENDER

#undef LOG_PERF
#ifdef DO_LOG_PERF
#define LOG_PERF(...) ((s_logPerf)? SLOGV(__VA_ARGS__):(void)0)
#else
#define LOG_PERF(...) ((void)0)
#endif

#undef LOG_RENDER
#ifdef DO_LOG_RENDER
#define LOG_RENDER(...) ((s_log)? SLOGV(__VA_ARGS__):(void)0)
#else
#define LOG_RENDER(...) ((void)0)
#endif

static int s_log = 0;
static int s_logPerf = 0;

using namespace android;

namespace RendererImplNamespace {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
        = glGetError()) {
            LOG_RENDER("after %s() glError (0x%x)", op, error);
    }
}
#endif


// RendererImpl class
// This is the implementation of the Renderer class.  Users of this class will only see it
// through the Renderer interface.  All implementation details are hidden in this RendererImpl
// class.
class RendererImpl : public Renderer, public WebTech::IBackingStore::IUpdater {
    struct RenderTask {
        enum RenderQuality {
            LOW,
            HIGH
        };

        RenderTask()
            : valid(false)
        {
        }
        SkColor color;
        bool invertColor;
        WebTech::IBackingStore::UpdateRegion requestArea; // requested valid region in content space.
        float contentScale;
        SkIPoint contentOrigin; // (x,y) in content space, for the point at (0,0) in the viewport
        int viewportWidth;
        int viewportHeight;
        RenderQuality quality;
        SkBitmap::Config config;
        int newContent;
        bool valid;
        bool canAbort;
        bool useGL;
        bool needGLSync;
        bool needRedraw;
    };

    struct RendererConfig {
        RendererConfig()
            : enableSpeculativeUpdate(true)
            , allowSplit(true)
            , splitSize(50)
            , enablePartialUpdate(true)
            , enablePartialRender(false)
            , enableDraw(true)
            , enableZoom(false)
        {
            if (enablePartialRender)
                enablePartialUpdate = false;
        }

        static RendererConfig* getConfig();

        void init()
        {
            char pval[PROPERTY_VALUE_MAX];
            property_get("persist.debug.tbs.log", pval, "0");
            s_log = atoi(pval);
            property_get("persist.debug.tbs.perf", pval, "0");
            s_logPerf = atoi(pval);
            property_get("persist.debug.tbs.enable", pval, "1");
            enableDraw = atoi(pval)? true : false;
            property_get("persist.debug.tbs.speculative", pval, "1");
            enableSpeculativeUpdate = atoi(pval)? true : false;
            property_get("persist.debug.tbs.partialupdate", pval, "1");
            enablePartialUpdate = atoi(pval)? true : false;
            property_get("persist.debug.tbs.partialrender", pval, "0");
            enablePartialRender = atoi(pval)? true : false;
            property_get("persist.debug.tbs.zoom", pval, "0");
            enableZoom = atoi(pval)? true : false;
            property_get("persist.debug.tbs.split", pval, "0");
            allowSplit = atoi(pval)? true : false;
            print();
        }

        void print()
        {
            LOG_RENDER("TBS properties:");
            LOG_RENDER("    enableDraw = %d", enableDraw? 1 : 0);
            LOG_RENDER("    enableSpeculativeUpdate = %d", enableSpeculativeUpdate? 1 : 0);
            LOG_RENDER("    enablePartialUpdate = %d", enablePartialUpdate? 1 : 0);
            LOG_RENDER("    enablePartialRender = %d", enablePartialRender? 1 : 0);
            LOG_RENDER("    enableZoom = %d", enableZoom? 1 : 0);
            LOG_RENDER("    allowSplit = %d", allowSplit? 1 : 0);
        }

        bool enableSpeculativeUpdate; // allow speculative update
        bool allowSplit; // allow the PictureSet to be split into smaller pieces for better performance
        int splitSize; // each block is 50 pages
        bool enablePartialUpdate; // allow backing store to perform partial update on new PictureSet instead of redrawing everything
        bool enablePartialRender;
        bool enableDraw; // enable the backing store
        bool enableZoom; // allow fast zooming using the backing store
    };

#if ENABLE(GPU_ACCELERATED_SCROLLING)

#define NUM_GL_RESOURCE_RENDERERS 1


    class GLResourceUsers {
    public:
        GLResourceUsers();
        ~GLResourceUsers();
        static GLResourceUsers* getGLResourceUsers();
        void getGLResource(RendererImpl* renderer);
        void releaseGLResource(RendererImpl* renderer);
        void incrementTimestamp()
        {
            ++m_curTime;
            if (m_curTime >= 0xffffffff) {
                m_curTime = 0;
                for (int i=0; i < NUM_GL_RESOURCE_RENDERERS; ++i)
                    m_timeStamp[i] = m_curTime;
            }
        }
    private:
        bool m_used[NUM_GL_RESOURCE_RENDERERS];
        RendererImpl* m_renderers[NUM_GL_RESOURCE_RENDERERS];
        unsigned int m_timeStamp[NUM_GL_RESOURCE_RENDERERS];
        unsigned int m_curTime;
    };
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    // implementation of WebTech::IBackingStore::IBuffer interface.
    class BackingStoreBuffer : public WebTech::IBackingStore::IBuffer {
    public:
        BackingStoreBuffer(int w, int h, int bpp
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            , IGraphicBufferAlloc* allocator
#endif // GPU_ACCELERATED_SCROLLING
            )
            : m_refCount(1)
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            , m_eglImage(0)
            , m_eglDisplay(0)
            , m_textureName(0)
#endif // GPU_ACCELERATED_SCROLLING
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            LOG_RENDER("BackingStoreBuffer constructor.  allocator = %p", allocator);
#endif // GPU_ACCELERATED_SCROLLING
            SkBitmap::Config config = bppToConfig(bpp);
            int stride = 0;

#if ENABLE(GPU_ACCELERATED_SCROLLING)
            if (allocator) {
                int format = bppToGraphicBufferFormat(bpp);
                LOG_RENDER("creating GraphicBuffer w = %d, h = %d", w, h);
                m_graphicBuffer = allocator->createGraphicBuffer(w, h, format,
                    GraphicBuffer::USAGE_HW_TEXTURE |
                    GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
                if (m_graphicBuffer == 0) {
                    LOG_RENDER("Error: cannot allocate GraphicBuffer");
                }
                if (m_graphicBuffer != 0) {
                    stride = m_graphicBuffer->getStride() * 4;
                    LOG_RENDER("GraphicBuffer width = %d, stride = %d", m_graphicBuffer->getWidth(), stride);
                }
            }
#endif // GPU_ACCELERATED_SCROLLING

            m_bitmap.setConfig(config, w, h, stride);

#if ENABLE(GPU_ACCELERATED_SCROLLING)
            if (m_graphicBuffer == 0)
#endif // GPU_ACCELERATED_SCROLLING
                m_bitmap.allocPixels();

            ++s_numBuffers;

            static unsigned int s_id;
            m_id = ++s_id;
            if (s_id <= 0)
                s_id = 1;

            LOG_RENDER("created BackingStoreBuffer - number of buffers: %d", s_numBuffers);
        }

        virtual ~BackingStoreBuffer()
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            if (m_eglImage) {
                eglDestroyImageKHR(m_eglDisplay, m_eglImage);
            }
#endif // GPU_ACCELERATED_SCROLLING

            --s_numBuffers;
            LOG_RENDER("~BackingStoreBuffer - number of buffers left: %d", s_numBuffers);
        }

        void addRef()
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            // Normally only TBS's working thread change the buffer's reference count.
            // But in GPU mode, the UI thread also add reference to the buffer because
            // the GPU may be using the buffer.  So only if GPU is used this function need
            // to be thread safe.
            MutexLocker locker(m_mutex);
#endif
            ++m_refCount;
        }

        virtual int width()
        {
            return m_bitmap.width();
        }

        virtual int height()
        {
            return m_bitmap.height();
        }

        virtual void release()
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            MutexLocker locker(m_mutex);
#endif
            --m_refCount;
            if (m_refCount<=0)
                delete this;
        }

        const SkBitmap& getBitmap()
        {
            return m_bitmap;
        }

        unsigned int getID()
        {
            return m_id;
        }

        void lock()
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            if (m_graphicBuffer != 0) {
                void* buf = NULL;
                LOG_RENDER("locking GraphicBuffer");
                m_graphicBuffer->lock(GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&buf));
                m_bitmap.setPixels(buf);
            }
#endif // GPU_ACCELERATED_SCROLLING
        }

        void unlock()
        {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            if (m_graphicBuffer != 0) {
                LOG_RENDER("unlocking GraphicBuffer");
                m_graphicBuffer->unlock();
            }
#endif // GPU_ACCELERATED_SCROLLING
        }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
        GLuint bindTexture()
        {
            LOG_RENDER("bindTexture");
            if (!m_eglImage && m_graphicBuffer != 0) {
                EGLDisplay display = eglGetCurrentDisplay();
                EGLClientBuffer clientBuffer = (EGLClientBuffer)m_graphicBuffer->getNativeBuffer();
                m_eglImage = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                    clientBuffer, 0);
                if (!m_eglImage) {
                    LOG_RENDER("eglCreateImageKHR failed");
                }
                m_eglDisplay = display;
            }

            if (!m_textureName) {
                glGenTextures(1, &m_textureName);
                glBindTexture(GL_TEXTURE_2D, m_textureName);checkGlError("glBindTexture");
                glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_eglImage);checkGlError("glEGLImageTargetTexture2DOES");
            }
            return m_textureName;
        }
#endif
        bool failed()
        {
            return
#if ENABLE(GPU_ACCELERATED_SCROLLING)
                (m_graphicBuffer == 0) &&
#endif // GPU_ACCELERATED_SCROLLING
                (!m_bitmap.getPixels());
        }

    private:
        static SkBitmap::Config bppToConfig(int bpp)
        {
            if (bpp == 16)
                return SkBitmap::kRGB_565_Config;
            return SkBitmap::kARGB_8888_Config;
        }

        static int bppToGraphicBufferFormat(int bpp)
        {
            if (bpp == 16)
                return HAL_PIXEL_FORMAT_RGB_565;
            return HAL_PIXEL_FORMAT_RGBA_8888;
        }

        int m_refCount;
        SkBitmap m_bitmap;
        static int s_numBuffers;
        unsigned int m_id;
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        WTF::Mutex m_mutex;
        sp<GraphicBuffer> m_graphicBuffer;
        EGLImageKHR m_eglImage;
        EGLDisplay m_eglDisplay;
        GLuint m_textureName;
#endif // GPU_ACCELERATED_SCROLLING
    };

#if ENABLE(GPU_ACCELERATED_SCROLLING)
    class GLBackingStoreBufferManager {
    public:
        GLBackingStoreBufferManager();
        ~GLBackingStoreBufferManager();
        static GLBackingStoreBufferManager* getGLBackingStoreBufferManager();
        BackingStoreBuffer* getBuffer(int w, int h);

    private:
        BackingStoreBuffer* m_bufferPortrait;
        BackingStoreBuffer* m_bufferLandscape;
        sp<ISurfaceComposer> m_composer;
        sp<IGraphicBufferAlloc> m_graphicBufferAlloc;
    };
#endif // GPU_ACCELERATED_SCROLLING

    // ContentData - data that must be guarded in mutex.  New content can be set in a webcore thread (webkit main thread),
    // but used in the UI thread.  Any data that can be changed in the webcore thread is encapsulated in
    // this class.
    class ContentData {
    public:
        ContentData()
            : m_incomingPicture(0)
            , m_picture(0)
            , m_contentWidth(0)
            , m_contentHeight(0)
            , m_numIncomingContent(0)
            , m_numIncomingPicture(0)
            , m_incomingLoading(false)
            , m_loading(false)
            , m_incomingInvalidateAll(false)
            , m_invalidateAll(false)
        {
        }

        ~ContentData()
        {
            if (m_incomingPicture)
                m_incomingPicture->unref();
            if (m_picture)
                m_picture->unref();
        }

        // returns number of times content has changed
        int numContentChanged()
        {
            MutexLocker locker(m_mutex);
            return m_numIncomingContent;
        }

        // returns number of times picture has changed
        int numPictureChanged()
        {
            MutexLocker locker(m_mutex);
            return m_numIncomingPicture;
        }

        // if numContentChanged() above returns non zero, changeToNewContent() can be called to
        // switch the currently active content (which may be used by IBackingStore) to the new content.
        // A copy of the reference of the new data is copied for access by the UI thread.  The content
        // previously used by the UI thread is released.
        void changeToNewContent()
        {
            MutexLocker locker(m_mutex);
            if (m_numIncomingContent > 0) {
                m_content = m_incomingContent.release();
                if (m_content) {
                    m_contentWidth = m_content->width();
                    m_contentHeight = m_content->height();
                } else {
                    m_contentWidth = 0;
                    m_contentHeight = 0;
                }
            }
            if (m_numIncomingPicture > 0) {
                if (m_picture)
                    m_picture->unref();
                m_picture = m_incomingPicture;
                m_incomingPicture = 0;
            }

            if (m_incomingContentInvalidRegion && !m_contentInvalidRegion) {
                m_contentInvalidRegion = m_incomingContentInvalidRegion.release();
            }
            else if (m_incomingContentInvalidRegion && m_contentInvalidRegion) {
                m_contentInvalidRegion->op(*m_incomingContentInvalidRegion, SkRegion::kUnion_Op);
                m_incomingContentInvalidRegion.clear();
            }

            m_numIncomingContent = 0;
            m_numIncomingPicture = 0;

            m_loading = m_incomingLoading;
            m_invalidateAll |= m_incomingInvalidateAll;
            m_incomingInvalidateAll = false;
        }

        // can be called from webcore thread (webkit main thread) when setting to null content.
        void onClearContent()
        {
            MutexLocker locker(m_mutex);
            m_incomingContent.clear();
            m_incomingContentInvalidRegion.clear();

            ++m_numIncomingContent;

            m_loading = m_incomingLoading = false;
            m_incomingInvalidateAll = true;
        }

        // can be called from webcore thread (webkit main thread) when new content is available.
        bool onNewContent(const PictureSet& content, SkRegion* region, bool* loading)
        {
            MutexLocker locker(m_mutex);
            int num = content.size();
            LOG_RENDER("new content size = %d x %d.  %d pictures", content.width(), content.height(), num);
            if (region) {
                LOG_RENDER("        - region={%d,%d,r=%d,b=%d}.", region->getBounds().fLeft,
                    region->getBounds().fTop, region->getBounds().fRight,
                    region->getBounds().fBottom);
            }
            m_incomingContent.clear();
            m_incomingContent = new PictureSet(content);

            bool invalidateAll = !region || !RendererConfig::getConfig()->enablePartialUpdate;
            invalidateAll |= ((m_contentWidth != content.width()) || (m_contentHeight != content.height()));

            if (region ) {
                SkIRect rect;
                rect.set(0, 0, content.width(), content.height());
                if (region->contains(rect))
                    invalidateAll = true;
            }
            if (!invalidateAll) {
                LOG_RENDER("setContent. invalidate region");
                if (!m_incomingContentInvalidRegion)
                    m_incomingContentInvalidRegion = new SkRegion(*region);
                else
                    m_incomingContentInvalidRegion->op(*region, SkRegion::kUnion_Op);
            } else {
                LOG_RENDER("setContent. invalidate All");
                m_incomingContentInvalidRegion.clear();
                m_incomingInvalidateAll = true;
            }
            ++m_numIncomingContent;
            if (loading)
                m_incomingLoading = *loading;
            return invalidateAll;
        }

        bool onNewPicture(const SkPicture* picture, WebCore::IntRect& rect)
        {
            MutexLocker locker(m_mutex);
            if (picture)
                LOG_RENDER("new picture size = %d x %d.", picture->width(), picture->height());
            LOG_RENDER("        - rect: x=%d, y=%d, w=%d, h=%d.", rect.x(), rect.y(), rect.width(), rect.height());
            LOG_RENDER("        - last rect: x=%d, y=%d, w=%d, h=%d.", m_lastPictureRect.x(), m_lastPictureRect.y(), m_lastPictureRect.width(), m_lastPictureRect.height());

            if (m_incomingPicture)
                m_incomingPicture->unref();
            m_incomingPicture = 0;
            if (picture)
                m_incomingPicture = new SkPicture(*picture);

            IntRect largeRect = rect;
            largeRect.inflateX(rect.width());
            largeRect.inflateY(rect.height());

            IntRect urect = unionRect(largeRect, m_lastPictureRect);
            bool invalidate = false;
            if (!urect.isEmpty()) {
                ++m_numIncomingPicture;
#if 1
                if (!m_incomingInvalidateAll) {
                    SkIRect irect;
                    irect = (SkIRect)urect;
                    SkRegion region(irect);
                    LOG_RENDER("        - union rect: x=%d, y=%d, w=%d, h=%d.", urect.x(), urect.y(), urect.width(), urect.height());

                    if (!m_incomingContentInvalidRegion)
                        m_incomingContentInvalidRegion = new SkRegion(region);
                    else
                        m_incomingContentInvalidRegion->op(region, SkRegion::kUnion_Op);
                }
#else
                m_incomingInvalidateAll = true;
#endif
            }

            m_lastPictureRect = largeRect;

            return invalidate;
        }

        WTF::Mutex m_mutex; // for guarding access from UI thread and webcore thread (webkit main thread).
        OwnPtr<PictureSet> m_incomingContent;
        OwnPtr<PictureSet> m_content;
        SkPicture* m_incomingPicture;
        SkPicture* m_picture;
        OwnPtr<SkRegion> m_incomingContentInvalidRegion;
        OwnPtr<SkRegion> m_contentInvalidRegion;
        int m_contentWidth;
        int m_contentHeight;
        int m_numIncomingContent;
        int m_numIncomingPicture;
        bool m_incomingLoading;
        bool m_loading;
        bool m_incomingInvalidateAll;
        bool m_invalidateAll;
        WebCore::IntRect m_lastPictureRect;
    };

public:
    RendererImpl()
        : m_backingStore(0)
        , m_lastScale(1.0f)
        , m_numTimesScaleUnchanged(0)
        , m_loading(false)
        , m_doPartialRender(false)
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        , m_syncObject(EGL_NO_SYNC_KHR)
#endif
        , m_bufferInUse(0)
    {
        RendererConfig::getConfig()->init();
        m_doPartialRender = RendererConfig::getConfig()->enablePartialRender;
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        m_2DRenderer = m_createRenderer2D();
#endif // GPU_ACCELERATED_SCROLLING

        char libraryName[PROPERTY_VALUE_MAX];
        property_get(WebTech::BackingStoreLibraryNameProperty, libraryName, WebTech::BackingStoreLibraryName);
        m_library = dlopen(libraryName, RTLD_LAZY);
        if (!m_library)
            LOG_RENDER("Failed to open acceleration library %s", libraryName);
        else {
            m_createBackingStore = (WebTech::CreateBackingStore_t) dlsym(m_library, WebTech::CreateBackingStoreFuncName);
            m_getBackingStoreVersion = (WebTech::GetBackingStoreVersion_t) dlsym(m_library, WebTech::GetBackingStoreVersionFuncName);
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            m_createRenderer2D = (WebTech::CreateRenderer2D_t) dlsym(m_library, WebTech::CreateRenderer2DFuncName);
#endif
            if (!m_createBackingStore || !m_getBackingStoreVersion
#if ENABLE(GPU_ACCELERATED_SCROLLING)
                || !m_createRenderer2D
#endif
                ) {
                dlclose(m_library);
                m_library = 0;
                LOG_RENDER("Failed to find acceleration routines in library %s", libraryName);
            }
            else
                LOG_RENDER("Loaded acceleration library %s", libraryName);
        }
    }

    ~RendererImpl()
    {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        GLResourceUsers::getGLResourceUsers()->releaseGLResource(this);
#endif
        if (m_backingStore)
            m_backingStore->release();
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        glBindTexture(GL_TEXTURE_2D, 0);
        if (m_syncObject != EGL_NO_SYNC_KHR)
            eglDestroySyncKHR(eglGetCurrentDisplay(), m_syncObject);
#endif
        if (m_bufferInUse) {
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            glFinish();
#endif
            m_bufferInUse->release();
        }
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        if (m_2DRenderer)
            m_2DRenderer->release();
#endif
        if (m_library)
            dlclose(m_library);
    }

    virtual void release()
    {
        delete this;
    }

    // can be called from webcore thread (webkit main thread).
    virtual void setContent(const PictureSet& content, SkRegion* region, bool loading)
    {
        bool invalidate = m_contentData.onNewContent(content, region, &loading);
        if (!m_doPartialRender && m_backingStore && invalidate)
            m_backingStore->invalidate();
    }

    // can be called from webcore thread (webkit main thread).
    virtual void setContent(const SkPicture* picture, WebCore::IntRect& rect)
    {
        bool invalidate = m_contentData.onNewPicture(picture, rect);
        if (!m_doPartialRender && m_backingStore && invalidate)
            m_backingStore->invalidate();
    }

    // can be called from webcore thread (webkit main thread).
    virtual void clearContent()
    {
        LOG_RENDER("client clearContent");
        m_contentData.onClearContent();
        if (!m_doPartialRender && m_backingStore)
            m_backingStore->invalidate();
    }

    // can be called from webcore thread (webkit main thread).
    virtual void pause()
    {
        LOG_RENDER("client pause");
        if (m_backingStore)
            m_backingStore->cleanup();
    }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
    void releaseGLMemory()
    {
        LOG_RENDER("releaseGLMemory");
        glBindTexture(GL_TEXTURE_2D, 0);
        if (m_bufferInUse) {
            glFinish();
            m_bufferInUse->release();
            m_bufferInUse = 0;
        }
        if (m_backingStore)
            m_backingStore->cleanup();
    }
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    virtual void finish()
    {
        LOG_RENDER("client finish");
        if (m_backingStore)
            m_backingStore->finish();
    }

    // called in the UI thread.
    virtual bool drawContent(SkCanvas* canvas, SkColor color, bool invertColor, PictureSet& content, bool& splitContent)
    {
        if (!RendererConfig::getConfig()->enableDraw)
            return false;
        LOG_RENDER("drawContent");

#ifdef DO_LOG_RENDER
        if (s_log) {
            const SkMatrix& matrix = canvas->getTotalMatrix();
            SkScalar tx = matrix.getTranslateX();
            SkScalar ty = matrix.getTranslateY();
            SkScalar sx = matrix.getScaleX();
            const SkRegion& clip = canvas->getTotalClip();
            SkIRect clipBound = clip.getBounds();
            int isRect = (clip.isRect())?1:0;
            SLOGV("drawContent tx=%f, ty=%f, scale=%f", tx, ty, sx);
            SLOGV("  clip %d, %d to %d, %d.  isRect=%d", clipBound.fLeft, clipBound.fTop, clipBound.fRight, clipBound.fBottom, isRect);
        }
#endif

        splitContent = false;

        RenderTask request;
        generateRequest(canvas, color, invertColor, request);

        bool drawn = onDraw(request, canvas);

        // The following is the code to allow splitting of the PictureSet
        // for situations where a huge PictureSet is slow to render.  This
        // requires a new setDrawTimes() function in the PictureSet class.
        // It can be enabled later when the changes in PictureSet is done.
#if DO_CONTENT_SPLIT
        if (RendererConfig::getConfig()->allowSplit) {
            int split = suggestContentSplitting(content, request);
            if (split) {
                LOG_RENDER("renderer client triggers content splitting %d", split);
                content.setDrawTimes((uint32_t)(100 << (split - 1)));
                splitContent = true;
            }
        }
#endif
        return drawn;
    }

#if ENABLE(GPU_ACCELERATED_SCROLLING)

    bool drawContentGL(PictureSet& content, WebCore::IntRect& viewRect, SkRect& contentRect, float scale, SkColor color)
    {
        if (!RendererConfig::getConfig()->enableDraw)
            return false;
        LOG_RENDER("drawContentGL");

        GLResourceUsers::getGLResourceUsers()->getGLResource(this);

#ifdef DO_LOG_RENDER
        if (s_log) {
            SLOGV("  viewRect (%d, %d) w = %d, h = %d.", viewRect.x(), viewRect.y(), viewRect.width(), viewRect.height());
            SLOGV("  contentRect (%f, %f) w = %f, h = %f.", contentRect.fLeft, contentRect.fTop, contentRect.width(), contentRect.height());
            SLOGV("  scale = %f", scale);
        }
#endif
        RenderTask request;
        generateRequestGL(viewRect, contentRect, scale, color, request);

        EGLDisplay display = eglGetCurrentDisplay();
        if (m_syncObject != EGL_NO_SYNC_KHR) {
            EGLint status = eglClientWaitSyncKHR(display, m_syncObject, 0, 1000000);

            if (status == EGL_TIMEOUT_EXPIRED_KHR)
                LOG_RENDER("Sync timeout");

            eglDestroySyncKHR(display, m_syncObject);
            m_syncObject = EGL_NO_SYNC_KHR;
        }
        if (m_bufferInUse) {
            m_bufferInUse->release();
            m_bufferInUse = 0;
        }
        onDraw(request, 0);

        m_syncObject = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, 0);

        return request.needRedraw;
    }
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    ////////////////////////////// IBackingStore::IUpdater methods ///////////////////////
    virtual void inPlaceScroll(WebTech::IBackingStore::IBuffer* buffer, int x, int y, int w, int h, int dx, int dy)
    {
        if (!buffer)
            return;

        BackingStoreBuffer* bitmap = static_cast<BackingStoreBuffer*>(buffer);
        if (!bitmap)
            return;

        bitmap->lock();
        int pitch = bitmap->getBitmap().rowBytes();
        int bpp = bitmap->getBitmap().bytesPerPixel();
        int ny = h;
        int nx = w * bpp;
        char* src = static_cast<char*>(bitmap->getBitmap().getPixels());
        src = src + x * bpp + y * pitch;
        int dptr = pitch;

        if (dy>0) {
            src = src + (h-1) * pitch;
            dptr = -dptr;
        }

        char* dst = src + dx*bpp + dy*pitch;

        if (!dy) {
            for (int i = 0; i < ny; ++i) {
                memmove(dst, src, nx);
                dst += dptr;
                src += dptr;
            }
        } else {
            for (int i = 0; i < ny; ++i) {
                memcpy(dst, src, nx);
                dst += dptr;
                src += dptr;
            }
        }
        bitmap->unlock();
    }

    virtual WebTech::IBackingStore::IBuffer* createBuffer(int w, int h)
    {
        LOG_RENDER("RendererImpl::createBuffer");
        BackingStoreBuffer* buffer;

#if ENABLE(GPU_ACCELERATED_SCROLLING)
        GLBackingStoreBufferManager* mgr = GLBackingStoreBufferManager::getGLBackingStoreBufferManager();

        if (mgr) {
            buffer = mgr->getBuffer(w, h);
        } else
#endif
            buffer = new BackingStoreBuffer(w, h, SkBitmap::ComputeBytesPerPixel(m_request.config)
#if ENABLE(GPU_ACCELERATED_SCROLLING)
            , 0
#endif
            );


        if (!buffer || buffer->failed()) {
            SLOGV("failed to allocate buffer for backing store");
            if (buffer)
                buffer->release();
            return 0;
        }
        return static_cast<WebTech::IBackingStore::IBuffer*>(buffer);
    }

    virtual void renderToBackingStoreRegion(WebTech::IBackingStore::IBuffer* buffer, int bufferX, int bufferY, WebTech::IBackingStore::UpdateRegion& region, WebTech::IBackingStore::UpdateQuality quality, float scale, bool existingRegion)
    {
        if (!m_contentData.m_content && !m_contentData.m_picture) {
            LOG_RENDER("renderToBackingStoreRegion: no content to draw");
            return;
        }

        if (!buffer) {
            LOG_RENDER("renderToBackingStoreRegion: no buffer to draw to");
            return;
        }
        BackingStoreBuffer* bitmap = static_cast<BackingStoreBuffer*>(buffer);
        if (!bitmap)
            return;
        bitmap->lock();

        LOG_RENDER("renderToBackingStoreRegion. out(%d, %d), area=(%d, %d) to (%d, %d) size=(%d, %d). scale = %f",
            bufferX, bufferY, region.x1, region.y1, region.x2, region.y2, region.x2 - region.x1, region.y2 - region.y1, scale);
        SkCanvas srcCanvas(bitmap->getBitmap());
        SkCanvas* canvas = static_cast<SkCanvas*>(&srcCanvas);
        SkRect clipRect;
        clipRect.set(bufferX, bufferY, bufferX + region.x2 - region.x1, bufferY + region.y2 - region.y1);
        canvas->clipRect(clipRect, SkRegion::kReplace_Op);

        SkScalar s = scale;
        SkScalar dx = -static_cast<SkScalar>(region.x1) + bufferX;
        SkScalar dy = -static_cast<SkScalar>(region.y1) + bufferY;
        canvas->translate(dx, dy);
        canvas->scale(s, s);

        uint32_t removeFlags, addFlags;

        removeFlags = SkPaint::kFilterBitmap_Flag | SkPaint::kDither_Flag;
        addFlags = 0;
        SkPaintFlagsDrawFilter filterLo(removeFlags, addFlags);

        int sc = canvas->save(SkCanvas::kClip_SaveFlag);

        if (m_contentData.m_content) {
            clipRect.set(0, 0, m_contentData.m_content->width(), m_contentData.m_content->height());
            canvas->clipRect(clipRect, SkRegion::kDifference_Op);
        }
        canvas->drawColor(m_request.color);
        canvas->restoreToCount(sc);

        if (existingRegion) {
            SkRegion* clip = m_contentData.m_contentInvalidRegion.get();
            SkRegion* transformedClip = transformContentClip(*clip, s, dx, dy);

            if (transformedClip) {
                canvas->clipRegion(*transformedClip);
                delete transformedClip;
                const SkRegion& totalClip = canvas->getTotalClip();
                if (totalClip.isEmpty()) {
                    LOG_RENDER("renderToBackingStoreRegion exiting because outside clip region");
                    bitmap->unlock();
                    return;
                }
            }
        }

        if (m_contentData.m_content) {
            m_contentData.m_content->draw(canvas
#if ENABLE(COLOR_INVERSION)
                , m_request.invertColor);
#else
                );
#endif
        }

        if (m_contentData.m_picture && m_contentData.m_picture->width() > 0) {
            LOG_RENDER("rendering picture %p\n", m_contentData.m_picture);
            canvas->drawPicture(*(m_contentData.m_picture));
        }
        bitmap->unlock();
    }

private:
    SkRegion* transformContentClip(SkRegion& rgn, float scale, float dx, float dy)
    {
        SkRegion* clip = new SkRegion;
        SkRegion::Iterator iter(rgn);
        int num = 0;
        while (!iter.done()) {
            SkIRect rect = iter.rect();
            rect.fLeft = static_cast<int32_t>(floor(rect.fLeft*scale + dx));
            rect.fTop = static_cast<int32_t>(floor(rect.fTop*scale + dy));
            rect.fRight = static_cast<int32_t>(ceil(rect.fRight*scale + dx));
            rect.fBottom = static_cast<int32_t>(ceil(rect.fBottom*scale + dy));
            clip->op(rect, SkRegion::kUnion_Op);
            iter.next();
            ++num;
        }
        LOG_RENDER("scaleContentClip - created clip region of %d rectangles", num);
        return clip;
    }

    bool detectInteractiveZoom(RenderTask& request)
    {
        bool ret = false;

        if (RendererConfig::getConfig()->enableZoom && !m_request.useGL && m_request.valid && (request.contentScale != m_request.contentScale) && request.quality == RenderTask::LOW) {
                LOG_RENDER("Renderer client detected interactive zoom");
                ret = true;
        } else if (m_request.useGL || !RendererConfig::getConfig()->enableZoom) {
            if (request.contentScale != m_lastScale)
                m_numTimesScaleUnchanged = 0;
            else if (m_numTimesScaleUnchanged < 5)
                    ++m_numTimesScaleUnchanged;

            if (m_request.valid && (m_numTimesScaleUnchanged < 3)) {
                LOG_RENDER("Renderer client detected interactive zoom");
                request.needRedraw = true;
                ret = true;
            }
        }
        m_lastScale = request.contentScale;
        return ret;
    }

    void generateRequest(SkCanvas* canvas, SkColor color, bool invertColor, RenderTask& task)
    {
        const SkRegion& clip = canvas->getTotalClip();
        const SkMatrix & matrix = canvas->getTotalMatrix();
        SkRegion* region = new SkRegion(clip);

        SkIRect clipBound = clip.getBounds();

        SkDevice* device = canvas->getDevice();
        const SkBitmap& bitmap = device->accessBitmap(true);

        SkDrawFilter* f = canvas->getDrawFilter();
        bool filterBitmap = true;
        if (f) {
            SkPaint tmpPaint;
            tmpPaint.setFilterBitmap(true);
            f->filter(canvas, &tmpPaint, SkDrawFilter::kBitmap_Type);
            filterBitmap = tmpPaint.isFilterBitmap();
        }

        task.requestArea.x1 = -matrix.getTranslateX() + clipBound.fLeft;
        task.requestArea.y1 = -matrix.getTranslateY() + clipBound.fTop;
        task.requestArea.x2 = -matrix.getTranslateX()+ clipBound.fRight;
        task.requestArea.y2 = -matrix.getTranslateY() + clipBound.fBottom;
        task.contentScale = matrix.getScaleX();
        task.contentOrigin.fX = -matrix.getTranslateX();
        task.contentOrigin.fY = -matrix.getTranslateY();
        task.viewportWidth = bitmap.width();
        task.viewportHeight = bitmap.height();
        task.config = bitmap.getConfig();
        task.color = color;
        task.invertColor = invertColor;
        task.quality = (filterBitmap)? RenderTask::HIGH : RenderTask::LOW;
        task.valid = true;
        task.newContent = 0;
        task.canAbort = true;
        task.useGL = false;
        task.needGLSync = false;
        task.needRedraw = false;
    }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
    void generateRequestGL(WebCore::IntRect& viewRect, SkRect& contentRect, float scale, SkColor color, RenderTask& task)
    {
        EGLDisplay display = eglGetCurrentDisplay();
        EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);

        int viewportWidth, viewportHeight, viewX, viewY;
        EGLint value;
        eglQuerySurface(display, surface, EGL_WIDTH, &value);
        viewportWidth = static_cast<int>(value);
        eglQuerySurface(display, surface, EGL_HEIGHT, &value);
        viewportHeight = static_cast<int>(value);

        LOG_RENDER("generateRequestGL viewport size = (%d x %d)", viewportWidth, viewportHeight);

        SkRect scaledContentRect;
        scaledContentRect.fLeft = contentRect.fLeft * scale;
        scaledContentRect.fRight = contentRect.fRight * scale;
        scaledContentRect.fTop = contentRect.fTop * scale;
        scaledContentRect.fBottom = contentRect.fBottom * scale;

        viewX = viewRect.x();
        viewY = (viewportHeight - viewRect.y()) - viewRect.height();
        task.requestArea.x1 = scaledContentRect.fLeft;
        task.requestArea.y1 = scaledContentRect.fTop;
        task.requestArea.x2 = scaledContentRect.fRight;
        task.requestArea.y2 = scaledContentRect.fBottom;
        if ((task.requestArea.x2 - task.requestArea.x1) > viewportWidth)
            task.requestArea.x2 = task.requestArea.x1 + viewportWidth;
        if ((task.requestArea.y2 - task.requestArea.y1) > viewportHeight)
            task.requestArea.y2 = task.requestArea.y1 + viewportHeight;
        task.contentScale = scale;
        task.contentOrigin.fX = scaledContentRect.fLeft - viewX;
        task.contentOrigin.fY = scaledContentRect.fTop - viewY;
        task.viewportWidth = viewportWidth;
        task.viewportHeight = viewportHeight;
        task.config = SkBitmap::kARGB_8888_Config;
        task.color = color;
        task.invertColor = false;
        task.quality = RenderTask::HIGH;
        task.valid = true;
        task.newContent = 0;
        task.canAbort = false;
        task.useGL = true;
        task.needGLSync = false;
        task.needRedraw = false;
    }
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    bool onDraw(RenderTask& request, SkCanvas* canvas)
    {
        double startTime =  WTF::currentTimeMS();

        bool drawn = false;
        bool shouldUpdate = true;
        bool abort = false;

        bool interactiveZoom = detectInteractiveZoom(request);
        if (interactiveZoom && (!m_request.valid || !RendererConfig::getConfig()->enableZoom) && m_request.canAbort) {
            shouldUpdate = false;
            if (m_backingStore)
                m_backingStore->invalidate();
        }

        bool result = false;
        if (!m_doPartialRender || request.quality == RenderTask::HIGH)
            handleNewContent(request);

        if (shouldUpdate) {
            bool changingBuffer = m_request.valid && (request.config != m_request.config
                           || request.viewportWidth != m_request.viewportWidth
                           || request.viewportHeight != m_request.viewportHeight);

            result = RenderRequest((interactiveZoom && !changingBuffer)? m_request : request);
        }

        if (result && !drawn)
            drawn = drawResult(canvas, request);

        if (!drawn)
            finish();

        double endTime =  WTF::currentTimeMS();
        double elapsedTime = endTime-startTime;
        LOG_PERF("drawContent %s %s %s took %f msec.",
            (request.newContent)? "(with new content)" : "",
            (m_loading)? "(loading)" : "",
            (drawn)? "" : "aborted and",
            elapsedTime);

        return drawn;
    }

    void handleNewContent(RenderTask& task)
    {
        task.newContent = false;
        if (m_contentData.numContentChanged() > 0 || m_contentData.numPictureChanged() > 0) {
            task.newContent = (m_contentData.numContentChanged() + m_contentData.numPictureChanged());
            if (m_backingStore)
                m_backingStore->finish();
            if (task.useGL)
                task.needGLSync = true;
            m_contentData.changeToNewContent();
            if (m_contentData.m_invalidateAll && !m_doPartialRender) {
                if (m_backingStore)
                    m_backingStore->invalidate();
                m_contentData.m_invalidateAll = false;
            }
        }
    }

    bool RenderRequest(RenderTask& request)
    {
        bool result = false;

        if (!m_library)
            return false;

        if (!m_backingStore) {
            const char* version = m_getBackingStoreVersion();
            LOG_RENDER("creating WebTech BackingStore - version %s", version);
            int vlen = strlen(WEBTECH_BACKINGSTORE_VERSION);
            int cmp = strncmp(version, WEBTECH_BACKINGSTORE_VERSION, vlen);
            if (cmp)
                LOG_RENDER("WARNING:  WebTech BackingStore version is wrong. Looking for version %s", WEBTECH_BACKINGSTORE_VERSION);

            m_backingStore = m_createBackingStore(static_cast<WebTech::IBackingStore::IUpdater*>(this));
            if (m_backingStore) {
                m_backingStore->setParam(WebTech::IBackingStore::LOG_DEBUG, s_log);
                m_backingStore->setParam(WebTech::IBackingStore::LOG_PERFORMANCE, s_logPerf);
                m_backingStore->setParam(WebTech::IBackingStore::ALLOW_SPECULATIVE_UPDATE, (RendererConfig::getConfig()->enableSpeculativeUpdate)? 1 : 0);
            }
        }

        if (!m_backingStore)
            return false;

        if (m_backingStore->checkError()) {
            m_backingStore->release();
            m_backingStore = 0;
            return false;
        }

        m_backingStore->setParam(WebTech::IBackingStore::ALLOW_INPLACE_SCROLL, request.useGL? 0 : 1);
        m_backingStore->setParam(WebTech::IBackingStore::ALLOW_PARTIAL_RENDER, m_doPartialRender? 1 : 0);
        m_backingStore->setParam(WebTech::IBackingStore::QUALITY, request.quality == RenderTask::HIGH? 1 : 0);
        m_backingStore->setParam(WebTech::IBackingStore::ALLOW_ABORT, request.canAbort? 1 : 0);
        m_backingStore->setParam(WebTech::IBackingStore::ALLOW_DELAYED_CLEANUP, request.useGL? 0 : 1);

        // see if we need to invalidate
        if (request.contentScale != m_request.contentScale
            || request.config != m_request.config
            || request.viewportWidth != m_request.viewportWidth
            || request.viewportHeight != m_request.viewportHeight) {
            m_backingStore->invalidate();
            if (request.useGL)
                request.needGLSync = true;
            m_contentData.m_contentInvalidRegion.clear();
        }

        if (m_loading != m_contentData.m_loading) {
            m_loading = m_contentData.m_loading;
            m_backingStore->setParam(WebTech::IBackingStore::PRIORITY, (m_loading)? -1 : 0);
        }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
        if (request.useGL && request.needGLSync) {
            LOG_RENDER("RenderRequest glFinish");
            glFinish();
        }
#endif // #if ENABLE(GPU_ACCELERATED_SCROLLING)

        m_request = request;
        result = m_backingStore->update(&(request.requestArea),
                                (m_contentData.m_contentInvalidRegion)? WebTech::IBackingStore::UPDATE_ALL : WebTech::IBackingStore::UPDATE_EXPOSED_ONLY,
                                request.contentOrigin.fX, request.contentOrigin.fY,
                                request.viewportWidth, request.viewportHeight,
                                static_cast<int>(ceil(m_contentData.m_contentWidth * request.contentScale)),
                                static_cast<int>(ceil(m_contentData.m_contentHeight * request.contentScale)),
                                request.contentScale,
                                request.newContent
                                );

        if (result) {
            m_contentData.m_contentInvalidRegion.clear();
        }

        return result;
    };

    // draw sub-region in backing store onto output
    void drawAreaToOutput(SkCanvas* srcCanvas,
        int outWidth, int outHeight, int outPitch, void* outPixels, SkBitmap::Config outConfig,
        float scale,
        SkPaint& paint,
        WebTech::IBackingStore::IDrawRegionIterator* iter,
        bool noClipping)
    {
        SkIPoint o;
        o.fX = iter->outX();
        o.fY = iter->outY();
        SkIPoint i;
        i.fX = iter->inX();
        i.fY = iter->inY();
        int width = iter->width();
        int height = iter->height();

        if (!iter->buffer()) {
            LOG_RENDER("drawAreaToOutput: no buffer");
            return;
        }
        BackingStoreBuffer* bufferImpl = static_cast<BackingStoreBuffer*>(iter->buffer());
        const SkBitmap& backingStoreBitmap = bufferImpl->getBitmap();

        SkBitmap bitmap;
        int inPitch = backingStoreBitmap.rowBytes();
        SkBitmap::Config inConfig = backingStoreBitmap.getConfig();
        bitmap.setConfig(inConfig, width, height, inPitch);
        char* pixels = static_cast<char*>(backingStoreBitmap.getPixels());
        int bpp = backingStoreBitmap.bytesPerPixel();
        pixels = pixels + i.fY * inPitch + i.fX * bpp;
        bitmap.setPixels(static_cast<void*>(pixels));

        // do memcpy instead of using SkCanvas to draw if possible
        if (noClipping && scale == 1.0f && outConfig == inConfig) {
            if (!i.fX && !o.fX && width == outWidth && height == outHeight && outPitch == inPitch) {
                    char* optr = static_cast<char*>(outPixels);
                    optr = optr + o.fY*outPitch;
                    char* iptr = static_cast<char*>(pixels);
                    memcpy(static_cast<void*>(optr), static_cast<void*>(iptr), height*outPitch);
            } else {
                int w = width * bpp;
                int h = height;
                char* optr = static_cast<char*>(outPixels);
                optr = optr + o.fY*outPitch + o.fX*bpp;
                char* iptr = static_cast<char*>(pixels);
                for (int y = 0; y < h; ++y) {
                    memcpy(static_cast<void*>(optr), static_cast<void*>(iptr), w);
                    optr += outPitch;
                    iptr += inPitch;
                }
            }
        } else {
            SkRect rect;
            rect.set(o.fX, o.fY, o.fX + bitmap.width(), o.fY + bitmap.height());
            srcCanvas->drawBitmapRect(bitmap, 0, rect, &paint);
        }
    }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
    void drawAreaToOutputGL(RenderTask& request, float scale, WebTech::IBackingStore::IDrawRegionIterator* iter, WebTech::IBackingStore::UpdateRegion& areaAvailable)
    {
        LOG_RENDER("RendererImpl::drawAreaToOutputGL");

        SkIPoint o;
        o.fX = iter->outX();
        o.fY = iter->outY();
        SkIPoint i;
        i.fX = iter->inX();
        i.fY = iter->inY();
        int width = areaAvailable.x2 - areaAvailable.x1;
        int height = areaAvailable.y2 - areaAvailable.y1;

        if (!iter->buffer()) {
            LOG_RENDER("drawAreaToOutputGL: no buffer");
            return;
        }
        BackingStoreBuffer* bufferImpl = static_cast<BackingStoreBuffer*>(iter->buffer());

        GLuint texture = bufferImpl->bindTexture();

        if (m_bufferInUse != bufferImpl) {
            bufferImpl->addRef();
            if (m_bufferInUse) {
                LOG_RENDER("drawAreaToOutputGL glFinish");
                glFinish();
                m_bufferInUse->release();
            }
            m_bufferInUse = bufferImpl;
        }

        m_2DRenderer->init(0, 0, request.viewportWidth, request.viewportHeight);

        LOG_RENDER("    draw bitmap (size= %d x %d) input area (%d, %d) - (%d, %d) to output area (%d, %d) - (%d, %d)",
            bufferImpl->getBitmap().width(), bufferImpl->getBitmap().height(),
            i.fX, i.fY, i.fX + width, i.fY + height,
            (int)(o.fX * scale), (int)(o.fY * scale), (int)((o.fX + width) * scale), (int)((o.fY + height) * scale));

        m_2DRenderer->drawTexture(texture,
            bufferImpl->getBitmap().width(), bufferImpl->getBitmap().height(),
            i.fX, i.fY, i.fX + width, i.fY + height,
            (int)(o.fX * scale), (int)(o.fY * scale), (int)((o.fX + width) * scale), (int)((o.fY + height) * scale),
            (scale != 1.0f), false);
    }
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    // draw valid region received from render thread to output.  The regions can be broken down
    // into sub-regions
    bool drawResult(SkCanvas* srcCanvas, RenderTask& request)
    {
        bool ret = m_doPartialRender;
        if (!m_backingStore)
            return ret;

        WebTech::IBackingStore::UpdateRegion areaToDraw = request.requestArea;
        SkIPoint contentOrigin = request.contentOrigin;
        float deltaScale = 1.0f;
        if (m_request.contentScale != request.contentScale) {
            if (request.canAbort && request.quality >= 1) {
                LOG_RENDER("Renderer client can't zoom result in high quality.  should wait.");
                return false;
            }
            if (request.canAbort && !RendererConfig::getConfig()->enableZoom) {
                LOG_RENDER("Renderer client aborted due to changing zoom");
                return false;
            }
            deltaScale = m_request.contentScale / request.contentScale;
            areaToDraw.x1 *= deltaScale;
            areaToDraw.y1 *= deltaScale;
            areaToDraw.x2 *= deltaScale;
            areaToDraw.y2 *= deltaScale;
            contentOrigin.fX *= deltaScale;
            contentOrigin.fY *= deltaScale;
        }

        LOG_RENDER("drawResult.  delta scale = %f", deltaScale);

        WebTech::IBackingStore::UpdateRegion areaAvailable;
        WebTech::IBackingStore::RegionAvailability availability = m_backingStore->canDrawRegion(areaToDraw, areaAvailable);
        bool allDrawn;
        if (m_doPartialRender)
            allDrawn = (availability >= WebTech::IBackingStore::FULLY_AVAILABLE);
        else
            allDrawn = (availability == WebTech::IBackingStore::FULLY_AVAILABLE);

        request.needRedraw |= (availability != WebTech::IBackingStore::FULLY_AVAILABLE);

        LOG_RENDER("drawing viewport area (%d, %d) to (%d, %d).  All valid in backing store: %d",
            areaToDraw.x1, areaToDraw.y1, areaToDraw.x2, areaToDraw.y2,
            (allDrawn)?1:0);
        if (!allDrawn && request.canAbort)
            return false;

        if (!request.useGL)
            drawResultSW(srcCanvas, request, areaAvailable, deltaScale, contentOrigin, ret);
#if ENABLE(GPU_ACCELERATED_SCROLLING)
        else
            drawResultGL(request, areaAvailable, deltaScale, contentOrigin, (availability == WebTech::IBackingStore::FULLY_AVAILABLE)? false : true, ret);
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)
        return ret;

    }

    void drawResultSW(SkCanvas* srcCanvas, RenderTask& request, WebTech::IBackingStore::UpdateRegion& areaAvailable, float deltaScale, SkIPoint& contentOrigin, bool& ret)
    {
        bool simpleClip = false;
        const SkRegion& clip = srcCanvas->getTotalClip();
        SkIRect clipBound = clip.getBounds();
        if (clip.isRect())
            simpleClip = true;

        SkPaint paint;
        paint.setFilterBitmap(false);
        paint.setDither(false);
        paint.setAntiAlias(false);
        paint.setColor(0xffffff);
        paint.setAlpha(255);
        paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

        srcCanvas->save();
        srcCanvas->setDrawFilter(0);

        srcCanvas->resetMatrix();

        srcCanvas->scale(1.0f / deltaScale, 1.0f / deltaScale);

        SkDevice* device = srcCanvas->getDevice();
        const SkBitmap& bitmap = device->accessBitmap(true);

        int outWidth = bitmap.width();
        int outHeight = bitmap.height();
        void* outPixels = bitmap.getPixels();
        int outPitch = bitmap.rowBytes();
        SkBitmap::Config outConfig = bitmap.getConfig();

        WebTech::IBackingStore::IDrawRegionIterator* iter = m_backingStore->beginDrawRegion(areaAvailable, contentOrigin.fX, contentOrigin.fY);
        if (iter) {
            do {
                drawAreaToOutput(srcCanvas, outWidth, outHeight, outPitch, outPixels, outConfig,
                            1.0f / deltaScale, paint, iter, simpleClip);
            } while (iter->next());
            iter->release();
            ret = true;
        } else
            ret = m_doPartialRender;

        srcCanvas->restore();

    }

#if ENABLE(GPU_ACCELERATED_SCROLLING)
    void drawResultGL(RenderTask& request, WebTech::IBackingStore::UpdateRegion& areaAvailable, float deltaScale, SkIPoint& contentOrigin, bool clear, bool& ret)
    {
        if (clear) {
            glClearColor((float)SkColorGetR(request.color) / 255.0,
                (float)SkColorGetG(request.color) / 255.0,
                (float)SkColorGetB(request.color) / 255.0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        WebTech::IBackingStore::IDrawRegionIterator* iter = m_backingStore->beginDrawRegion(areaAvailable, contentOrigin.fX, contentOrigin.fY);
        if (iter) {
                drawAreaToOutputGL(request, 1.0f / deltaScale, iter, areaAvailable);
            iter->release();
            ret = true;
        } else
            ret = m_doPartialRender;
    }
#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

    int suggestContentSplitting(PictureSet& content, RenderTask& request)
    {
        unsigned int numBlocks = (content.height() / (request.viewportHeight * RendererConfig::getConfig()->splitSize));
        if (numBlocks>content.size()) {
            numBlocks /= content.size();
            int numSplit = 0;
            while (numBlocks>1) {
                ++numSplit;
                numBlocks = numBlocks >> 1;
            }
            LOG_RENDER("suggestContentSplitting: content length=%d.  num pictures=%d.  num split=%d",
                content.height(), content.size(), numSplit);
            return numSplit;
        }
        return 0;
    }

private:
    WebTech::IBackingStore* m_backingStore;
    ContentData m_contentData;
    RenderTask m_request;
    float m_lastScale;
    int m_numTimesScaleUnchanged;
    bool m_loading;
    bool m_doPartialRender;
#if ENABLE(GPU_ACCELERATED_SCROLLING)
    WebTech::IRenderer2D* m_2DRenderer;
    EGLSyncKHR m_syncObject;
#endif // GPU_ACCELERATED_SCROLLING
    BackingStoreBuffer* m_bufferInUse;
    void* m_library;
    WebTech::CreateBackingStore_t m_createBackingStore;
    WebTech::GetBackingStoreVersion_t m_getBackingStoreVersion;
#if ENABLE(GPU_ACCELERATED_SCROLLING)
    WebTech::CreateRenderer2D_t m_createRenderer2D;
#endif
};

RendererImpl::RendererConfig* RendererImpl::RendererConfig::getConfig()
{
static RendererImpl::RendererConfig s_config;
    return &s_config;
}

#if ENABLE(GPU_ACCELERATED_SCROLLING)
////////////////////////// GLResourceUsers implementation///////////////////
RendererImpl::GLResourceUsers::GLResourceUsers()
    : m_curTime(0)
{
    for (int i=0; i < NUM_GL_RESOURCE_RENDERERS; ++i) {
        m_used[i] = false;
        m_renderers[i] = 0;
    }
}

RendererImpl::GLResourceUsers::~GLResourceUsers()
{

}

RendererImpl::GLResourceUsers* RendererImpl::GLResourceUsers::getGLResourceUsers()
{
static RendererImpl::GLResourceUsers s_GLResourceUsers;
    return &s_GLResourceUsers;
}

void RendererImpl::GLResourceUsers::getGLResource(RendererImpl* renderer)
{
    int i;
    for (int i=0; i < NUM_GL_RESOURCE_RENDERERS; ++i) {
        if (m_used[i] && m_renderers[i] == renderer) {
            incrementTimestamp();
            m_timeStamp[i] = m_curTime;
            return;
        }
    }
    LOG_RENDER("GLResourceUsers::getGLResource");
    int unused = -1;
    unsigned int minTimestamp = 0xffffffff;
    for (i=0; i < NUM_GL_RESOURCE_RENDERERS; ++i) {
        if (!m_used[i]) {
            unused = i;
            break;
        }
        if (m_timeStamp[i] < minTimestamp) {
            minTimestamp = m_timeStamp[i];
            unused = i;
        }
    }

    if (m_used[unused] && m_renderers[unused])
        m_renderers[unused]->releaseGLMemory();

    m_used[unused] = false;

    incrementTimestamp();

    m_renderers[unused] = renderer;
    if (renderer)
        m_used[unused] = true;
    m_timeStamp[unused] = m_curTime;
}

void RendererImpl::GLResourceUsers::releaseGLResource(RendererImpl* renderer)
{
    LOG_RENDER("GLResourceUsers::releaseGLResource");
    for (int i=0; i < NUM_GL_RESOURCE_RENDERERS; ++i) {
        if (m_used[i] && m_renderers[i] == renderer) {
            m_used[i] = false;
            m_renderers[i] = 0;
            break;
        }
    }
}

////////////////////////// GLBackingStoreBufferManager implementation///////////////////

RendererImpl::GLBackingStoreBufferManager::GLBackingStoreBufferManager()
    : m_bufferPortrait(0)
    , m_bufferLandscape(0)
{
    m_composer = (ComposerService::getComposerService());
    if (m_composer == 0)
        LOG_RENDER("cannot create composer");
    m_graphicBufferAlloc = m_composer->createGraphicBufferAlloc();
    if (m_graphicBufferAlloc == 0)
        LOG_RENDER("cannot create GraphicBufferAlloc");
}

RendererImpl::GLBackingStoreBufferManager::~GLBackingStoreBufferManager()
{

}

RendererImpl::GLBackingStoreBufferManager* RendererImpl::GLBackingStoreBufferManager::getGLBackingStoreBufferManager()
{
static RendererImpl::GLBackingStoreBufferManager s_GLBackingStoreBufferManager;
    return &s_GLBackingStoreBufferManager;
}

RendererImpl::BackingStoreBuffer* RendererImpl::GLBackingStoreBufferManager::getBuffer(int w, int h)
{
    GLBackingStoreBufferManager* mgr = GLBackingStoreBufferManager::getGLBackingStoreBufferManager();
    RendererImpl::BackingStoreBuffer* buffer;
    if (m_graphicBufferAlloc.get()) {
        if (w > h) {
            if (m_bufferPortrait) {
                if (m_bufferPortrait->width() != w || m_bufferPortrait->height() != h) {
                    m_bufferPortrait->release();
                    m_bufferPortrait = 0;
                }
            }
            if (!m_bufferPortrait) {
                LOG_RENDER("createBuffer: creating portrait buffer");
                m_bufferPortrait = new RendererImpl::BackingStoreBuffer(w, h, SkBitmap::ComputeBytesPerPixel(SkBitmap::kARGB_8888_Config), m_graphicBufferAlloc.get());
            }
            m_bufferPortrait->addRef();
            buffer = m_bufferPortrait;
        } else {
            if (m_bufferLandscape) {
                if (m_bufferLandscape->width() != w || m_bufferLandscape->height() != h) {
                    m_bufferLandscape->release();
                    m_bufferLandscape = 0;
                }
            }
            if (!m_bufferLandscape) {
                LOG_RENDER("createBuffer: creating landscape buffer");
                m_bufferLandscape = new RendererImpl::BackingStoreBuffer(w, h, SkBitmap::ComputeBytesPerPixel(SkBitmap::kARGB_8888_Config), m_graphicBufferAlloc.get());
            }
            m_bufferLandscape->addRef();
            buffer = m_bufferLandscape;
        }

        return buffer;
    }

    return 0;
}

#endif // ENABLE(GPU_ACCELERATED_SCROLLING)

int RendererImpl::BackingStoreBuffer::s_numBuffers = 0;

} // namespace RendererImplNamespace

using namespace RendererImplNamespace;
namespace android {
Renderer* android::Renderer::createRenderer()
{
    return static_cast<Renderer*>(new RendererImpl());
}

}

#endif // ACCELERATED_SCROLLING
