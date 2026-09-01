// The macOS side of CSystemMedia. Like the rest of the Objective-C++ in the
// tree it is compiled without ARC, so every object made here is given back by
// hand.

#include "custom_media.h"

#if defined(CONF_PLATFORM_MACOS)

#include <base/log.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
// Videos are scaled down to at most this size, every frame is a full texture
// upload and a background does not need more than that.
constexpr int MAX_VIDEO_WIDTH = 1280;
constexpr int MAX_VIDEO_HEIGHT = 720;

template<typename T>
void SafeRelease(T &Ref)
{
	if(Ref != nullptr)
	{
		CFRelease(Ref);
		Ref = nullptr;
	}
}

// CoreGraphics has no straight alpha layout at eight bits per channel, so a
// picture with real transparency comes out of the context with its colours
// already multiplied by their alpha. Every other decoder this feature has hands
// over straight alpha -- WIC converts to 32bppRGBA, FFmpeg to AV_PIX_FMT_RGBA,
// and the engine's own PNG loader is straight as well -- and the graphics
// pipeline multiplies by alpha itself when it blends. Left alone the colour
// would therefore be multiplied twice where the background is see-through, and
// not at all where it is drawn opaque, so the same file would look different on
// a Mac than it does anywhere else. Undoing it here is what keeps them the same.
//
// It cannot be undone exactly: the multiplication already happened in eight bits
// and threw precision away, so a nearly transparent pixel comes back banded.
// That is a better answer than a picture that is visibly darker.
void UnpremultiplyRgba(uint8_t *pRgba, size_t Size)
{
	for(size_t i = 0; i + 3 < Size; i += 4)
	{
		const unsigned Alpha = pRgba[i + 3];
		if(Alpha == 0 || Alpha == 255)
			continue;
		for(size_t c = 0; c < 3; ++c)
			pRgba[i + c] = (uint8_t)std::min<unsigned>(255, (pRgba[i + c] * 255u + Alpha / 2) / Alpha);
	}
}

double SampleSeconds(CMSampleBufferRef Sample)
{
	const CMTime Timestamp = CMSampleBufferGetPresentationTimeStamp(Sample);
	// A track that carries no timestamps hands out an invalid time, which would
	// come out of CMTimeGetSeconds() as a NaN and compare false against every
	// limit, so the file would never wrap around.
	if(!CMTIME_IS_NUMERIC(Timestamp))
		return 0.0;
	return CMTimeGetSeconds(Timestamp);
}
} // namespace

class CSystemMedia::CImpl
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

	AVURLAsset *m_pAsset = nullptr;
	AVAssetTrack *m_pTrack = nullptr;
	// Kept because looping builds a second reader, which has to be asked for the
	// same pixel format and the same size as the first one.
	NSDictionary *m_pOutputSettings = nullptr;
	AVAssetReader *m_pReader = nullptr;
	AVAssetReaderTrackOutput *m_pOutput = nullptr;
	// The frame decoded while opening, which is what told us the size the reader
	// really hands out. The first NextFrame() call takes it.
	CMSampleBufferRef m_PendingSample = nullptr;

	// The size the reader hands out, which is the frame the way the file stores
	// it. m_Width and m_Height are those two after the turn below.
	int m_SourceWidth = 0;
	int m_SourceHeight = 0;
	// Degrees clockwise the file asks for, always a multiple of 90. A track
	// output hands out the stored frame and leaves the track's transform to
	// whoever puts it on screen, so a video shot in portrait arrives lying on its
	// side and has to be turned here.
	int m_Rotation = 0;
	double m_Duration = 0.0;
	// How long one frame is shown, taken from the frame rate of the file.
	double m_FrameInterval = 1.0 / 30.0;
	// Wall clock time the next frame is due at. The video is played forward
	// instead of seeking to a position derived from the clock, which would make
	// the decoder jump around and decode far more frames than needed.
	double m_NextFrameTime = -1.0;

	bool StartReader();
	void ReleaseReader();
	CMSampleBufferRef NextSample();
	bool CopyFrame(CMSampleBufferRef Sample, uint8_t *pRgba);
};

