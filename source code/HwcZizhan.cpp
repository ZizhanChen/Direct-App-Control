/*
 * Copyright (C) 2025 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0
 */

#define LOG_TAG "HwcZizhan"
#include <android/log.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include <drm_fourcc.h>
#include <hardware/gralloc.h>

#include "HwcZizhan.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace android {
namespace zizhan {

// HAL pixel format -> DRM fourcc
static uint32_t halToDrmFourcc(uint32_t halFormat) {
    switch (halFormat) {
        case 1:  /* HAL_PIXEL_FORMAT_RGBA_8888 */ return DRM_FORMAT_ABGR8888;
        case 2:  /* HAL_PIXEL_FORMAT_RGBX_8888 */ return DRM_FORMAT_XBGR8888;
        case 3:  /* HAL_PIXEL_FORMAT_RGB_888  */ return DRM_FORMAT_BGR888;
        case 4:  /* HAL_PIXEL_FORMAT_RGB_565  */ return DRM_FORMAT_BGR565;
        case 5:  /* HAL_PIXEL_FORMAT_BGRA_8888*/ return DRM_FORMAT_ARGB8888;
        case 22: /* HAL_PIXEL_FORMAT_RGBA_FP16 */ return DRM_FORMAT_ABGR16161616F;
        case 0x11430: /* HAL_PIXEL_FORMAT_RGBA_1010102 */ return DRM_FORMAT_ABGR2101010;
        default:
            return DRM_FORMAT_ABGR8888; // 兜底
    }
}

static inline int bytesPerPixel(uint32_t halFormat) {
    switch (halFormat) {
        case 1: case 2: case 5: return 4;
        case 3: return 3;
        case 4: return 2;
        case 22: return 8;
        case 0x11430: return 4;
        default: return 4;
    }
}

// ----------------------------------------------------------------

HwcZizhan::HwcZizhan() = default;
HwcZizhan::~HwcZizhan() { destroy(); }

HwcZizhan& HwcZizhan::get() {
    static HwcZizhan inst;
    return inst;
}

bool HwcZizhan::init() {
    std::lock_guard<std::mutex> _l(mLock);
    if (mState.inited) return true;

    char value[PROPERTY_VALUE_MAX] = {0};
    if (property_get("debug.zizhan.hwc", value, "0") <= 0 || value[0] != '1') {
        LOGI("hwc disabled by property");
        return false;
    }

    if (!openDrmLocked())        { LOGE("openDrm failed");     return false; }
    if (!findPrimaryCrtcLocked()){ LOGE("no primary CRTC");    return false; }
    if (!findConnectorLocked())  { LOGE("no connector");       return false; }
    enumeratePlanesLocked();

    if (mState.planes.empty()) { LOGE("no plane available");  return false; }

    mState.inited = true;
    LOGI("HwcZizhan ready, crtc=%u conn=%u planes=%zu",
         mState.primaryCrtcId, mState.connectorId, mState.planes.size());
    return true;
}

bool HwcZizhan::openDrmLocked() {
    drmModeRes* res = nullptr;
    const char* paths[] = {"/dev/dri/card0", "/dev/dri/card1"};
    for (const char* p : paths) {
        int fd = open(p, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        res = drmModeGetResources(fd);
        if (res) {
            mState.drmFd = fd;
            drmModeFreeResources(res);
            drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
            drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
            drmSetMaster(fd);   // 需要 CAP_SYS_ADMIN 或 system uid
            LOGI("open %s ok", p);
            return true;
        }
        close(fd);
    }
    return false;
}

bool HwcZizhan::findPrimaryCrtcLocked() {
    drmModeRes* res = drmModeGetResources(mState.drmFd);
    if (!res) return false;

    for (int i = 0; i < res->count_crtcs; ++i) {
        mState.primaryCrtcId = res->crtcs[i];
        break;
    }
    if (mState.primaryCrtcId == 0) { drmModeFreeResources(res); return false; }

    mState.crtcActiveProp  = findPropertyLocked(mState.primaryCrtcId, "ACTIVE");
    mState.crtcModeIdProp  = findPropertyLocked(mState.primaryCrtcId, "MODE_ID");

    // 读当前 CRTC 的 mode，作为之后 commit 用的 mode blob
    drmModeCrtc* crtc = drmModeGetCrtc(mState.drmFd, mState.primaryCrtcId);
    if (crtc) {
        if (crtc->mode_valid) {
            drmModeCreatePropertyBlob(mState.drmFd, &crtc->mode,
                                      sizeof(crtc->mode), &mState.crtcModeId);
        }
        drmModeFreeCrtc(crtc);
    }
    drmModeFreeResources(res);
    return true;
}

bool HwcZizhan::findConnectorLocked() {
    drmModeRes* res = drmModeGetResources(mState.drmFd);
    if (!res) return false;

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector* conn = drmModeGetConnector(mState.drmFd, res->connectors[i]);
        if (!conn) continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            mState.connectorId = conn->connector_id;
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            mState.connCrtcIdProp = findPropertyLocked(mState.connectorId, "CRTC_ID");
            return true;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);
    return false;
}

uint32_t HwcZizhan::findPropertyLocked(uint32_t objectId, const char* name) {
    drmModeObjectProperties* props =
        drmModeObjectGetProperties(mState.drmFd, objectId, DRM_MODE_OBJECT_ANY);
    if (!props) return 0;
    uint32_t id = 0;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr p = drmModeGetProperty(mState.drmFd, props->props[i]);
        if (p) {
            if (strcmp(p->name, name) == 0) { id = p->prop_id; drmModeFreeProperty(p); break; }
            drmModeFreeProperty(p);
        }
    }
    drmModeFreeObjectProperties(props);
    return id;
}

