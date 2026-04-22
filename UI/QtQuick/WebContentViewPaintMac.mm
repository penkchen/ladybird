/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <LibCore/IOSurface.h>
#include <UI/QtQuick/Platform.h>

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QtQuick/QSGTexture>

#import <Metal/Metal.h>

namespace Ladybird {

QSGTexture* acquire_iosurface_texture(QQuickWindow* window, Core::IOSurfaceHandle const& iosurface_handle, QSize size, void** pinned)
{
    auto* rif = window->rendererInterface();
    if (!rif || rif->graphicsApi() != QSGRendererInterface::Metal)
        return nullptr;

    auto* device_resource = rif->getResource(window, QSGRendererInterface::DeviceResource);
    if (!device_resource)
        return nullptr;

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_resource;
    if (!device)
        return nullptr;

    auto iosurface_ref = (IOSurfaceRef)iosurface_handle.core_foundation_pointer();
    if (!iosurface_ref)
        return nullptr;

    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                    width:size.width()
                                                                                   height:size.height()
                                                                                mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> texture = [device newTextureWithDescriptor:desc
                                                    iosurface:iosurface_ref
                                                        plane:0];
    if (!texture)
        return nullptr;

    // QSGMetalTexture::fromNative does not retain the native texture.
    // Transfer a +1 retain out through `pinned` and release any prior
    // retained texture so its lifetime extends past this frame's GPU work.
    if (*pinned)
        CFRelease(*pinned);
    *pinned = const_cast<void*>(CFBridgingRetain(texture));

    return QNativeInterface::QSGMetalTexture::fromNative(texture, window, size,
        QQuickWindow::TextureIsOpaque);
}

void release_native_texture(void* pinned)
{
    if (pinned)
        CFRelease(pinned);
}

}
