// frameworks/native/libs/drm_utils/DrmUtils.cpp

#define LOG_TAG "DrmUtils"
#include <log/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <DrmUtils.h>
#include <errno.h> 

// 私有上下文结构
struct DrmContext {
    int fd = -1;
    uint32_t crtc_id = 0;
    uint32_t connector_id = 0;
    drmModeModeInfo mode = {};
    bool initialized = false;
};

DrmContext* drm_context_create() {
    return new DrmContext();
}

int drm_context_init(DrmContext* ctx) {
    if (!ctx || ctx->initialized) {
        return -1;
    }

    // 打开 DRM 设备
    ctx->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (ctx->fd < 0) {
        ALOGE("[zizhan] Failed to open DRM device: %s", strerror(errno));
        return -1;
    }

    // 获取 DRM 资源
    drmModeRes* resources = drmModeGetResources(ctx->fd);
    if (!resources) {
        ALOGE("[zizhan] drmModeGetResources failed");
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    // 查找连接的显示器
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector* conn = drmModeGetConnector(ctx->fd, resources->connectors[i]);
        if (!conn) continue;
        
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            ctx->connector_id = conn->connector_id;
            ctx->mode = conn->modes[0];  // 使用第一个显示模式
            ALOGI("[zizhan] Found connector %u with mode: %s (%dx%d)", 
                  ctx->connector_id, ctx->mode.name, 
                  ctx->mode.hdisplay, ctx->mode.vdisplay);
            break;
        }
        drmModeFreeConnector(conn);
    }

    // 查找可用的 CRTC
    if (ctx->connector_id != 0) {
        for (int i = 0; i < resources->count_crtcs; i++) {
            drmModeCrtc* crtc = drmModeGetCrtc(ctx->fd, resources->crtcs[i]);
            if (!crtc) continue;
            
            if (crtc->mode_valid) {
                ctx->crtc_id = crtc->crtc_id;
                ALOGI("[zizhan] Using CRTC: %u", ctx->crtc_id);
            }
            drmModeFreeCrtc(crtc);
            
            if (ctx->crtc_id != 0) break;
        }
    }

    drmModeFreeResources(resources);

    if (ctx->connector_id == 0 || ctx->crtc_id == 0) {
        ALOGE("[zizhan] No suitable display configuration found");
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    ctx->initialized = true;
    return 0;
}

uint32_t drm_context_create_fb(DrmContext* ctx, int dma_buf_fd, int width, int height) {
    if (!ctx || !ctx->initialized || dma_buf_fd < 0) {
        ALOGE("[zizhan] Invalid arguments to createFB: fd=%d, dmaBufFd=%d", ctx->fd, dma_buf_fd);
        return 0;
    }

    // 检查 DRM 是否支持 dumb buffer
    uint64_t has_dumb;
    if (drmGetCap(ctx->fd, DRM_CAP_DUMB_BUFFER, &has_dumb) != 0 || !has_dumb) {
        ALOGE("[zizhan] DRM device does not support dumb buffers");
        return 0;
    }

    uint32_t fb_id = 0;
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    
    handles[0] = static_cast<uint32_t>(dma_buf_fd);
    pitches[0] = static_cast<uint32_t>(width * 4); // 假设32bpp
    
    int ret = drmModeAddFB2WithModifiers(ctx->fd, width, height,
                                       DRM_FORMAT_RGBA8888,
                                       handles, pitches, offsets,
                                       modifiers, &fb_id,
                                       DRM_MODE_FB_MODIFIERS);
    
    if (ret != 0) {
        ret = drmModeAddFB2(ctx->fd, width, height, DRM_FORMAT_RGBA8888,
                           handles, pitches, offsets, &fb_id, 0);
        if (ret != 0) {
            //ALOGE("[zizhan] Failed to create DRM FB: %s", strerror(errno));
            ALOGE("Failed to create DRM FB: %s (errno=%d)", strerror(errno), errno);
            ALOGE("Parameters: fd=%d, w=%d, h=%d, stride=%d, format=0x%x", 
                  ctx->fd, width, height, pitches[0], DRM_FORMAT_RGBA8888);
            return 0;
        }
    }
    
    ALOGV("[zizhan] Created DRM FB %u for buffer %dx%d", fb_id, width, height);
    return fb_id;
}

int drm_context_page_flip(DrmContext* ctx, uint32_t fb_id) {
    if (!ctx || !ctx->initialized || fb_id == 0) {
        return -1;
    }

    static bool first_frame = true;
    
    if (first_frame) {
        // 第一帧需要先设置 CRTC
        int ret = drmModeSetCrtc(ctx->fd, ctx->crtc_id, fb_id,
                               0, 0, &ctx->connector_id, 1, &ctx->mode);
        if (ret != 0) {
            ALOGE("[zizhan] drmModeSetCrtc failed: %s", strerror(errno));
            return ret;
        }
        first_frame = false;
        return 0;
    }
    
    int ret = drmModePageFlip(ctx->fd, ctx->crtc_id, fb_id, 
                             DRM_MODE_PAGE_FLIP_EVENT, nullptr);
    if (ret != 0) {
        ALOGE("[zizhan] drmModePageFlip failed: %s", strerror(errno));
    }
    else{
        ALOGI("[zizhan] drmModePageFlip secceed!!!!!!!!!!");
    }
    return ret;
}

void drm_context_destroy(DrmContext* ctx) {
    if (!ctx) return;
    
    if (ctx->fd >= 0) {
        close(ctx->fd);
    }
    delete ctx;
}