void HwcZizhan::enumeratePlanesLocked() {
    drmModePlaneRes* pres = drmModeGetPlaneResources(mState.drmFd);
    if (!pres) return;

    for (uint32_t i = 0; i < pres->count_planes; ++i) {
        drmModePlane* p = drmModeGetPlane(mState.drmFd, pres->planes[i]);
        if (!p) continue;

        PlaneProps pp;
        pp.planeId       = p->plane_id;
        pp.possibleCrtcs = p->possible_crtcs;
        pp.isPrimary     = false;

        // 识别 plane type
        uint32_t typeProp = findPropertyLocked(pp.planeId, "type");
        if (typeProp) {
            drmModeObjectProperties* props =
                drmModeObjectGetProperties(mState.drmFd, pp.planeId, DRM_MODE_OBJECT_PLANE);
            if (props) {
                for (uint32_t k = 0; k < props->count_props; ++k) {
                    if (props->props[k] == typeProp) {
                        // DRM_PLANE_TYPE_PRIMARY = 1, OVERLAY = 0, CURSOR = 2
                        uint64_t v = props->prop_values[k];
                        pp.isPrimary = (v == 1);
                        break;
                    }
                }
                drmModeFreeObjectProperties(props);
            }
        }

        pp.propFbId    = findPropertyLocked(pp.planeId, "FB_ID");
        pp.propCrtcId  = findPropertyLocked(pp.planeId, "CRTC_ID");
        pp.propCrtcX   = findPropertyLocked(pp.planeId, "CRTC_X");
        pp.propCrtcY   = findPropertyLocked(pp.planeId, "CRTC_Y");
        pp.propCrtcW   = findPropertyLocked(pp.planeId, "CRTC_W");
        pp.propCrtcH   = findPropertyLocked(pp.planeId, "CRTC_H");
        pp.propSrcX    = findPropertyLocked(pp.planeId, "SRC_X");
        pp.propSrcY    = findPropertyLocked(pp.planeId, "SRC_Y");
        pp.propSrcW    = findPropertyLocked(pp.planeId, "SRC_W");
        pp.propSrcH    = findPropertyLocked(pp.planeId, "SRC_H");
        pp.propAlpha   = findPropertyLocked(pp.planeId, "alpha");
        pp.propBlend   = findPropertyLocked(pp.planeId, "pixel_blend_mode");
        pp.propZpos    = findPropertyLocked(pp.planeId, "zpos");
        pp.propInFence = findPropertyLocked(pp.planeId, "IN_FENCE_FD");

        // 必须能配到本 CRTC 上
        const uint32_t crtcIndex = __builtin_ctz(mState.primaryCrtcId);
        (void)crtcIndex;
        if ((pp.possibleCrtcs & (1u << 0)) == 0 &&
            pp.possibleCrtcs != 0) {
            // 如果 bit0 不匹配本 CRTC，需要更多逻辑，简化处理：只要非零就接受
        }

        mState.planes.push_back(pp);
        drmModeFreePlane(p);
    }
    drmModeFreePlaneResources(pres);

    // Primary plane 放前面，overlay 之后按 zpos 能力简单排序
    std::stable_sort(mState.planes.begin(), mState.planes.end(),
        [](const PlaneProps& a, const PlaneProps& b){
            return a.isPrimary > b.isPrimary;
        });
}

