// C++/WinRT needs a modern API level; the build targets Vista globally.
#if defined(_WIN32)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif

#include "custom_music_win.h"

#if defined(CONF_FAMILY_WINDOWS)

#include <base/log.h>

// Same reason as in custom_media_win.cpp: the engine defines these globally so
// windows.h leaves `IStorage` and `ERROR` alone, but the WinRT and WIC headers
// need the full set. Nothing from the engine is included here.
#undef NOGDI
#undef WIN32_LEAN_AND_MEAN

#include <windows.h>
// clang-format off
#include <shlwapi.h>
#include <wincodec.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
// clang-format on

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;

namespace {
// The artwork is only used as a small square, no need to keep it large.
constexpr uint32_t ARTWORK_SIZE = 128;

std::string ToUtf8(const winrt::hstring &Str)
{
	if(Str.empty())
		return std::string();
	const int Size = WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), (int)Str.size(), nullptr, 0, nullptr, nullptr);
	if(Size <= 0)
		return std::string();
	std::string Result((size_t)Size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), (int)Str.size(), Result.data(), Size, nullptr, nullptr);
	return Result;
}
} // namespace

class CWindowsMusic::CImpl
{
public:
	~CImpl() { Stop(); }

	void Start();
	void Stop();

	mutable std::mutex m_Lock;
	CTrack m_Track;
	std::vector<uint8_t> m_vArtwork;
	int m_ArtworkWidth = 0;
	int m_ArtworkHeight = 0;
	bool m_ArtworkFresh = false;

private:
	std::thread m_Thread;
	std::atomic_bool m_Running{false};

	void Run();
	// Decodes the thumbnail into a small RGBA square.
	bool ReadArtwork(const IRandomAccessStreamReference &Reference);
};

void CWindowsMusic::CImpl::Start()
{
	if(m_Running.exchange(true))
		return;
	m_Thread = std::thread([this] { Run(); });
}

void CWindowsMusic::CImpl::Stop()
{
	if(!m_Running.exchange(false))
		return;
	if(m_Thread.joinable())
		m_Thread.join();
}

