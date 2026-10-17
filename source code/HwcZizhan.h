/*
 * Copyright (C) 2025 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0
 */

#ifndef ANDROID_HWC_ZIZHAN_H
#define ANDROID_HWC_ZIZHAN_H

#include <stdint.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <xf86drm.h>
#include <xf86drmMode.h>

namespace android {
namespace zizhan {

/**
 * 每个窗口 = 一个 Layer。
 * 同一个 BQ（mUniqueId 不变）反复 queueBuffer 时沿用同一个 layerId，
 * 只更新 buffer / fence。
 */
struct LayerDesc {
    int64_t  layerId    = -1;
    uint64_t key        = 0;    // BQ 的唯一 id，用于跨 buffer 复用 layer
    int      left       = 0;
    int      top        = 0;
    int      width      = 0;
    int      height     = 0;
    int      zorder     = 0;

    // 内部状态
    uint32_t planeId    = 0;
    uint32_t fbId       = 0;
    uint64_t fbBufferKey= 0;    // 当前 fbId 对应的 bufferId
    int      lastInFd   = -1;
};

/**
 * libhwcomposerzizhan
 *
 * 极简 DRM/KMS 硬件合成器：多个 layer 一次性 atomic commit 到主显示。
 *
 * 线程安全：所有公开方法均加锁。
 *
 * 使用流程（由 BufferQueueProducer 调用）：
 *   HwcZizhan::get().init();                              // 仅第一次
 *   int64_t id = HwcZizhan::get().addOrUpdateLayer(
 *                  mCore->mUniqueId, left, top, h, w, z);
 *   HwcZizhan::get().updateLayerBuffer(
 *                  id, dmabufFd, bufferId, w, h, format,
 *                  pitch, inFenceFd);
 *   HwcZizhan::get().commit();
 */
class HwcZizhan {
public:
    static HwcZizhan& get();

    /** 幂等初始化：打开 DRM、枚举 CRTC/Plane/Connector，失败返回 false。 */
    bool init();

    /**
     * 添加或更新一个窗口对应的 layer。
     * @param key    窗口唯一 key（用 BufferQueueCore::mUniqueId）
     * @param left/top/width/height  窗口在屏幕上的位置尺寸
     * @param zorder 叠放顺序，越大越靠上
     * @return layerId，<0 表示失败
     */
    int64_t addOrUpdateLayer(uint64_t key,
                             int left, int top, int width, int height, int zorder);

    /**
     * 更新 layer 的当前 buffer，并记录 fence。
     * 不在这里做阻塞等待：fence 通过 DRM property "IN_FENCE_FD" 交给驱动；
     * 若驱动不支持，就在 commit 前由用户态等一次。
     *
     * @param layerId        addOrUpdateLayer 返回的 id
     * @param dmabufFd       该 GraphicBuffer 的第一个 fd（调用方负责 dup）
     * @param bufferId       GraphicBuffer::getId()，用于 fb 复用
     * @param format         HAL_PIXEL_FORMAT_* （见 toDrmFourcc）
     * @param pitchBytes     每行字节数（gb->getStride() * bpp）
     * @param inFenceFd      GPU 写完成的 fence fd，<0 表示无
     */
    bool updateLayerBuffer(int64_t layerId,
                           int dmabufFd,
                           uint64_t bufferId,
                           int width, int height,
                           uint32_t format,
                           uint32_t pitchBytes,
                           int inFenceFd);

    /** 从当前合成中移除 layer（窗口销毁时调用）。 */
    bool removeLayer(int64_t layerId);

    /** 移除该 key 对应的 layer（若无）。 */
    bool removeLayerByKey(uint64_t key);

    /** 把所有 layer 的当前状态一次性 atomic commit。 */
    bool commit();

    /** 断开 DRM 主控并释放资源（进程退出时用）。 */
    void destroy();

private:
    HwcZizhan();
    ~HwcZizhan();
    HwcZizhan(const HwcZizhan&) = delete;
    HwcZizhan& operator=(const HwcZizhan&) = delete;

    // DRM 初始化
    bool openDrmLocked();
    bool findPrimaryCrtcLocked();
    bool findConnectorLocked();
    void enumeratePlanesLocked();
    uint32_t findPropertyLocked(uint32_t objectId, const char* name);

    // fb 管理
    uint32_t getOrCreateFbLocked(int dmabufFd, uint64_t bufferId,
                                 int width, int height,
                                 uint32_t format, uint32_t pitchBytes);
    void destroyFbLocked(uint32_t fbId);
    void drainFbCacheLocked();

    // 平面分配
    uint32_t pickPlaneForLayerLocked(const LayerDesc& l) const;

    // 每个 plane 必须的属性 id
    struct PlaneProps {
        uint32_t planeId = 0;
        uint32_t propFbId = 0;
        uint32_t propCrtcId = 0;
        uint32_t propCrtcX = 0;
        uint32_t propCrtcY = 0;
        uint32_t propCrtcW = 0;
        uint32_t propCrtcH = 0;
        uint32_t propSrcX = 0;
        uint32_t propSrcY = 0;
        uint32_t propSrcW = 0;
        uint32_t propSrcH = 0;
        uint32_t propAlpha = 0;
        uint32_t propBlend = 0;
        uint32_t propZpos = 0;
        uint32_t propInFence = 0;
        uint32_t possibleCrtcs = 0;
        bool     isPrimary = false;
    };

    struct State {
        bool     inited = false;
        int      drmFd = -1;

        uint32_t primaryCrtcId = 0;
        uint32_t connectorId = 0;
        uint32_t crtcModeId = 0;

        // CRTC / Connector 属性
        uint32_t crtcActiveProp = 0;
        uint32_t crtcModeIdProp = 0;
        uint32_t connCrtcIdProp = 0;

        // 所有可用 plane
        std::vector<PlaneProps> planes;

        // layerId -> LayerDesc
        std::unordered_map<int64_t, LayerDesc> layers;
        // key -> layerId（快速反查）
        std::unordered_map<uint64_t, int64_t>   keyToLayer;

        // (bufferId) -> fbId，同一个 buffer 在多帧中复用
        std::unordered_map<uint64_t, uint32_t>  fbCache;

        int64_t nextLayerId = 1;
    };

    std::mutex mLock;
    State mState;
};

} // namespace zizhan
} // namespace android

#endif // ANDROID_HWC_ZIZHAN_H
