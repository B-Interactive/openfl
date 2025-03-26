#define VIDEOGL_EXPORTS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <string>
#include <stdio.h>
#include <GL/gl.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfuuid")

#ifndef GL_CLAMP_TO_EDGE
	#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_RG
	#define GL_RG 0x8227
#endif

#ifndef GL_TEXTURE_SWIZZLE_R
	#define GL_TEXTURE_SWIZZLE_R 0x8E42
	#define GL_TEXTURE_SWIZZLE_G 0x8E43
	#define GL_TEXTURE_SWIZZLE_B 0x8E44
	#define GL_TEXTURE_SWIZZLE_A 0x8E45
	#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#endif

IMFSourceReader* reader = nullptr;
unsigned char* pixelBuffer = nullptr;
int frameWidth = 0;
int frameHeight = 0;

GLuint yTextureID = 0;
GLuint uvTextureID = 0;

extern "C" unsigned int video_gl_get_texture_id_y()
{
	return static_cast<unsigned int>(yTextureID);
}

extern "C" unsigned int video_gl_get_texture_id_uv()
{
	return static_cast<unsigned int>(uvTextureID);
}

std::wstring widen(const char* utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wstr[0], len);
	return wstr;
}

extern "C" int video_get_width(const char* path)
{
    IMFSourceReader* probeReader = nullptr;
    auto widePath = widen(path);
    HRESULT hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &probeReader);
    if (FAILED(hr)) return -1;

    IMFMediaType* actualType = nullptr;
    hr = probeReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualType);
    if (FAILED(hr)) {
        probeReader->Release();
        return -1;
    }

    UINT32 w = 0, h = 0;
    hr = MFGetAttributeSize(actualType, MF_MT_FRAME_SIZE, &w, &h);
    actualType->Release();
    probeReader->Release();

    return SUCCEEDED(hr) ? static_cast<int>(w) : -1;
}

extern "C" int video_get_height(const char* path)
{
    IMFSourceReader* probeReader = nullptr;
    auto widePath = widen(path);
    HRESULT hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &probeReader);
    if (FAILED(hr)) return -1;

    IMFMediaType* actualType = nullptr;
    hr = probeReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualType);
    if (FAILED(hr)) {
        probeReader->Release();
        return -1;
    }

    UINT32 w = 0, h = 0;
    hr = MFGetAttributeSize(actualType, MF_MT_FRAME_SIZE, &w, &h);
    actualType->Release();
    probeReader->Release();

    return SUCCEEDED(hr) ? static_cast<int>(h) : -1;
}

extern "C" bool video_init()
{
	return SUCCEEDED(MFStartup(MF_VERSION));
}

extern "C" bool video_gl_load(const char* path)
{
	auto widePath = widen(path);
	HRESULT hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &reader);
	if (FAILED(hr)) return false;

	IMFMediaType* type = nullptr;
	hr = MFCreateMediaType(&type);
	if (FAILED(hr)) return false;

	type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
	hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type);
	type->Release();
	if (FAILED(hr)) return false;

	IMFMediaType* actualType = nullptr;
	hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actualType);
	if (FAILED(hr)) return false;

	UINT32 w = 0, h = 0;
	hr = MFGetAttributeSize(actualType, MF_MT_FRAME_SIZE, &w, &h);
	actualType->Release();
	if (FAILED(hr)) return false;

	frameWidth = static_cast<int>(w);
	frameHeight = static_cast<int>(h);

	// Setup textures
	if (yTextureID == 0)
	{
		glGenTextures(1, &yTextureID);
		glBindTexture(GL_TEXTURE_2D, yTextureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frameWidth, frameHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	}

	if (uvTextureID == 0)
	{
		glGenTextures(1, &uvTextureID);
		glBindTexture(GL_TEXTURE_2D, uvTextureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, frameWidth / 2, frameHeight / 2, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
	}
	
	return true;
}

extern "C" bool video_software_load(const char* path, unsigned char* externalBuffer, int bufferSize)
{
	auto widePath = widen(path);
	HRESULT hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &reader);
	//printf("MFCreateSourceReaderFromURL HRESULT: 0x%x\n", hr);
	if (FAILED(hr)) return false;

	IMFMediaType* type = nullptr;
	hr = MFCreateMediaType(&type);
	if (FAILED(hr)) { reader->Release(); reader = nullptr; return false; }

	type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); // <-- Use NV12 here

	hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type);
	type->Release();
	//printf("SetCurrentMediaType (NV12) HRESULT: 0x%x\n", hr);
	if (FAILED(hr)) { reader->Release(); reader = nullptr; return false; }

	IMFMediaType* actualType = nullptr;
	hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actualType);
	if (FAILED(hr)) { reader->Release(); reader = nullptr; return false; }

	UINT32 w = 0, h = 0;
	hr = MFGetAttributeSize(actualType, MF_MT_FRAME_SIZE, &w, &h);
	actualType->Release();
	if (FAILED(hr) || w == 0 || h == 0) { reader->Release(); reader = nullptr; return false; }

	frameWidth = static_cast<int>(w);
	frameHeight = static_cast<int>(h);

	// NV12 uses 1.5 bytes per pixel (Y plane + UV plane half-sized)
	int requiredSize = frameWidth * frameHeight * 1.5;
	if (bufferSize < requiredSize) { reader->Release(); reader = nullptr; return false; }

	pixelBuffer = externalBuffer;

	//if (width) *width = frameWidth;
	//if (height) *height = frameHeight;

	return true;
}

