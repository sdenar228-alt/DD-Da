// The Media Foundation source reader was added in Windows 7, while the build
// targets Vista. Raise it for this file only; the DLLs are delay loaded, so a
// system without Media Foundation (the Windows N editions) still starts and
// simply reports that it cannot decode the file.
#if defined(_WIN32)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

#include "custom_media_win.h"

#include <cstring>

#if defined(CONF_FAMILY_WINDOWS)

#include <base/log.h>
#include <base/windows.h>

// The build defines these globally so that windows.h keeps its hands off the
// names `IStorage` and `ERROR`. The Media Foundation and WIC headers need the
// full set though (mmreg.h wants BITMAPINFOHEADER), and this file deliberately
// pulls in no engine header that would clash.
#undef NOGDI
#undef WIN32_LEAN_AND_MEAN

#include <windows.h>
// clang-format off
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
// clang-format on

#if defined(_MSC_VER)
// For VcppException, the exception the delay load helper raises.
#include <delayimp.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>

namespace {
// Videos are scaled down to at most this size, every frame is a full texture
// upload and a background does not need more than that.
constexpr int MAX_VIDEO_WIDTH = 1280;
constexpr int MAX_VIDEO_HEIGHT = 720;

template<typename T>
void SafeRelease(T *&pInterface)
{
	if(pInterface != nullptr)
	{
		pInterface->Release();
		pInterface = nullptr;
	}
}

#if defined(_MSC_VER)
// A Windows edition without Media Foundation does not make the delay loaded
// calls fail, it makes the loader raise one of these on the first one.
bool IsDelayLoadFailure(DWORD ExceptionCode)
{
	return ExceptionCode == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ||
	       ExceptionCode == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND);
}
#endif
} // namespace

class CWindowsMedia::CImpl
{
public:
	~CImpl() { Close(); }

	bool OpenImage(const char *pAbsolutePath);
	bool OpenVideo(const char *pAbsolutePath);
	void Close();
	bool NextFrame(double Time, uint8_t *pRgba, size_t Size);

	bool m_IsOpen = false;
	bool m_IsStill = false;
	int m_Width = 0;
	int m_Height = 0;
	// Loop back to the start after this many seconds, 0 for the whole file.
	double m_MaxDuration = 0.0;

private:
	// A decoded picture is kept around, it never changes.
	std::vector<uint8_t> m_vStillRgba;

	IMFSourceReader *m_pReader = nullptr;
	bool m_MfStarted = false;
	// Media Foundation hands out bottom-up RGB32 unless the stride says so.
	bool m_Flipped = false;
	int m_Stride = 0;
	double m_Duration = 0.0;
	// How long one frame is shown, taken from the frame rate of the file.
	double m_FrameInterval = 1.0 / 30.0;
	// Wall clock time the next frame is due at. The video is played forward
	// instead of seeking to a position derived from the clock, which would make
	// the decoder jump around and decode far more frames than needed.
	double m_NextFrameTime = -1.0;

	bool OpenVideoImpl(const char *pAbsolutePath);
	void SeekToStart();
};

