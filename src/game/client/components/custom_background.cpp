#include "custom_background.h"

#include "custom_media_win.h"

#include <base/io.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <vector>

#if defined(CONF_VIDEORECORDER)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

// Frames are scaled down to at most this size. A background does not need more
// than that, and every frame is a full texture upload.
static constexpr int MAX_FRAME_WIDTH = 1280;
static constexpr int MAX_FRAME_HEIGHT = 720;

class CCustomBackground::CMedia
{
public:
	~CMedia() { Close(); }

	// Opens the file. Takes ownership of `File` and closes it. `pPath` is only
	// used for log messages. Returns false if it cannot be decoded.
	bool Open(const char *pPath, IOHANDLE File);
	void Close();

	// Decodes the frame that should be shown at `Time` seconds since the start,
	// looping at the end. Returns false when the currently shown frame is still
	// the right one. `Image` receives an RGBA image the caller takes over.
	bool NextFrame(float Time, CImageInfo &Image);

	// True when the file holds a single picture, so it only has to be shown once.
	bool IsStill() const { return m_IsStill; }
	// False when the linked FFmpeg has no decoders compiled in.
	static bool HasDecoders();
	int Width() const { return m_Width; }
	int Height() const { return m_Height; }

private:
#if defined(CONF_VIDEORECORDER)
	AVFormatContext *m_pFormatContext = nullptr;
	AVCodecContext *m_pCodecContext = nullptr;
	SwsContext *m_pSwsContext = nullptr;
	AVFrame *m_pFrame = nullptr;
	AVPacket *m_pPacket = nullptr;
	AVIOContext *m_pIoContext = nullptr;
	IOHANDLE m_File = nullptr;
	int m_StreamIndex = -1;

	// FFmpeg reads through the engine file system, so that the file is found in
	// whichever storage path it lives in and no absolute path has to be built.
	static int IoRead(void *pOpaque, uint8_t *pBuffer, int BufferSize);
	static int64_t IoSeek(void *pOpaque, int64_t Offset, int Whence);

	int m_Width = 0;
	int m_Height = 0;
	// Presentation time of the frame that is currently shown, in seconds.
	double m_CurrentPts = -1.0;
	bool m_Eof = false;

	// Reads and decodes until one frame comes out. Returns false at the end of
	// the file.
	bool DecodeOne();
	void Rewind();
#else
	int m_Width = 0;
	int m_Height = 0;
#endif
	bool m_IsStill = false;
};

#if defined(CONF_VIDEORECORDER)

bool CCustomBackground::CMedia::HasDecoders()
{
	void *pIter = nullptr;
	return av_demuxer_iterate(&pIter) != nullptr;
}

int CCustomBackground::CMedia::IoRead(void *pOpaque, uint8_t *pBuffer, int BufferSize)
{
	const unsigned Read = io_read(static_cast<IOHANDLE>(pOpaque), pBuffer, (unsigned)BufferSize);
	return Read == 0 ? AVERROR_EOF : (int)Read;
}

int64_t CCustomBackground::CMedia::IoSeek(void *pOpaque, int64_t Offset, int Whence)
{
	IOHANDLE File = static_cast<IOHANDLE>(pOpaque);
	if(Whence == AVSEEK_SIZE)
		return io_length(File);

	EIoSeekOrigin Origin;
	switch(Whence)
	{
	case SEEK_SET: Origin = EIoSeekOrigin::START; break;
	case SEEK_CUR: Origin = EIoSeekOrigin::CURRENT; break;
	case SEEK_END: Origin = EIoSeekOrigin::END; break;
	default: return -1;
	}
	if(io_seek(File, Offset, Origin) != 0)
		return -1;
	return io_tell(File);
}

