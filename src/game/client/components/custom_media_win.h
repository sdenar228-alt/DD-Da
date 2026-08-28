#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_MEDIA_WIN_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_MEDIA_WIN_H

#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)

#include <cstdint>
#include <memory>
#include <vector>

// Decodes pictures and videos with the codecs that ship with Windows, so that
// no FFmpeg build with decoders is needed. Pictures go through WIC (jpg, bmp,
// gif, webp, tiff, ...), videos through Media Foundation (mp4/h264 and whatever
// else the system has a codec for).
//
// This lives in its own translation unit on purpose: the Windows COM headers
// declare an `IStorage` interface, which clashes with the engine one.
class CWindowsMedia
{
public:
	CWindowsMedia();
	~CWindowsMedia();

	// Both return false when Windows cannot decode the file.
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

	// Fills `vRgba` with `Width() * Height() * 4` bytes of RGBA. For videos it
	// returns false while the frame that is already shown is still the right one
	// for `Time` (in seconds), looping at the end.
	bool NextFrame(double Time, std::vector<uint8_t> &vRgba);

private:
	class CImpl;
	std::unique_ptr<CImpl> m_pImpl;
};

#endif

#endif
