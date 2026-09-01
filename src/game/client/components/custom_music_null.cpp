// The platforms that publish no now playing information the client can read.
// Nothing is ever reported, so the music island never appears.

#include "custom_music.h"

#if !defined(CONF_FAMILY_WINDOWS) && !defined(CONF_PLATFORM_MACOS)

// The pimpl still needs a complete type for the destructor.
class CSystemMusic::CImpl
{
};

CSystemMusic::CSystemMusic() = default;
CSystemMusic::~CSystemMusic() = default;

void CSystemMusic::Start() {}
void CSystemMusic::Stop() {}

void CSystemMusic::SendCommand(ECommand Command)
{
	(void)Command;
}

CSystemMusic::CTrack CSystemMusic::Track() const { return CTrack(); }

bool CSystemMusic::TakeArtwork(std::vector<uint8_t> &vRgba, int &Width, int &Height)
{
	(void)vRgba;
	(void)Width;
	(void)Height;
	return false;
}

#endif
