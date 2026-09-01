#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_MEDIA_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_MEDIA_H

#include <base/detect.h>

#include <cstddef>
#include <cstdint>
#include <memory>

// Decodes pictures and videos with the codecs that ship with the system, so
// that no FFmpeg build with decoders is needed. On Windows that is WIC for
// pictures and Media Foundation for videos, on macOS ImageIO and AVFoundation.
// Everywhere else the class exists but decodes nothing, and the caller falls
// back to FFmpeg.
//
// Each platform's implementation lives in its own translation unit on purpose.
// The Windows COM headers declare an `IStorage` interface that clashes with the
// engine one, and the macOS one is Objective-C++; neither can be included from
// a file that also sees the engine headers.
class CSystemMedia
{
public:
	CSystemMedia();
	~CSystemMedia();

	// Both return false when the system cannot decode the file.
	bool OpenImage(const char *pAbsolutePath);
	bool OpenVideo(const char *pAbsolutePath);
	void Close();

	bool IsOpen() const;
	// True for a picture, which only has to be uploaded once.
	bool IsStill() const;
	int Width() const;
	int Height() const;

	// Loops the video back to the start after this many seconds. 0 plays the
	// whole file.
	void SetMaxDuration(double Seconds);

	// Fills the caller's buffer with `Width() * Height() * 4` bytes of RGBA. For
	// videos it returns false while the frame that is already shown is still the
	// right one for `Time` (in seconds), looping at the end.
	// Writes the frame straight into the caller's buffer, which it owns, so the
	// picture can go to the graphics thread without being copied again on the
	// way.
	bool NextFrame(double Time, uint8_t *pRgba, size_t Size);

private:
	class CImpl;
	std::unique_ptr<CImpl> m_pImpl;
};

#endif