// ----------------------------------------------------------------

uint32_t HwcZizhan::pickPlaneForLayerLocked(const LayerDesc& l) const {
    // 简单策略：第一个用 Primary，其余用 Overlay。
    // 若 Overlay 不够，后面会返回 0 导致提交失败（该 layer 被忽略）。
    bool havePrimary = false;
    for (const auto& lp : mState.layers) {
        if (lp.second.planeId != 0) {
            for (const auto& pp : mState.planes) {
                if (pp.planeId == lp.second.planeId && pp.isPrimary) {
                    havePrimary = true;
                    break;
                }
            }
        }
    }
    if (!havePrimary) {
        for (const auto& pp : mState.planes) {
            if (pp.isPrimary) return pp.planeId;
        }
    }
    for (const auto& pp : mState.planes) {
        if (pp.isPrimary) continue;
        // 已被其他 layer 占用？
        bool used = false;
        for (const auto& lp : mState.layers) {
            if (lp.second.planeId == pp.planeId) { used = true; break; }
        }
        if (!used) return pp.planeId;
    }
    return 0;
}

int64_t HwcZizhan::addOrUpdateLayer(uint64_t key,
                                    int left, int top, int width, int height, int zorder) {
    std::lock_guard<std::mutex> _l(mLock);
    if (!mState.inited) return -1;

    auto it = mState.keyToLayer.find(key);
    if (it != mState.keyToLayer.end()) {
        LayerDesc& l = mState.layers[it->second];
        l.left = left; l.top = top; l.width = width; l.height = height; l.zorder = zorder;
        LOGI("update layerId=%" PRId64 " key=%" PRIu64 " (%d,%d %dx%d z=%d)",
             l.layerId, key, left, top, width, height, zorder);
        return l.layerId;
    }

    LayerDesc l;
    l.layerId = mState.nextLayerId++;
    l.key     = key;
    l.left    = left; l.top = top; l.width = width; l.height = height; l.zorder = zorder;
    l.planeId = pickPlaneForLayerLocked(l);
    if (l.planeId == 0) {
        LOGW("no plane available for key=%" PRIu64, key);
        return -1;
    }
    mState.layers.emplace(l.layerId, l);
    mState.keyToLayer.emplace(key, l.layerId);
    LOGI("add layerId=%" PRId64 " key=%" PRIu64 " plane=%u (%d,%d %dx%d z=%d)",
         l.layerId, key, l.planeId, left, top, width, height, zorder);
    return l.layerId;
}

bool HwcZizhan::updateLayerBuffer(int64_t layerId,
                                  int dmabufFd,
                                  uint64_t bufferId,
                                  int width, int height,
                                  uint32_t halFormat,
                                  uint32_t pitchBytes,
                                  int inFenceFd) {
    std::lock_guard<std::mutex> _l(mLock);
    if (!mState.inited) return false;
    auto it = mState.layers.find(layerId);
    if (it == mState.layers.end()) return false;

    uint32_t fbId = getOrCreateFbLocked(dmabufFd, bufferId,
                                        width, height, halFormat, pitchBytes);
    if (fbId == 0) return false;

    LayerDesc& l = it->second;
    if (l.lastInFd >= 0) close(l.lastInFd);
    l.lastInFd = inFenceFd;    // 调用方 dup 出来的
    l.fbId = fbId;
    l.fbBufferKey = bufferId;
    // 实际用的 width/height 可能比 layer 大（缩放），暂不记录
    return true;
}