bool CSystemMedia::CImpl::OpenImage(const char *pAbsolutePath)
{
	Close();

	CGImageSourceRef Source = nullptr;
	CGImageRef Image = nullptr;
	CGColorSpaceRef ColorSpace = nullptr;
	CGContextRef Context = nullptr;
	bool Success = false;

	@autoreleasepool
	{
		do
		{
			NSString *pPath = [NSString stringWithUTF8String:pAbsolutePath];
			if(pPath == nullptr)
				break;
			Source = CGImageSourceCreateWithURL((CFURLRef)[NSURL fileURLWithPath:pPath], nullptr);
			if(Source == nullptr)
				break;
			// Only the first picture in the file, which is all there is in anything
			// but an animation.
			Image = CGImageSourceCreateImageAtIndex(Source, 0, nullptr);
			if(Image == nullptr)
				break;

			const int Width = (int)CGImageGetWidth(Image);
			const int Height = (int)CGImageGetHeight(Image);
			if(Width <= 0 || Height <= 0)
				break;

			ColorSpace = CGColorSpaceCreateDeviceRGB();
			if(ColorSpace == nullptr)
				break;

			m_vStillRgba.resize((size_t)Width * Height * 4);
			// Drawing into a context of our own is what does the work: whatever the
			// file holds, be it indexed, CMYK or sixteen bits per channel, comes out
			// of this in the one layout the graphics card takes.
			//
			// The two constants are different enum types, so they are ored as plain
			// numbers; adding them as enums is deprecated since C++20 and warns.
			Context = CGBitmapContextCreate(m_vStillRgba.data(), Width, Height, 8, (size_t)Width * 4, ColorSpace,
				(CGBitmapInfo)((uint32_t)kCGImageAlphaPremultipliedLast | (uint32_t)kCGBitmapByteOrder32Big));
			if(Context == nullptr)
				break;
			CGContextDrawImage(Context, CGRectMake(0.0, 0.0, Width, Height), Image);
			UnpremultiplyRgba(m_vStillRgba.data(), m_vStillRgba.size());

			m_Width = Width;
			m_Height = Height;
			m_IsStill = true;
			m_IsOpen = true;
			Success = true;
		} while(false);
	}

	SafeRelease(Context);
	SafeRelease(ColorSpace);
	SafeRelease(Image);
	SafeRelease(Source);

	if(!Success)
	{
		m_vStillRgba.clear();
		m_vStillRgba.shrink_to_fit();
		log_error("custombackground", "macOS cannot decode the picture '%s'", pAbsolutePath);
	}
	return Success;
}

