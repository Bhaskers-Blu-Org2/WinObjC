//******************************************************************************
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************
#pragma once

#import "Starboard.h"
#import "CoreGraphicsInternal.h"
#import "CGImageInternal.h"

// Ignore some warnings
#if defined __clang__

#pragma clang diagnostic push
#ifdef _M_ARM
// Disable 'invalid calling convention' warnings for __stdcall usage in ARM builds
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif // _M_ARM

#endif // __clang__

#include <COMIncludes.h>
#import <WRLHelpers.h>
#import <ErrorHandling.h>
#import <wrl/client.h>
#import <wrl/implements.h>
#import <Wincodec.h>
#import <D2d1.h>
#import <IDisplayTexture.h>
#include <COMIncludes_End.h>

#import <memory>

// clang-format off
//The following interface is used to obtain the set display texture from a CGIWICBitmap
struct __declspec(uuid("BA77A716-5FAF-42B1-B5B3-9B6369A0625D")) __declspec(novtable) ICGDisplayTexture : public IUnknown
{
	virtual std::shared_ptr<IDisplayTexture> DisplayTexture() = 0;
};
// clang-format on

class CGIWICBitmap;

class CGIWICBitmapLock
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
                                          IAgileObject,
                                          Microsoft::WRL::FtmBase,
                                          IWICBitmapLock> {
public:
    CGIWICBitmapLock(_In_ std::shared_ptr<IDisplayTexture> texture, _In_ const WICRect* region, _In_ WICPixelFormatGUID pixelFormat)
        : m_texture(texture), m_pixelFormat(pixelFormat), m_locked_rect(region) {
        int bpr;
        m_dataBuffer = static_cast<BYTE*>(m_texture->Lock(&bpr));
        m_bytesPerRow = static_cast<size_t>(bpr);
    }

    CGIWICBitmapLock(_In_ BYTE* data, _In_ const WICRect* region, _In_ size_t bytesPerRow, _In_ WICPixelFormatGUID pixelFormat)
        : m_dataBuffer(data), m_pixelFormat(pixelFormat), m_locked_rect(region), m_bytesPerRow(bytesPerRow) {
        m_texture = nullptr;
    }

    ~CGIWICBitmapLock() {
        if (m_texture) {
            m_texture->Unlock();
        }
    }

    HRESULT STDMETHODCALLTYPE GetSize(_Out_ UINT* width, _Out_ UINT* height) {
        RETURN_HR_IF_NULL(E_POINTER, width);
        RETURN_HR_IF_NULL(E_POINTER, height);

        *width = m_locked_rect->Width;
        *height = m_locked_rect->Height;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetStride(_Out_ UINT* stride) {
        RETURN_HR_IF_NULL(E_POINTER, stride);
        *stride = m_bytesPerRow;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataPointer(_Out_ UINT* bufferSize, _Outptr_ WICInProcPointer* data) {
        RETURN_HR_IF_NULL(E_POINTER, bufferSize);
        RETURN_HR_IF_NULL(E_POINTER, data);
        *bufferSize = m_locked_rect->Height * m_bytesPerRow;
        *data = m_dataBuffer;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelFormat(_Out_ WICPixelFormatGUID* pixelFormat) {
        RETURN_HR_IF_NULL(E_POINTER, pixelFormat);
        *pixelFormat = m_pixelFormat;
        return S_OK;
    }

private:
    WICPixelFormatGUID m_pixelFormat;
    BYTE* m_dataBuffer;
    const WICRect* m_locked_rect;
    size_t m_bytesPerRow;
    std::shared_ptr<IDisplayTexture> m_texture;
};

class CGIWICBitmap : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
                                                         IAgileObject,
                                                         Microsoft::WRL::FtmBase,
                                                         IWICBitmap,
                                                         ICGDisplayTexture> {
public:
    CGIWICBitmap(_In_ std::shared_ptr<IDisplayTexture> texture, _In_ WICPixelFormatGUID pixelFormat, _In_ UINT height, _In_ UINT width)
        : m_dataBuffer(nullptr), m_texture(texture) {
        Init(pixelFormat, height, width);
    }

    // if data is null, then a buffer that is the size of bytes per row is allocated.
    CGIWICBitmap(_In_ void* data, _In_ WICPixelFormatGUID pixelFormat, _In_ UINT height, _In_ UINT width) : m_texture(nullptr) {
        Init(pixelFormat, height, width);

        // Obtain bytes per row from pixelFormat
        const __CGImagePixelProperties* properties = _CGGetPixelFormatProperties(m_pixelFormat);
        FAIL_FAST_IF(properties == nullptr);

        // bytesPerRow = ((bitsPerPixel) / 8 byte/pixel) * width
        m_bytesPerRow = (properties->bitsPerPixel >> 3) * m_width;

        if (data) {
            m_dataBuffer = static_cast<BYTE*>(data);
        } else {
            m_dataBuffer = new BYTE[m_height * m_bytesPerRow];
            m_freeData = true;
        }
    }

    ~CGIWICBitmap() {
        if (m_freeData) {
            delete[] m_dataBuffer;
            m_dataBuffer = nullptr;
            m_freeData = false;
        }
    }

    // IWICBitmap interface
    // TODO #1124: Today we do not support locking a region of the WIC bitmap for rendering. We only support locking the complete bitmap.
    // This will suffice CoreText requirement but needs to be revisted for CoreGraphics usage in future.

    HRESULT STDMETHODCALLTYPE Lock(_In_ const WICRect* region, _In_ DWORD flags, _COM_Outptr_ IWICBitmapLock** outLock) {
        // flags is ignored.
        RETURN_HR_IF_NULL(E_POINTER, outLock);

        if (region == nullptr) {
            WICRect rect = { 0, 0, m_width, m_height };
            region = &rect;
        } else {
            // This is for tracking purpose, we do not support sub-regions.
            // TODO #1379 - should support regional locking.
            // This check will be removed.
            if (region->X || region->Y || (region->Height < m_height) || (region->Width < m_width)) {
                return E_NOTIMPL;
            }

            RETURN_HR_IF(E_INVALIDARG, (region->Width > m_width) || (region->Height > m_height));
        }

        Microsoft::WRL::ComPtr<IWICBitmapLock> lock =
            m_texture ? Microsoft::WRL::Make<CGIWICBitmapLock>(m_texture, region, m_pixelFormat) :
                        Microsoft::WRL::Make<CGIWICBitmapLock>(m_dataBuffer, region, m_bytesPerRow, m_pixelFormat);
        *outLock = lock.Detach();
        return S_OK;
    }

    // Only supports full region as per now.
    HRESULT STDMETHODCALLTYPE CopyPixels(_In_opt_ const WICRect* copyRect,
                                         _In_ UINT stride,
                                         _In_ UINT bufferSize,
                                         _Out_writes_all_(cbBufferSize) BYTE* buffer) {
        RETURN_HR_IF_NULL(E_POINTER, buffer);

        Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
        RETURN_IF_FAILED(Lock(copyRect, 0, &lock));

        UINT sourceDataSize;
        BYTE* sourceData;
        RETURN_IF_FAILED(lock->GetDataPointer(&sourceDataSize, &sourceData));

        RETURN_HR_IF(E_INVALIDARG, sourceDataSize > bufferSize);

        // TODO #1379 - should support regional copying.
        RETURN_HR_IF(E_UNEXPECTED, memcpy_s(buffer, bufferSize, sourceData, sourceDataSize) != 0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetPalette(_In_ IWICPalette* palette) {
        UNIMPLEMENTED();
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetResolution(_In_ double dpiX, _In_ double dpiY) {
        m_dpiY = dpiY;
        m_dpiX = dpiX;
        return S_OK;
    }

    // IWICBitmapSource interface (that IWICBitmap inherits)
    HRESULT STDMETHODCALLTYPE GetSize(_Out_ UINT* width, _Out_ UINT* height) {
        RETURN_HR_IF_NULL(E_POINTER, width);
        RETURN_HR_IF_NULL(E_POINTER, height);

        *width = m_width;
        *height = m_height;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelFormat(_Out_ WICPixelFormatGUID* pixelFormat) {
        RETURN_HR_IF_NULL(E_POINTER, pixelFormat);
        *pixelFormat = m_pixelFormat;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetResolution(_Out_ double* dpiX, _Out_ double* dpiY) {
        RETURN_HR_IF_NULL(E_POINTER, dpiX);
        RETURN_HR_IF_NULL(E_POINTER, dpiY);
        *dpiY = m_dpiY;
        *dpiX = m_dpiX;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CopyPalette(_In_ IWICPalette* palette) {
        UNIMPLEMENTED();
        return E_NOTIMPL;
    }

    std::shared_ptr<IDisplayTexture> DisplayTexture() {
        return m_texture;
    }

private:
    void Init(_In_ WICPixelFormatGUID pixelFormat, _In_ UINT height, _In_ UINT width) {
        m_dpiX = 96;
        m_dpiY = 96;
        m_pixelFormat = pixelFormat;
        m_height = height;
        m_width = width;
        m_freeData = false;
    }

    WICPixelFormatGUID m_pixelFormat;
    std::shared_ptr<IDisplayTexture> m_texture;
    UINT m_height;
    UINT m_width;
    BYTE* m_dataBuffer;
    size_t m_bytesPerRow;
    bool m_freeData;
    double m_dpiX;
    double m_dpiY;
};

#if defined __clang__
#pragma clang diagnostic pop
#endif