bool CCustomBackground::CMedia::Open(const char *pPath, IOHANDLE File)
{
	Close();
	m_File = File;

	constexpr int IoBufferSize = 32 * 1024;
	uint8_t *pIoBuffer = static_cast<uint8_t *>(av_malloc(IoBufferSize));
	if(pIoBuffer == nullptr)
	{
		Close();
		return false;
	}
	m_pIoContext = avio_alloc_context(pIoBuffer, IoBufferSize, 0, m_File, IoRead, nullptr, IoSeek);
	if(m_pIoContext == nullptr)
	{
		av_free(pIoBuffer);
		Close();
		return false;
	}

	m_pFormatContext = avformat_alloc_context();
	if(m_pFormatContext == nullptr)
	{
		Close();
		return false;
	}
	m_pFormatContext->pb = m_pIoContext;
	m_pFormatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

	const int OpenResult = avformat_open_input(&m_pFormatContext, nullptr, nullptr, nullptr);
	if(OpenResult != 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(OpenResult, aError, sizeof(aError));
		log_error("custombackground", "Could not open '%s': %s", pPath, aError);
		if(!HasDecoders())
		{
			log_error("custombackground", "The bundled FFmpeg was built for encoding only and has no decoders, so videos cannot be played. Replace avcodec-61.dll, avformat-61.dll, avutil-59.dll, swresample-5.dll and swscale-8.dll next to the executable with a full FFmpeg build of the same version to enable them. PNG images work without that.");
		}
		Close();
		return false;
	}
	if(avformat_find_stream_info(m_pFormatContext, nullptr) < 0)
	{
		log_error("custombackground", "No stream info in '%s'", pPath);
		Close();
		return false;
	}

	const AVCodec *pCodec = nullptr;
	m_StreamIndex = av_find_best_stream(m_pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if(m_StreamIndex < 0 || pCodec == nullptr)
	{
		log_error("custombackground", "No video stream in '%s'", pPath);
		Close();
		return false;
	}

	m_pCodecContext = avcodec_alloc_context3(pCodec);
	if(m_pCodecContext == nullptr ||
		avcodec_parameters_to_context(m_pCodecContext, m_pFormatContext->streams[m_StreamIndex]->codecpar) < 0 ||
		avcodec_open2(m_pCodecContext, pCodec, nullptr) < 0)
	{
		log_error("custombackground", "Could not open the decoder for '%s'", pPath);
		Close();
		return false;
	}

	m_pFrame = av_frame_alloc();
	m_pPacket = av_packet_alloc();
	if(m_pFrame == nullptr || m_pPacket == nullptr)
	{
		Close();
		return false;
	}

	// Keep the aspect ratio while staying inside the frame size limit.
	const int SrcWidth = std::max(1, m_pCodecContext->width);
	const int SrcHeight = std::max(1, m_pCodecContext->height);
	const float Scale = std::min({1.0f, MAX_FRAME_WIDTH / (float)SrcWidth, MAX_FRAME_HEIGHT / (float)SrcHeight});
	// Even dimensions keep the scaler on its fast paths.
	m_Width = std::max(2, (int)(SrcWidth * Scale) & ~1);
	m_Height = std::max(2, (int)(SrcHeight * Scale) & ~1);

	// A single picture (png, jpg, ...) shows up as a stream with one frame.
	const AVStream *pStream = m_pFormatContext->streams[m_StreamIndex];
	m_IsStill = pStream->nb_frames == 1 || m_pFormatContext->duration <= 0;

	m_CurrentPts = -1.0;
	m_Eof = false;
	return true;
}

void CCustomBackground::CMedia::Close()
{
	if(m_pSwsContext != nullptr)
	{
		sws_freeContext(m_pSwsContext);
		m_pSwsContext = nullptr;
	}
	if(m_pFrame != nullptr)
		av_frame_free(&m_pFrame);
	if(m_pPacket != nullptr)
		av_packet_free(&m_pPacket);
	if(m_pCodecContext != nullptr)
		avcodec_free_context(&m_pCodecContext);
	if(m_pFormatContext != nullptr)
		avformat_close_input(&m_pFormatContext);
	if(m_pIoContext != nullptr)
	{
		// The context may have swapped its buffer, so free the current one.
		av_freep(&m_pIoContext->buffer);
		avio_context_free(&m_pIoContext);
	}
	if(m_File != nullptr)
	{
		io_close(m_File);
		m_File = nullptr;
	}
	m_StreamIndex = -1;
	m_CurrentPts = -1.0;
	m_Eof = false;
	m_IsStill = false;
}

void CCustomBackground::CMedia::Rewind()
{
	av_seek_frame(m_pFormatContext, m_StreamIndex, 0, AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(m_pCodecContext);
	m_CurrentPts = -1.0;
	m_Eof = false;
}

bool CCustomBackground::CMedia::DecodeOne()
{
	while(true)
	{
		const int Received = avcodec_receive_frame(m_pCodecContext, m_pFrame);
		if(Received == 0)
			return true;
		if(Received != AVERROR(EAGAIN) && Received != AVERROR_EOF)
			return false;
		if(Received == AVERROR_EOF)
			return false;

		const int Read = av_read_frame(m_pFormatContext, m_pPacket);
		if(Read < 0)
		{
			// Flush whatever is still buffered in the decoder.
			avcodec_send_packet(m_pCodecContext, nullptr);
			if(avcodec_receive_frame(m_pCodecContext, m_pFrame) == 0)
				return true;
			return false;
		}

		if(m_pPacket->stream_index == m_StreamIndex)
			avcodec_send_packet(m_pCodecContext, m_pPacket);
		av_packet_unref(m_pPacket);
	}
}

bool CCustomBackground::CMedia::NextFrame(float Time, CImageInfo &Image)
{
	if(m_pCodecContext == nullptr)
		return false;

	const AVStream *pStream = m_pFormatContext->streams[m_StreamIndex];
	const double TimeBase = av_q2d(pStream->time_base);
	const double Duration = m_pFormatContext->duration > 0 ? m_pFormatContext->duration / (double)AV_TIME_BASE : 0.0;
	const double Wanted = m_IsStill || Duration <= 0.0 ? 0.0 : std::fmod((double)Time, Duration);

	if(m_CurrentPts >= 0.0)
	{
		if(m_IsStill)
			return false;
		// The video looped, start over.
		if(Wanted < m_CurrentPts)
			Rewind();
		else if(Wanted < m_CurrentPts + 0.001)
			return false;
	}

	bool Got = false;
	// Skip ahead if the game is running faster than the video, but never spend
	// an unbounded amount of time in one render frame.
	for(int i = 0; i < 8; ++i)
	{
		if(!DecodeOne())
		{
			if(m_IsStill || i > 0)
				break;
			Rewind();
			if(!DecodeOne())
				return false;
		}
		Got = true;
		const int64_t Timestamp = m_pFrame->best_effort_timestamp;
		m_CurrentPts = Timestamp == AV_NOPTS_VALUE ? Wanted : Timestamp * TimeBase;
		if(m_IsStill || m_CurrentPts >= Wanted)
			break;
	}
	if(!Got)
		return false;

	m_pSwsContext = sws_getCachedContext(m_pSwsContext,
		m_pFrame->width, m_pFrame->height, (AVPixelFormat)m_pFrame->format,
		m_Width, m_Height, AV_PIX_FMT_RGBA,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	if(m_pSwsContext == nullptr)
		return false;

	Image.Free();
	Image.m_Width = m_Width;
	Image.m_Height = m_Height;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();

	uint8_t *apDst[4] = {Image.m_pData, nullptr, nullptr, nullptr};
	int aDstLineSize[4] = {(int)(m_Width * 4), 0, 0, 0};
	sws_scale(m_pSwsContext, m_pFrame->data, m_pFrame->linesize, 0, m_pFrame->height, apDst, aDstLineSize);
	return true;
}

#else

bool CCustomBackground::CMedia::HasDecoders()
{
	return false;
}
bool CCustomBackground::CMedia::Open(const char *pPath, IOHANDLE File)
{
	log_error("custombackground", "This build was compiled without FFmpeg, only PNG backgrounds are available. File='%s'", pPath);
	if(File != nullptr)
		io_close(File);
	return false;
}
void CCustomBackground::CMedia::Close() {}
bool CCustomBackground::CMedia::NextFrame(float Time, CImageInfo &Image)
{
	(void)Time;
	(void)Image;
	return false;
}

#endif

CCustomBackground::CCustomBackground() :
	m_pMedia(std::make_unique<CMedia>())
{
#if defined(CONF_FAMILY_WINDOWS)
	m_pWindowsMedia = std::make_unique<CWindowsMedia>();
#endif
	m_Texture.Invalidate();
}

CCustomBackground::~CCustomBackground() = default;

void CCustomBackground::Unload()
{
	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
	m_pMedia->Close();
#if defined(CONF_FAMILY_WINDOWS)
	m_pWindowsMedia->Close();
	m_UsingWindowsMedia = false;
#endif
	m_LoadedFile.clear();
	m_LoadFailed = false;
	m_IsStill = false;
	m_HasFrame = false;
	m_Width = 0;
	m_Height = 0;
}

void CCustomBackground::OnShutdown()
{
	Unload();
}

void CCustomBackground::Update()
{
	const char *pFile = g_Config.m_ClCustomBackgroundFile;
	if(pFile[0] == '\0' || g_Config.m_ClCustomBackground == 0)
	{
		if(!m_LoadedFile.empty() || m_Texture.IsValid())
			Unload();
		return;
	}

	if(m_LoadedFile != pFile)
	{
		Unload();
		m_LoadedFile = pFile;

		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "backgrounds/%s", pFile);

		// The system decoder needs a real path, so find the storage path the
		// file lives in and build the absolute one.
		char aAbsolutePath[IO_MAX_PATH_LENGTH];
		aAbsolutePath[0] = 0;
		for(int Type = IStorage::TYPE_SAVE; Type < Storage()->NumPaths(); ++Type)
		{
			if(Storage()->FileExists(aPath, Type))
			{
				Storage()->GetCompletePath(Type, aPath, aAbsolutePath, sizeof(aAbsolutePath));
				break;
			}
		}

#if defined(CONF_FAMILY_WINDOWS)
		// Anything that is not a PNG goes to the Windows codecs first: WIC reads
		// jpg, bmp, gif and friends, Media Foundation plays mp4 and whatever
		// else the system can decode.
		if(aAbsolutePath[0] != 0 && str_endswith_nocase(pFile, ".png") == nullptr)
		{
			const bool LooksLikeImage =
				str_endswith_nocase(pFile, ".jpg") != nullptr ||
				str_endswith_nocase(pFile, ".jpeg") != nullptr ||
				str_endswith_nocase(pFile, ".bmp") != nullptr ||
				str_endswith_nocase(pFile, ".webp") != nullptr ||
				str_endswith_nocase(pFile, ".tif") != nullptr ||
				str_endswith_nocase(pFile, ".tiff") != nullptr;
			// A gif can be animated, so it goes to the video decoder first.
			const bool Opened = LooksLikeImage ?
						    m_pWindowsMedia->OpenImage(aAbsolutePath) :
						    (m_pWindowsMedia->OpenVideo(aAbsolutePath) || m_pWindowsMedia->OpenImage(aAbsolutePath));
			if(Opened)
			{
				m_UsingWindowsMedia = true;
				m_IsStill = m_pWindowsMedia->IsStill();
				m_Width = m_pWindowsMedia->Width();
				m_Height = m_pWindowsMedia->Height();
			}
			else
			{
				m_LoadFailed = true;
				return;
			}
		}
		else
#endif
		// PNG goes through the engine image loader, which is always available.
		// Everything else needs a decoder from FFmpeg.
		if(str_endswith_nocase(pFile, ".png") != nullptr)
		{
			CImageInfo Image;
			if(!Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL))
			{
				m_LoadFailed = true;
				return;
			}
			m_Width = Image.m_Width;
			m_Height = Image.m_Height;
			m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "custom background");
			m_HasFrame = m_Texture.IsValid() && !m_Texture.IsNullTexture();
			m_IsStill = true;
			if(!m_HasFrame)
			{
				m_Texture.Invalidate();
				m_LoadFailed = true;
			}
			return;
		}

		else
		{
			IOHANDLE MediaFile = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
			if(MediaFile == nullptr)
			{
				m_LoadFailed = true;
				log_error("custombackground", "Could not find '%s'", aPath);
				return;
			}
			if(!m_pMedia->Open(aPath, MediaFile))
			{
				m_LoadFailed = true;
				return;
			}
			m_IsStill = m_pMedia->IsStill();
			m_Width = m_pMedia->Width();
			m_Height = m_pMedia->Height();
		}
	}

	if(m_LoadFailed || (m_IsStill && m_HasFrame))
		return;

	CImageInfo Frame;
#if defined(CONF_FAMILY_WINDOWS)
	if(m_UsingWindowsMedia)
	{
		std::vector<uint8_t> vRgba;
		if(!m_pWindowsMedia->NextFrame(Client()->GlobalTime(), vRgba) || vRgba.empty())
			return;
		Frame.m_Width = m_pWindowsMedia->Width();
		Frame.m_Height = m_pWindowsMedia->Height();
		Frame.m_Format = CImageInfo::FORMAT_RGBA;
		Frame.Allocate();
		mem_copy(Frame.m_pData, vRgba.data(), std::min(vRgba.size(), Frame.DataSize()));
	}
	else
#endif
		if(!m_pMedia->NextFrame(Client()->GlobalTime(), Frame))
		return;

	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
	m_Texture = Graphics()->LoadTextureRawMove(Frame, 0, "custom background");
	m_HasFrame = m_Texture.IsValid() && !m_Texture.IsNullTexture();
	if(!m_HasFrame)
		m_Texture.Invalidate();
}

