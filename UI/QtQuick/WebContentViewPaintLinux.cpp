/*
 * Copyright (c) 2026, Penk Chen <penkia@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/SharedImage.h>
#include <UI/QtQuick/Platform.h>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QtQuick/QSGTexture>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace Ladybird {

namespace {

struct PinnedState {
    EGLDisplay display { EGL_NO_DISPLAY };
    EGLImageKHR image { EGL_NO_IMAGE_KHR };
    unsigned int texture { 0 };
};

PFNEGLCREATEIMAGEKHRPROC s_egl_create_image { nullptr };
PFNEGLDESTROYIMAGEKHRPROC s_egl_destroy_image { nullptr };
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_gl_egl_image_target_texture_2d { nullptr };

bool resolve_gl_egl_entry_points()
{
    if (s_egl_create_image && s_egl_destroy_image && s_gl_egl_image_target_texture_2d)
        return true;
    s_egl_create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    s_egl_destroy_image = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    s_gl_egl_image_target_texture_2d = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    return s_egl_create_image && s_egl_destroy_image && s_gl_egl_image_target_texture_2d;
}

}

QSGTexture* acquire_linux_dmabuf_texture(QQuickWindow* window, Gfx::LinuxDmaBufHandle const& handle, QSize size, void** pinned)
{
    auto* rif = window->rendererInterface();
    if (!rif || rif->graphicsApi() != QSGRendererInterface::OpenGL)
        return nullptr;

    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return nullptr;

    if (!resolve_gl_egl_entry_points())
        return nullptr;

    auto display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLint const attribs[] = {
        EGL_WIDTH, handle.size.width(),
        EGL_HEIGHT, handle.size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(handle.drm_format),
        EGL_DMA_BUF_PLANE0_FD_EXT, handle.file.fd(),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(handle.pitch),
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLint>(handle.modifier & 0xFFFFFFFFu),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLint>(handle.modifier >> 32u),
        EGL_NONE
    };

    auto image = s_egl_create_image(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
    if (image == EGL_NO_IMAGE_KHR)
        return nullptr;

    auto* gl = context->functions();
    unsigned int texture = 0;
    gl->glGenTextures(1, &texture);
    gl->glBindTexture(GL_TEXTURE_2D, texture);
    s_gl_egl_image_target_texture_2d(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glBindTexture(GL_TEXTURE_2D, 0);

    // fromNative doesn't take ownership; keep the EGLImage + texture alive via pinned.
    release_native_texture(*pinned);
    *pinned = new PinnedState { display, image, texture };

    return QNativeInterface::QSGOpenGLTexture::fromNative(texture, window, size,
        QQuickWindow::TextureIsOpaque);
}

void release_native_texture(void* pinned)
{
    if (!pinned)
        return;
    auto* state = static_cast<PinnedState*>(pinned);

    // The GL texture can only be deleted while a context is current on this thread; the
    // view destructor path may run without one, in which case the driver reclaims on teardown.
    if (state->texture != 0) {
        if (auto* context = QOpenGLContext::currentContext())
            context->functions()->glDeleteTextures(1, &state->texture);
    }
    if (state->image != EGL_NO_IMAGE_KHR && s_egl_destroy_image)
        s_egl_destroy_image(state->display, state->image);

    delete state;
}

}