bool CWindowsMedia::CImpl::OpenImage(const char *pAbsolutePath)
{
	Close();

	// WIC needs COM, but only the apartment this thread already has.
	const HRESULT ComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool ComInitialized = SUCCEEDED(ComResult);

	IWICImagingFactory *pFactory = nullptr;
	IWICBitmapDecoder *pDecoder = nullptr;
	IWICBitmapFrameDecode *pFrame = nullptr;
	IWICFormatConverter *pConverter = nullptr;
	bool Success = false;

	do
	{
		if(FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
			break;

		const std::wstring WidePath = windows_utf8_to_wide(pAbsolutePath);
		if(FAILED(pFactory->CreateDecoderFromFilename(WidePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder)))
			break;
		if(FAILED(pDecoder->GetFrame(0, &pFrame)))
			break;

		UINT Width = 0, Height = 0;
		if(FAILED(pFrame->GetSize(&Width, &Height)) || Width == 0 || Height == 0)
			break;

		// Convert whatever the file uses into straight RGBA.
		if(FAILED(pFactory->CreateFormatConverter(&pConverter)))
			break;
		if(FAILED(pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
			break;

		const UINT Stride = Width * 4;
		m_vStillRgba.resize((size_t)Stride * Height);
		if(FAILED(pConverter->CopyPixels(nullptr, Stride, (UINT)m_vStillRgba.size(), m_vStillRgba.data())))
		{
			m_vStillRgba.clear();
			break;
		}

		m_Width = (int)Width;
		m_Height = (int)Height;
		m_IsStill = true;
		m_IsOpen = true;
		Success = true;
	} while(false);

	SafeRelease(pConverter);
	SafeRelease(pFrame);
	SafeRelease(pDecoder);
	SafeRelease(pFactory);
	if(ComInitialized)
		CoUninitialize();

	if(!Success)
		log_error("custombackground", "Windows cannot decode the picture '%s'", pAbsolutePath);
	return Success;
}

bool CWindowsMedia::CImpl::OpenVideoImpl(const char *pAbsolutePath)
{
	Close();

	if(FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
		return false;
	m_MfStarted = true;

	IMFAttributes *pAttributes = nullptr;
	if(FAILED(MFCreateAttributes(&pAttributes, 2)))
	{
		Close();
		return false;
	}
	// Lets the reader insert a converter so it can hand out plain RGB32.
	pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	pAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);

	const std::wstring WidePath = windows_utf8_to_wide(pAbsolutePath);
	const HRESULT CreateResult = MFCreateSourceReaderFromURL(WidePath.c_str(), pAttributes, &m_pReader);
	SafeRelease(pAttributes);
	if(FAILED(CreateResult) || m_pReader == nullptr)
	{
		log_error("custombackground", "Windows has no codec for '%s'", pAbsolutePath);
		Close();
		return false;
	}

	m_pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if(FAILED(m_pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE)))
	{
		log_error("custombackground", "'%s' has no video track", pAbsolutePath);
		Close();
		return false;
	}

	IMFMediaType *pWanted = nullptr;
	if(FAILED(MFCreateMediaType(&pWanted)))
	{
		Close();
		return false;
	}
	pWanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pWanted->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

	// Ask for a smaller frame when the file is larger than a background needs.
	// Every frame is copied across the process and uploaded to the graphics card,
	// so a 4K file costs nine times what a 720p one does for a picture that ends
	// up on the same screen. The reader has its video processor enabled, so it
	// does the scaling itself while decoding.
	UINT32 NativeWidth = 0, NativeHeight = 0;
	IMFMediaType *pNative = nullptr;
	if(SUCCEEDED(m_pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pNative)) && pNative != nullptr)
	{
		MFGetAttributeSize(pNative, MF_MT_FRAME_SIZE, &NativeWidth, &NativeHeight);
		SafeRelease(pNative);
	}
	if(NativeWidth > 0 && NativeHeight > 0 &&
		((int)NativeWidth > MAX_VIDEO_WIDTH || (int)NativeHeight > MAX_VIDEO_HEIGHT))
	{
		const double Scale = std::min((double)MAX_VIDEO_WIDTH / NativeWidth, (double)MAX_VIDEO_HEIGHT / NativeHeight);
		// Even numbers, because that is what the video processor wants.
		const UINT32 WantedWidth = std::max<UINT32>(2, ((UINT32)(NativeWidth * Scale) / 2) * 2);
		const UINT32 WantedHeight = std::max<UINT32>(2, ((UINT32)(NativeHeight * Scale) / 2) * 2);
		MFSetAttributeSize(pWanted, MF_MT_FRAME_SIZE, WantedWidth, WantedHeight);
	}

	HRESULT TypeResult = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pWanted);
	if(FAILED(TypeResult))
	{
		// Not every source can be scaled on the way out; take it at its own size.
		IMFMediaType *pPlain = nullptr;
		if(SUCCEEDED(MFCreateMediaType(&pPlain)) && pPlain != nullptr)
		{
			pPlain->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			pPlain->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			TypeResult = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pPlain);
			SafeRelease(pPlain);
		}
	}
	SafeRelease(pWanted);
	if(FAILED(TypeResult))
	{
		log_error("custombackground", "Could not decode '%s' to RGB", pAbsolutePath);
		Close();
		return false;
	}

	IMFMediaType *pActual = nullptr;
	if(FAILED(m_pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActual)) || pActual == nullptr)
	{
		Close();
		return false;
	}
	UINT32 Width = 0, Height = 0;
	MFGetAttributeSize(pActual, MF_MT_FRAME_SIZE, &Width, &Height);
	UINT32 FpsNumerator = 0, FpsDenominator = 0;
	if(SUCCEEDED(MFGetAttributeRatio(pActual, MF_MT_FRAME_RATE, &FpsNumerator, &FpsDenominator)) && FpsNumerator > 0 && FpsDenominator > 0)
		m_FrameInterval = (double)FpsDenominator / (double)FpsNumerator;
	INT32 Stride = 0;
	if(FAILED(pActual->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32 *)&Stride)) || Stride == 0)
	{
		// Unset means the default layout, which is bottom-up for RGB32.
		Stride = -(INT32)(Width * 4);
	}
	SafeRelease(pActual);

	if(Width == 0 || Height == 0)
	{
		Close();
		return false;
	}

	m_Flipped = Stride < 0;
	m_Stride = Stride < 0 ? -Stride : Stride;
	m_Width = (int)Width;
	m_Height = (int)Height;

	// Duration comes back in 100 nanosecond units.
	PROPVARIANT DurationVar;
	PropVariantInit(&DurationVar);
	m_Duration = 0.0;
	if(SUCCEEDED(m_pReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &DurationVar)) && DurationVar.vt == VT_UI8)
		m_Duration = (double)DurationVar.uhVal.QuadPart / 10000000.0;
	PropVariantClear(&DurationVar);

	m_IsStill = m_Duration <= 0.0;
	m_NextFrameTime = -1.0;
	m_IsOpen = true;

	if(m_Width > MAX_VIDEO_WIDTH || m_Height > MAX_VIDEO_HEIGHT)
	{
		log_info("custombackground", "Video stayed at %dx%d, this source cannot be scaled while decoding; a smaller file is cheaper", m_Width, m_Height);
	}
	return true;
}