bool CCustomBackground::RenderFullscreen()
{
	if(g_Config.m_ClCustomBackground == 0)
		return false;

	Update();
	if(!m_HasFrame || !m_Texture.IsValid())
		return false;

	// Everything drawn after this keeps using the caller's coordinate system, so
	// the mapping has to be put back. Without that the menu is laid out in this
	// 300 unit high space and comes out oversized and clipped.
	const CScreenRect SavedScreenRect = Graphics()->GetScreen();

	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenWidth, ScreenHeight);

	// The texture is always drawn over the full screen; `cl_custom_background_fit`
	// decides how the aspect ratio mismatch is resolved.
	float x = 0.0f, y = 0.0f, w = ScreenWidth, h = ScreenHeight;
	if(g_Config.m_ClCustomBackgroundFit != 0 && m_Width > 0 && m_Height > 0)
	{
		const float ImageAspect = m_Width / (float)m_Height;
		const float ScreenAspect = Graphics()->ScreenAspect();
		const bool Cover = g_Config.m_ClCustomBackgroundFit == 1;
		// Cover fills the screen and crops the overhang, contain fits the whole
		// image in and leaves bars.
		if((ImageAspect > ScreenAspect) == Cover)
		{
			w = ScreenHeight * ImageAspect;
			x = (ScreenWidth - w) / 2.0f;
		}
		else
		{
			h = ScreenWidth / ImageAspect;
			y = (ScreenHeight - h) / 2.0f;
		}
	}

	const float Opacity = g_Config.m_ClCustomBackgroundOpacity / 100.0f;
	Graphics()->TextureSet(m_Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Opacity);
	const IGraphics::CQuadItem QuadItem(x, y, w, h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	Graphics()->MapScreen(SavedScreenRect);
	return true;
}

void CCustomBackground::OnRender()
{
	if(g_Config.m_ClCustomBackground == 0 || !g_Config.m_ClCustomBackgroundIngame)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderFullscreen();
}
