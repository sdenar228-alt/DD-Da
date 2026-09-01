// The platforms whose system decoders the client does not use. Every open is
// refused, so the background falls back to FFmpeg and to the engine's own PNG
// loader, which is what happened before any system decoder existed.

#include "custom_media.h"

#if !defined(CONF_FAMILY_WINDOWS) && !defined(CONF_PLATFORM_MACOS)

// The pimpl still needs a complete type for the destructor.
class CSystemMedia::CImpl
{
};

CSystemMedia::CSystemMedia() = default;
CSystemMedia::~CSystemMedia() = default;

bool CSystemMedia::OpenImage(const char *pAbsolutePath)
{
	(void)pAbsolutePath;
	return false;
}

bool CSystemMedia::OpenVideo(const char *pAbsolutePath)
{
	(void)pAbsolutePath;
	return false;
}

void CSystemMedia::Close() {}
bool CSystemMedia::IsOpen() const { return false; }
bool CSystemMedia::IsStill() const { return false; }
int CSystemMedia::Width() const { return 0; }
int CSystemMedia::Height() const { return 0; }

void CSystemMedia::SetMaxDuration(double Seconds)
{
	(void)Seconds;
}

bool CSystemMedia::NextFrame(double Time, uint8_t *pRgba, size_t Size)
{
	(void)Time;
	(void)pRgba;
	(void)Size;
	return false;
}

#endif