bool CWindowsMusic::CImpl::ReadArtwork(const IRandomAccessStreamReference &Reference)
{
	if(Reference == nullptr)
		return false;

	IRandomAccessStreamWithContentType Stream = nullptr;
	try
	{
		Stream = Reference.OpenReadAsync().get();
	}
	catch(...)
	{
		return false;
	}
	if(Stream == nullptr || Stream.Size() == 0 || Stream.Size() > 8 * 1024 * 1024)
		return false;

	const uint32_t Size = (uint32_t)Stream.Size();
	Buffer ReadBuffer(Size);
	try
	{
		Stream.ReadAsync(ReadBuffer, Size, InputStreamOptions::None).get();
	}
	catch(...)
	{
		return false;
	}

	const uint8_t *pBytes = ReadBuffer.data();
	if(pBytes == nullptr || ReadBuffer.Length() == 0)
		return false;

	// WIC decodes whatever the app handed over (usually jpeg or png).
	IStream *pMemStream = SHCreateMemStream(pBytes, ReadBuffer.Length());
	if(pMemStream == nullptr)
		return false;

	IWICImagingFactory *pFactory = nullptr;
	IWICBitmapDecoder *pDecoder = nullptr;
	IWICBitmapFrameDecode *pFrame = nullptr;
	IWICBitmapScaler *pScaler = nullptr;
	IWICFormatConverter *pConverter = nullptr;
	bool Success = false;

	do
	{
		if(FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
			break;
		if(FAILED(pFactory->CreateDecoderFromStream(pMemStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder)))
			break;
		if(FAILED(pDecoder->GetFrame(0, &pFrame)))
			break;
		if(FAILED(pFactory->CreateBitmapScaler(&pScaler)))
			break;
		if(FAILED(pScaler->Initialize(pFrame, ARTWORK_SIZE, ARTWORK_SIZE, WICBitmapInterpolationModeFant)))
			break;
		if(FAILED(pFactory->CreateFormatConverter(&pConverter)))
			break;
		if(FAILED(pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
			break;

		std::vector<uint8_t> vRgba((size_t)ARTWORK_SIZE * ARTWORK_SIZE * 4);
		if(FAILED(pConverter->CopyPixels(nullptr, ARTWORK_SIZE * 4, (UINT)vRgba.size(), vRgba.data())))
			break;

		{
			const std::lock_guard<std::mutex> Guard(m_Lock);
			m_vArtwork = std::move(vRgba);
			m_ArtworkWidth = (int)ARTWORK_SIZE;
			m_ArtworkHeight = (int)ARTWORK_SIZE;
			m_ArtworkFresh = true;
		}
		Success = true;
	} while(false);

	if(pConverter != nullptr)
		pConverter->Release();
	if(pScaler != nullptr)
		pScaler->Release();
	if(pFrame != nullptr)
		pFrame->Release();
	if(pDecoder != nullptr)
		pDecoder->Release();
	if(pFactory != nullptr)
		pFactory->Release();
	pMemStream->Release();
	return Success;
}

void CWindowsMusic::CImpl::Run()
{
	// The worker owns its own apartment, the render thread is left alone.
	const HRESULT ComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool ComInitialized = SUCCEEDED(ComResult);

	GlobalSystemMediaTransportControlsSessionManager Manager = nullptr;
	try
	{
		Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
	}
	catch(...)
	{
		log_info("musicisland", "The Windows media session is unavailable, the island stays hidden");
	}

	uint64_t Revision = 0;
	std::string LastKey;

	while(m_Running.load())
	{
		CTrack Track;
		if(Manager != nullptr)
		{
			try
			{
				GlobalSystemMediaTransportControlsSession Session = Manager.GetCurrentSession();
				if(Session != nullptr)
				{
					const auto Properties = Session.TryGetMediaPropertiesAsync().get();
					if(Properties != nullptr)
					{
						Track.m_Title = ToUtf8(Properties.Title());
						Track.m_Artist = ToUtf8(Properties.Artist());
					}
					const auto Info = Session.GetPlaybackInfo();
					if(Info != nullptr)
					{
						Track.m_Playing = Info.PlaybackStatus() ==
								  GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
					}

					const std::string Key = Track.m_Title + "\x1f" + Track.m_Artist;
					if(Key != LastKey)
					{
						LastKey = Key;
						++Revision;
						if(Properties != nullptr)
							ReadArtwork(Properties.Thumbnail());
					}
				}
				else
				{
					LastKey.clear();
				}
			}
			catch(...)
			{
				// A session can disappear between the calls above, just try again.
			}
		}
		Track.m_Revision = Revision;

		{
			const std::lock_guard<std::mutex> Guard(m_Lock);
			m_Track = Track;
		}


		// Polling twice a second is plenty for a now playing display.
		for(int i = 0; i < 5 && m_Running.load(); ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	Manager = nullptr;
	if(ComInitialized)
		CoUninitialize();
}

CWindowsMusic::CWindowsMusic() :
	m_pImpl(std::make_unique<CImpl>())
{
}

CWindowsMusic::~CWindowsMusic() = default;

void CWindowsMusic::Start() { m_pImpl->Start(); }
void CWindowsMusic::Stop() { m_pImpl->Stop(); }

CWindowsMusic::CTrack CWindowsMusic::Track() const
{
	const std::lock_guard<std::mutex> Guard(m_pImpl->m_Lock);
	return m_pImpl->m_Track;
}

bool CWindowsMusic::TakeArtwork(std::vector<uint8_t> &vRgba, int &Width, int &Height)
{
	const std::lock_guard<std::mutex> Guard(m_pImpl->m_Lock);
	if(!m_pImpl->m_ArtworkFresh || m_pImpl->m_vArtwork.empty())
		return false;
	m_pImpl->m_ArtworkFresh = false;
	vRgba = m_pImpl->m_vArtwork;
	Width = m_pImpl->m_ArtworkWidth;
	Height = m_pImpl->m_ArtworkHeight;
	return true;
}

#else

// Not Windows: the pimpl still needs a complete type for the destructor.
class CWindowsMusic::CImpl
{
};

CWindowsMusic::CWindowsMusic() = default;
CWindowsMusic::~CWindowsMusic() = default;
void CWindowsMusic::Start() {}
void CWindowsMusic::Stop() {}
CWindowsMusic::CTrack CWindowsMusic::Track() const { return CTrack(); }
bool CWindowsMusic::TakeArtwork(std::vector<uint8_t> &vRgba, int &Width, int &Height)
{
	(void)vRgba;
	(void)Width;
	(void)Height;
	return false;
}

#endif
