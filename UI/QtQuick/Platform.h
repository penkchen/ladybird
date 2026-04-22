/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

#include <QSize>

class QQuickWindow;
class QSGTexture;

namespace Core {
class IOSurfaceHandle;
}

namespace Gfx {
struct LinuxDmaBufHandle;
}

namespace Ladybird {

#ifdef AK_OS_MACOS
QSGTexture* acquire_iosurface_texture(QQuickWindow* window, Core::IOSurfaceHandle const& iosurface_handle, QSize size, void** pinned);
#endif

#ifdef AK_OS_LINUX
// Returns nullptr if the RHI isn't OpenGL or the required EGL/GL extensions aren't present;
// callers should fall back to the CPU upload path.
QSGTexture* acquire_linux_dmabuf_texture(QQuickWindow* window, Gfx::LinuxDmaBufHandle const& handle, QSize size, void** pinned);
#endif

#if defined(AK_OS_MACOS) || defined(AK_OS_LINUX)
void release_native_texture(void* pinned);
#endif

}