extern "C" bool video_software_update_frame()
{
	if (!reader || !pixelBuffer) return false;

	IMFSample* sample = nullptr;
	DWORD flags = 0;
	HRESULT hr = reader->ReadSample(
					 MF_SOURCE_READER_FIRST_VIDEO_STREAM,
					 0, nullptr, &flags, nullptr, &sample
				 );

	//printf("ReadSample HRESULT: 0x%x, flags: 0x%x\n", hr, flags);
	if (FAILED(hr)) return false;
	if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		if (sample) sample->Release();
		return false;
	}

	if (!sample)
	{
		//printf("No sample returned.\n");
		return false;
	}

	IMFMediaBuffer* buffer = nullptr;
	hr = sample->ConvertToContiguousBuffer(&buffer);
	sample->Release();

	if (FAILED(hr) || !buffer)
	{
		//printf("ConvertToContiguousBuffer failed: 0x%x\n", hr);
		return false;
	}

	BYTE* data = nullptr;
	DWORD length = 0;
	hr = buffer->Lock(&data, nullptr, &length);
	//printf("Buffer Lock HRESULT: 0x%x, length: %u\n", hr, length);

	//BYTE* yPlane = data;
	//BYTE* uvPlane = data + (frameWidth * frameHeight);

	int requiredSize = frameWidth * frameHeight * 1.5; // NV12 size

	if (SUCCEEDED(hr) && length >= requiredSize)
	{
		memcpy(pixelBuffer, data, requiredSize);

		//printf("Successfully copied %d bytes to pixelBuffer.\n", requiredSize);
	}
	else
	{
		//printf("Buffer size mismatch. Required: %d, Actual: %u\n", requiredSize, length);
		buffer->Unlock();
		buffer->Release();
		return false;
	}

	buffer->Unlock();
	buffer->Release();

	return true;
}

extern "C" bool video_gl_update_frame()
{
	if (!reader) return false;

	IMFSample* sample = nullptr;
	DWORD flags = 0;
	HRESULT hr = reader->ReadSample(
		MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		0, nullptr, &flags, nullptr, &sample
	);

	if (FAILED(hr)) return false;
	if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		if (sample) sample->Release();
		return false;
	}

	if (!sample) return false;

	IMFMediaBuffer* buffer = nullptr;
	hr = sample->ConvertToContiguousBuffer(&buffer);
	sample->Release();
	if (FAILED(hr) || !buffer) return false;

	IMF2DBuffer* buffer2D = nullptr;
	hr = buffer->QueryInterface(IID_PPV_ARGS(&buffer2D));

	if (SUCCEEDED(hr) && buffer2D != nullptr)
	{
		BYTE* scanline0 = nullptr;
		LONG stride = 0;
		hr = buffer2D->Lock2D(&scanline0, &stride);

		if (SUCCEEDED(hr))
		{
			// === Y plane ===
			static std::vector<BYTE> tightY;
			tightY.resize(frameWidth * frameHeight);

			for (int row = 0; row < frameHeight; row++)
			{
				BYTE* src = scanline0 + row * stride;
				BYTE* dst = &tightY[row * frameWidth];
				memcpy(dst, src, frameWidth);
			}

			// === UV plane ===
			int uvWidth = frameWidth / 2;
			int uvHeight = frameHeight / 2;
			BYTE* uvPlane = scanline0 + (stride * frameHeight); // <- Fix here

			static std::vector<BYTE> tightUV;
			tightUV.resize(uvWidth * uvHeight * 2);

			for (int row = 0; row < uvHeight; row++)
			{
				BYTE* src = uvPlane + row * stride;
				BYTE* dst = &tightUV[row * uvWidth * 2];
				memcpy(dst, src, uvWidth * 2);
			}

			// === Debug ===
			//printf("=== NV12 Diagnostic ===\n");
			//printf("Frame Size: %dx%d\n", frameWidth, frameHeight);
			//printf("Stride: %d\n", stride);
			//printf("First 16 Y bytes:\n");
			//for (int i = 0; i < 16; i++) printf("%02X ", tightY[i]);
			//printf("\nUV plane offset = %d bytes into buffer\n", (int)(uvPlane - scanline0));
			//printf("First 16 UV pairs (U,V):\n");
			//for (int i = 0; i < 16 * 2; i += 2)
			//{
				//printf("(%02X, %02X) ", tightUV[i], tightUV[i + 1]);
			//}
			//printf("\n");

			// === Upload ===
			if (yTextureID != 0)
			{
				glBindTexture(GL_TEXTURE_2D, yTextureID);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RED, GL_UNSIGNED_BYTE, tightY.data());
			}

			if (uvTextureID != 0)
			{
				glBindTexture(GL_TEXTURE_2D, uvTextureID);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uvWidth, uvHeight, GL_RG, GL_UNSIGNED_BYTE, tightUV.data());
			}

			buffer2D->Unlock2D();
			buffer2D->Release();
			buffer->Release();
			return true;
		}

		buffer2D->Release();
	}

	buffer->Release();
	return false;
}


extern "C" unsigned char* video_get_frame_pixels(int* width, int* height)
{
	if (width) *width = frameWidth;
	if (height) *height = frameHeight;
	return pixelBuffer;
}

extern "C" void video_shutdown()
{
	pixelBuffer = nullptr;

	if (reader)
	{
		reader->Release();
		reader = nullptr;
	}

	MFShutdown();
}