bool CSystemMedia::CImpl::OpenVideo(const char *pAbsolutePath)
{
	Close();

	@autoreleasepool
	{
		NSString *pPath = [NSString stringWithUTF8String:pAbsolutePath];
		if(pPath == nullptr)
			return false;

		m_pAsset = [[AVURLAsset alloc] initWithURL:[NSURL fileURLWithPath:pPath] options:nullptr];
		if(m_pAsset == nullptr)
		{
			log_error("custombackground", "macOS has no codec for '%s'", pAbsolutePath);
			Close();
			return false;
		}

		NSArray *pTracks = [m_pAsset tracksWithMediaType:AVMediaTypeVideo];
		if(pTracks.count == 0)
		{
			log_error("custombackground", "'%s' has no video track", pAbsolutePath);
			Close();
			return false;
		}
		m_pTrack = [[pTracks objectAtIndex:0] retain];

		const CMTime AssetDuration = m_pAsset.duration;
		if(CMTIME_IS_NUMERIC(AssetDuration))
			m_Duration = CMTimeGetSeconds(AssetDuration);
		if(m_pTrack.nominalFrameRate > 0.0f)
			m_FrameInterval = 1.0 / m_pTrack.nominalFrameRate;

		// The angle the transform turns the frame by. Only whole quarter turns are
		// worth taking apart; anything else in there is a mirror or a scale, and no
		// camera writes one of those into a plain video file.
		const CGAffineTransform Transform = m_pTrack.preferredTransform;
		const int Degrees = (((int)std::lround(std::atan2(Transform.b, Transform.a) * 180.0 / M_PI) % 360) + 360) % 360;
		m_Rotation = ((Degrees + 45) / 90 * 90) % 360;
		const bool Swapped = m_Rotation == 90 || m_Rotation == 270;

		const CGSize NaturalSize = m_pTrack.naturalSize;
		const int NativeWidth = (int)std::lround(std::fabs(NaturalSize.width));
		const int NativeHeight = (int)std::lround(std::fabs(NaturalSize.height));
		if(NativeWidth <= 0 || NativeHeight <= 0)
		{
			log_error("custombackground", "'%s' has no usable frame size", pAbsolutePath);
			Close();
			return false;
		}
		// The limit is about how much of the screen a frame ends up covering, so it
		// is measured on the picture the way it will be seen, after the turn.
		const int DisplayWidth = Swapped ? NativeHeight : NativeWidth;
		const int DisplayHeight = Swapped ? NativeWidth : NativeHeight;

		NSMutableDictionary *pSettings = [NSMutableDictionary dictionaryWithCapacity:3];
		[pSettings setObject:[NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA] forKey:(NSString *)kCVPixelBufferPixelFormatTypeKey];
		// Ask for a smaller frame when the file is larger than a background needs.
		// Every frame is copied across the process and uploaded to the graphics card,
		// so a 4K file costs nine times what a 720p one does for a picture that ends
		// up on the same screen. The reader scales while it decodes, which is far
		// cheaper than decoding the whole frame and scaling it afterwards.
		if(DisplayWidth > MAX_VIDEO_WIDTH || DisplayHeight > MAX_VIDEO_HEIGHT)
		{
			const double Scale = std::min((double)MAX_VIDEO_WIDTH / DisplayWidth, (double)MAX_VIDEO_HEIGHT / DisplayHeight);
			// Asked for in the orientation the file stores, because that is what the
			// reader works in. Even numbers, because that is what the scaler wants.
			const int WantedWidth = std::max(2, ((int)(NativeWidth * Scale) / 2) * 2);
			const int WantedHeight = std::max(2, ((int)(NativeHeight * Scale) / 2) * 2);
			[pSettings setObject:[NSNumber numberWithInt:WantedWidth] forKey:(NSString *)kCVPixelBufferWidthKey];
			[pSettings setObject:[NSNumber numberWithInt:WantedHeight] forKey:(NSString *)kCVPixelBufferHeightKey];
		}
		m_pOutputSettings = [pSettings copy];

		if(!StartReader())
		{
			log_error("custombackground", "macOS cannot read '%s'", pAbsolutePath);
			Close();
			return false;
		}

		// Nothing makes the reader honour the size it was asked for, and the caller
		// sizes its buffer from Width() and Height() before it ever asks for a
		// frame. So the first frame is decoded here and kept for the first
		// NextFrame() call: the size it arrives in is the size this file plays at.
		m_PendingSample = [m_pOutput copyNextSampleBuffer];
		CVImageBufferRef Frame = m_PendingSample == nullptr ? nullptr : CMSampleBufferGetImageBuffer(m_PendingSample);
		if(Frame == nullptr)
		{
			log_error("custombackground", "Could not decode a frame of '%s'", pAbsolutePath);
			Close();
			return false;
		}
		m_SourceWidth = (int)CVPixelBufferGetWidth(Frame);
		m_SourceHeight = (int)CVPixelBufferGetHeight(Frame);
		if(m_SourceWidth <= 0 || m_SourceHeight <= 0)
		{
			Close();
			return false;
		}
		m_Width = Swapped ? m_SourceHeight : m_SourceWidth;
		m_Height = Swapped ? m_SourceWidth : m_SourceHeight;

		m_IsStill = m_Duration <= 0.0;
		m_NextFrameTime = -1.0;
		m_IsOpen = true;
	}

	if(m_Width > MAX_VIDEO_WIDTH || m_Height > MAX_VIDEO_HEIGHT)
	{
		log_info("custombackground", "Video stayed at %dx%d, this source cannot be scaled while decoding; a smaller file is cheaper", m_Width, m_Height);
	}
	return true;
}

