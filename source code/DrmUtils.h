// frameworks/native/libs/drm_utils/include/drm_utils/DrmUtils.h

#ifndef DRM_UTILS_H
#define DRM_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// DRM 上下文结构体
typedef struct DrmContext DrmContext;

// 创建 DRM 上下文
DrmContext* drm_context_create();

// 初始化 DRM 设备
int drm_context_init(DrmContext* ctx);

// 使用 DMA-BUF 创建帧缓冲
uint32_t drm_context_create_fb(DrmContext* ctx, int dma_buf_fd, int width, int height);

// 提交帧缓冲到显示
int drm_context_page_flip(DrmContext* ctx, uint32_t fb_id);

// 销毁 DRM 上下文
void drm_context_destroy(DrmContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // DRM_UTILS_H