bool HwcZizhan::removeLayer(int64_t layerId) {
    std::lock_guard<std::mutex> _l(mLock);
    auto it = mState.layers.find(layerId);
    if (it == mState.layers.end()) return false;
    uint64_t key = it->second.key;
    mState.layers.erase(it);
    mState.keyToLayer.erase(key);
    drainFbCacheLocked();
    LOGI("remove layerId=%" PRId64 " key=%" PRIu64, layerId, key);
    return true;
}

bool HwcZizhan::removeLayerByKey(uint64_t key) {
    std::lock_guard<std::mutex> _l(mLock);
    auto it = mState.keyToLayer.find(key);
    if (it == mState.keyToLayer.end()) return false;
    int64_t layerId = it->second;
    mState.layers.erase(layerId);
    mState.keyToLayer.erase(it);
    drainFbCacheLocked();
    LOGI("remove key=%" PRIu64 " (layerId=%" PRId64 ")", key, layerId);
    return true;
}

uint32_t HwcZizhan::getOrCreateFbLocked(int dmabufFd, uint64_t bufferId,
                                        int width, int height,
                                        uint32_t halFormat, uint32_t pitchBytes) {
    auto it = mState.fbCache.find(bufferId);
    if (it != mState.fbCache.end()) return it->second;

    uint32_t fourcc = halToDrmFourcc(halFormat);
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {pitchBytes, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    uint64_t modifiers[4] = {0, 0, 0, 0};
    uint32_t flags = 0;

    int ret = drmPrimeFDToHandle(mState.drmFd, dmabufFd, &handles[0]);
    if (ret) {
        LOGE("drmPrimeFDToHandle failed: %s", strerror(errno));
        return 0;
    }

    uint32_t fbId = 0;
    if (drmModeAddFB2WithModifiers(mState.drmFd,
                                   width, height, fourcc,
                                   handles, pitches, offsets, modifiers,
                                   &fbId, flags) != 0) {
        // modifier 不支持就退到 AddFB2
        if (drmModeAddFB2(mState.drmFd, width, height, fourcc,
                          handles, pitches, offsets, &fbId, 0) != 0) {
            LOGE("drmModeAddFB2 failed: %s", strerror(errno));
            drmCloseBufferHandle(mState.drmFd, handles[0]);
            return 0;
        }
    }

    mState.fbCache.emplace(bufferId, fbId);
    LOGI("new fb %u for bufferId=%" PRIu64 " %dx%d pitch=%u fourcc=%c%c%c%c",
         fbId, bufferId, width, height, pitchBytes,
         (char)(fourcc & 0xff), (char)((fourcc >> 8) & 0xff),
         (char)((fourcc >> 16) & 0xff), (char)((fourcc >> 24) & 0xff));
    return fbId;
}

void HwcZizhan::destroyFbLocked(uint32_t fbId) {
    if (fbId) drmModeRmFB(mState.drmFd, fbId);
}

void HwcZizhan::drainFbCacheLocked() {
    // 简单 LRU：超过 8 个就清空，避免泄漏
    if (mState.fbCache.size() <= 8) return;
    for (auto& kv : mState.fbCache) {
        destroyFbLocked(kv.second);
    }
    mState.fbCache.clear();
    LOGI("drain fb cache");
}

// ----------------------------------------------------------------

bool HwcZizhan::commit() {
    std::lock_guard<std::mutex> _l(mLock);
    if (!mState.inited || mState.drmFd < 0) return false;

    if (mState.layers.empty()) return false;

    drmModeAtomicReqPtr req = drmModeAtomicAlloc();
    if (!req) return false;

    // 1. CRTC 激活 + mode
    if (mState.crtcActiveProp)
        drmModeAtomicAddProperty(req, mState.primaryCrtcId, mState.crtcActiveProp, 1);
    if (mState.crtcModeIdProp && mState.crtcModeId)
        drmModeAtomicAddProperty(req, mState.primaryCrtcId, mState.crtcModeIdProp, mState.crtcModeId);

    // 2. Connector 绑 CRTC
    if (mState.connCrtcIdProp)
        drmModeAtomicAddProperty(req, mState.connectorId, mState.connCrtcIdProp, mState.primaryCrtcId);

    // 3. 每个 layer 配到自己 plane
    int zBase = 0;
    for (auto& kv : mState.layers) {
        LayerDesc& l = kv.second;
        if (l.fbId == 0 || l.planeId == 0) continue;

        PlaneProps* pp = nullptr;
        for (auto& p : mState.planes) {
            if (p.planeId == l.planeId) { pp = &p; break; }
        }
        if (!pp) continue;

        // 屏幕区域
        if (pp->propFbId)   drmModeAtomicAddProperty(req, pp->planeId, pp->propFbId,   l.fbId);
        if (pp->propCrtcId) drmModeAtomicAddProperty(req, pp->planeId, pp->propCrtcId, mState.primaryCrtcId);
        if (pp->propCrtcX)  drmModeAtomicAddProperty(req, pp->planeId, pp->propCrtcX,  l.left);
        if (pp->propCrtcY)  drmModeAtomicAddProperty(req, pp->planeId, pp->propCrtcY,  l.top);
        if (pp->propCrtcW)  drmModeAtomicAddProperty(req, pp->planeId, pp->propCrtcW,  l.width);
        if (pp->propCrtcH)  drmModeAtomicAddProperty(req, pp->planeId, pp->propCrtcH,  l.height);

        // 源区域 = 16.16 定点
        if (pp->propSrcX)   drmModeAtomicAddProperty(req, pp->planeId, pp->propSrcX,   0);
        if (pp->propSrcY)   drmModeAtomicAddProperty(req, pp->planeId, pp->propSrcY,   0);
        if (pp->propSrcW)   drmModeAtomicAddProperty(req, pp->planeId, pp->propSrcW,   (uint64_t)l.width  << 16);
        if (pp->propSrcH)   drmModeAtomicAddProperty(req, pp->planeId, pp->propSrcH,   (uint64_t)l.height << 16);

        if (pp->propAlpha)  drmModeAtomicAddProperty(req, pp->planeId, pp->propAlpha,  0xFFFF);
        if (pp->propBlend)  drmModeAtomicAddProperty(req, pp->planeId, pp->propBlend,  DRM_MODE_BLEND_PREMULTI);
        if (pp->propZpos)   drmModeAtomicAddProperty(req, pp->planeId, pp->propZpos,   zBase++);

        if (pp->propInFence && l.lastInFd >= 0) {
            drmModeAtomicAddProperty(req, pp->planeId, pp->propInFence, l.lastInFd);
        }
    }

    const uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
    int ret = drmModeAtomicCommit(mState.drmFd, req, flags, nullptr);

    // in_fence_fd 提交后由 DRM 驱动 owns，我们这边置空防止误 close
    for (auto& kv : mState.layers) {
        kv.second.lastInFd = -1;
    }

    drmModeAtomicFree(req);

    if (ret) {
        LOGW("atomic commit failed: %s", strerror(errno));
        return false;
    }
    return true;
}

void HwcZizhan::destroy() {
    std::lock_guard<std::mutex> _l(mLock);
    if (mState.drmFd >= 0) {
        for (auto& kv : mState.fbCache) destroyFbLocked(kv.second);
        mState.fbCache.clear();
        mState.layers.clear();
        mState.keyToLayer.clear();
        if (mState.crtcModeId)
            drmModeDestroyPropertyBlob(mState.drmFd, mState.crtcModeId);
        drmDropMaster(mState.drmFd);
        close(mState.drmFd);
        mState.drmFd = -1;
    }
    mState.inited = false;
}

} // namespace zizhan
} // namespace android