bool CSystemMedia::CImpl::StartReader()
{
	// An AVAssetReader only runs forward and cannot be rewound, so both opening
	// the file and looping it come through here.
	ReleaseReader();

	m_pReader = [[AVAssetReader alloc] initWithAsset:m_pAsset error:nullptr];
	if(m_pReader == nullptr)
		return false;

	m_pOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:m_pTrack outputSettings:m_pOutputSettings];
	if(m_pOutput == nullptr)
	{
		ReleaseReader();
		return false;
	}
	// Every frame is copied out and let go of before the next one is asked for, so
	// the reader does not have to hand out a copy of its own. It only takes this
	// while reading has not started yet, which is why it is set here.
	m_pOutput.alwaysCopiesSampleData = NO;
	if(![m_pReader canAddOutput:m_pOutput])
	{
		ReleaseReader();
		return false;
	}
	[m_pReader addOutput:m_pOutput];

	if(![m_pReader startReading])
	{
		ReleaseReader();
		return false;
	}
	return true;
}

void CSystemMedia::CImpl::ReleaseReader()
{
	SafeRelease(m_PendingSample);
	if(m_pReader != nullptr)
	{
		// The reader decodes ahead on a thread of its own, and without this it would
		// keep working on a file nobody is going to ask about again.
		[m_pReader cancelReading];
		[m_pReader release];
		m_pReader = nullptr;
	}
	[m_pOutput release];
	m_pOutput = nullptr;
}

void CSystemMedia::CImpl::Close()
{
	@autoreleasepool
	{
		ReleaseReader();
		[m_pOutputSettings release];
		m_pOutputSettings = nullptr;
		[m_pTrack release];
		m_pTrack = nullptr;
		[m_pAsset release];
		m_pAsset = nullptr;
	}
	m_vStillRgba.clear();
	m_vStillRgba.shrink_to_fit();
	m_IsOpen = false;
	m_IsStill = false;
	m_Width = 0;
	m_Height = 0;
	m_SourceWidth = 0;
	m_SourceHeight = 0;
	m_Rotation = 0;
	m_Duration = 0.0;
	m_FrameInterval = 1.0 / 30.0;
	m_NextFrameTime = -1.0;
	// m_MaxDuration is a setting, not file state, so it survives Close().
}

CMSampleBufferRef CSystemMedia::CImpl::NextSample()
{
	// The frame that was decoded while opening is handed out before any new one.
	if(m_PendingSample != nullptr)
	{
		CMSampleBufferRef Sample = m_PendingSample;
		m_PendingSample = nullptr;
		return Sample;
	}

	// One frame per call, the file is played forward. Two attempts are enough:
	// the second one is for the frame right after wrapping around at the end.
	for(int i = 0; i < 2; ++i)
	{
		CMSampleBufferRef Sample = [m_pOutput copyNextSampleBuffer];
		// A file longer than the limit is simply cut there and starts over.
		const bool PastLimit = Sample != nullptr && m_MaxDuration > 0.0 && SampleSeconds(Sample) >= m_MaxDuration;
		if(Sample == nullptr || PastLimit)
		{
			SafeRelease(Sample);
			// A reader that was just built and still hands out nothing means the file
			// has nothing left to give, and building another one would never end.
			if(i > 0 || !StartReader())
				return nullptr;
			continue;
		}
		return Sample;
	}
	return nullptr;
}