#if defined(_MSC_VER)
// The work is in OpenVideoImpl because a frame with an __except must not own
// anything that needs unwinding, and the delay load exception is raised before
// any Media Foundation call gets the chance to return a failed result.
bool CWindowsMedia::CImpl::OpenVideo(const char *pAbsolutePath)
{
	__try
	{
		return OpenVideoImpl(pAbsolutePath);
	}
	__except(IsDelayLoadFailure(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		log_error("custombackground", "Media Foundation is missing on this Windows edition, cannot play '%s'", pAbsolutePath);
		Close();
		return false;
	}
}
#else
bool CWindowsMedia::CImpl::OpenVideo(const char *pAbsolutePath)
{
	return OpenVideoImpl(pAbsolutePath);
}
#endif

void CWindowsMedia::CImpl::Close()
{
	SafeRelease(m_pReader);
	if(m_MfStarted)
	{
		MFShutdown();
		m_MfStarted = false;
	}
	m_vStillRgba.clear();
	m_vStillRgba.shrink_to_fit();
	m_IsOpen = false;
	m_IsStill = false;
	m_Width = 0;
	m_Height = 0;
	m_Flipped = false;
	m_Stride = 0;
	m_Duration = 0.0;
	m_FrameInterval = 1.0 / 30.0;
	m_NextFrameTime = -1.0;
	// m_MaxDuration is a setting, not file state, so it survives Close().
}

void CWindowsMedia::CImpl::SeekToStart()
{
	PROPVARIANT Position;
	PropVariantInit(&Position);
	Position.vt = VT_I8;
	Position.hVal.QuadPart = 0;
	m_pReader->SetCurrentPosition(GUID_NULL, Position);
	PropVariantClear(&Position);
}

bool CWindowsMedia::CImpl::NextFrame(double Time, uint8_t *pRgba, size_t Size)
{
	if(!m_IsOpen)
		return false;

	// A picture is decoded once and handed out as is.
	if(m_pReader == nullptr)
	{
		if(m_vStillRgba.empty() || pRgba == nullptr || Size < m_vStillRgba.size())
			return false;
		std::memcpy(pRgba, m_vStillRgba.data(), m_vStillRgba.size());
		return true;
	}

	if(m_NextFrameTime >= 0.0)
	{
		if(m_IsStill)
			return false;
		if(Time < m_NextFrameTime)
			return false;
		// After a hitch (a long map load for example) do not try to catch up on
		// every missed frame, just carry on from here.
		if(Time > m_NextFrameTime + 1.0)
			m_NextFrameTime = Time;
		m_NextFrameTime += m_FrameInterval;
	}
	else
	{
		m_NextFrameTime = Time + m_FrameInterval;
	}

	IMFSample *pSample = nullptr;
	// One frame per call, the file is played forward. Two attempts are enough:
	// the second one is for the frame right after wrapping around at the end.
	for(int i = 0; i < 2; ++i)
	{
		DWORD StreamFlags = 0;
		LONGLONG Timestamp = 0;
		SafeRelease(pSample);
		if(FAILED(m_pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &StreamFlags, &Timestamp, &pSample)))
			return false;

		const bool Eof = (StreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;
		// Timestamps are in 100 nanosecond units. A file longer than the limit is
		// simply cut there and starts over.
		const bool PastLimit = pSample != nullptr && m_MaxDuration > 0.0 &&
				       (double)Timestamp / 10000000.0 >= m_MaxDuration;
		if(Eof || PastLimit)
		{
			SafeRelease(pSample);
			SeekToStart();
			continue;
		}
		if(pSample != nullptr)
			break;
	}
	if(pSample == nullptr)
		return false;

	IMFMediaBuffer *pBuffer = nullptr;
	if(FAILED(pSample->ConvertToContiguousBuffer(&pBuffer)) || pBuffer == nullptr)
	{
		SafeRelease(pSample);
		return false;
	}

	BYTE *pData = nullptr;
	DWORD CurrentLength = 0;
	if(FAILED(pBuffer->Lock(&pData, nullptr, &CurrentLength)))
	{
		SafeRelease(pBuffer);
		SafeRelease(pSample);
		return false;
	}

	const size_t RowSize = (size_t)m_Width * 4;
	const size_t Needed = RowSize * m_Height;
	if(pRgba == nullptr || Size < Needed)
	{
		pBuffer->Unlock();
		SafeRelease(pBuffer);
		SafeRelease(pSample);
		return false;
	}
	for(int y = 0; y < m_Height; ++y)
	{
		const int SourceRow = m_Flipped ? m_Height - 1 - y : y;
		if((size_t)(SourceRow + 1) * m_Stride > CurrentLength)
			continue;
		const BYTE *pSrc = pData + (size_t)SourceRow * m_Stride;
		uint8_t *pDst = pRgba + (size_t)y * RowSize;
		// Media Foundation RGB32 is stored as BGRX. A whole pixel at a time
		// rather than four bytes: this loop runs over every pixel of every frame,
		// and byte-wise it was one of the more expensive things in the client.
		for(int x = 0; x < m_Width; ++x)
		{
			uint32_t Pixel;
			std::memcpy(&Pixel, pSrc + (size_t)x * 4, sizeof(Pixel));
			Pixel = 0xFF000000u | ((Pixel & 0x00FF0000u) >> 16) | (Pixel & 0x0000FF00u) | ((Pixel & 0x000000FFu) << 16);
			std::memcpy(pDst + (size_t)x * 4, &Pixel, sizeof(Pixel));
		}
	}

	pBuffer->Unlock();
	SafeRelease(pBuffer);
	SafeRelease(pSample);

	return true;
}

CWindowsMedia::CWindowsMedia() :
	m_pImpl(std::make_unique<CImpl>())
{
}

CWindowsMedia::~CWindowsMedia() = default;

bool CWindowsMedia::OpenImage(const char *pAbsolutePath) { return m_pImpl->OpenImage(pAbsolutePath); }
bool CWindowsMedia::OpenVideo(const char *pAbsolutePath) { return m_pImpl->OpenVideo(pAbsolutePath); }
void CWindowsMedia::Close() { m_pImpl->Close(); }
bool CWindowsMedia::IsOpen() const { return m_pImpl->m_IsOpen; }
bool CWindowsMedia::IsStill() const { return m_pImpl->m_IsStill; }
int CWindowsMedia::Width() const { return m_pImpl->m_Width; }
int CWindowsMedia::Height() const { return m_pImpl->m_Height; }
void CWindowsMedia::SetMaxDuration(double Seconds) { m_pImpl->m_MaxDuration = Seconds > 0.0 ? Seconds : 0.0; }
bool CWindowsMedia::NextFrame(double Time, uint8_t *pRgba, size_t Size) { return m_pImpl->NextFrame(Time, pRgba, Size); }

#endif