bool CSystemMedia::CImpl::CopyFrame(CMSampleBufferRef Sample, uint8_t *pRgba)
{
	CVImageBufferRef Frame = CMSampleBufferGetImageBuffer(Sample);
	if(Frame == nullptr)
		return false;
	// The caller sized its buffer from Width() and Height() when the file was
	// opened, so a reader that starts handing out something else after a loop must
	// not be written into it.
	if((int)CVPixelBufferGetWidth(Frame) != m_SourceWidth || (int)CVPixelBufferGetHeight(Frame) != m_SourceHeight)
		return false;
	if(CVPixelBufferLockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
		return false;

	const uint8_t *pBase = (const uint8_t *)CVPixelBufferGetBaseAddress(Frame);
	// Rows are padded out to whatever the decoder found convenient, which is
	// hardly ever the width times four.
	const size_t BytesPerRow = CVPixelBufferGetBytesPerRow(Frame);
	const bool Usable = pBase != nullptr && BytesPerRow >= (size_t)m_SourceWidth * 4;
	if(Usable)
	{
		// Where the pixel in the top left corner of the stored frame belongs, and
		// how far along the next one in a row and the first one of the next row are
		// from it. The turn the file asks for costs nothing more than these three
		// numbers, because every pixel is being written one by one anyway.
		int Base, RowStep, PixelStep;
		switch(m_Rotation)
		{
		case 90:
			Base = m_SourceHeight - 1;
			RowStep = -1;
			PixelStep = m_Width;
			break;
		case 180:
			Base = (m_SourceHeight - 1) * m_Width + m_SourceWidth - 1;
			RowStep = -m_Width;
			PixelStep = -1;
			break;
		case 270:
			Base = (m_SourceWidth - 1) * m_Width;
			RowStep = 1;
			PixelStep = -m_Width;
			break;
		default:
			Base = 0;
			RowStep = m_Width;
			PixelStep = 1;
			break;
		}

		for(int y = 0; y < m_SourceHeight; ++y)
		{
			const uint8_t *pSource = pBase + (size_t)y * BytesPerRow;
			int Target = Base + y * RowStep;
			for(int x = 0; x < m_SourceWidth; ++x, Target += PixelStep)
			{
				uint32_t Pixel;
				std::memcpy(&Pixel, pSource + (size_t)x * 4, sizeof(Pixel));
				// 32BGRA keeps the colours the other way round. A whole pixel at a
				// time rather than four bytes: this loop runs over every pixel of every
				// frame, and byte-wise it was one of the more expensive things in the
				// client.
				Pixel = 0xFF000000u | ((Pixel & 0x00FF0000u) >> 16) | (Pixel & 0x0000FF00u) | ((Pixel & 0x000000FFu) << 16);
				std::memcpy(pRgba + (size_t)Target * 4, &Pixel, sizeof(Pixel));
			}
		}
	}

	CVPixelBufferUnlockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly);
	return Usable;
}

bool CSystemMedia::CImpl::NextFrame(double Time, uint8_t *pRgba, size_t Size)
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

	// Asked before a frame is taken off the reader, so that a buffer too small to
	// hold one does not swallow the frame it cannot take.
	if(pRgba == nullptr || Size < (size_t)m_Width * m_Height * 4)
		return false;

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

	@autoreleasepool
	{
		CMSampleBufferRef Sample = NextSample();
		if(Sample == nullptr)
			return false;
		const bool Success = CopyFrame(Sample, pRgba);
		CFRelease(Sample);
		return Success;
	}
}

CSystemMedia::CSystemMedia() :
	m_pImpl(std::make_unique<CImpl>())
{
}

CSystemMedia::~CSystemMedia() = default;

bool CSystemMedia::OpenImage(const char *pAbsolutePath) { return m_pImpl->OpenImage(pAbsolutePath); }
bool CSystemMedia::OpenVideo(const char *pAbsolutePath) { return m_pImpl->OpenVideo(pAbsolutePath); }
void CSystemMedia::Close() { m_pImpl->Close(); }
bool CSystemMedia::IsOpen() const { return m_pImpl->m_IsOpen; }
bool CSystemMedia::IsStill() const { return m_pImpl->m_IsStill; }
int CSystemMedia::Width() const { return m_pImpl->m_Width; }
int CSystemMedia::Height() const { return m_pImpl->m_Height; }
void CSystemMedia::SetMaxDuration(double Seconds) { m_pImpl->m_MaxDuration = Seconds > 0.0 ? Seconds : 0.0; }
bool CSystemMedia::NextFrame(double Time, uint8_t *pRgba, size_t Size) { return m_pImpl->NextFrame(Time, pRgba, Size); }

#endif
