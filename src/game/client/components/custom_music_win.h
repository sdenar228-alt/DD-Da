#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_MUSIC_WIN_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_MUSIC_WIN_H

#include <base/detect.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Reads what is currently playing from the Windows media session, the same
// source that feeds the volume flyout. Every app that reports to the system
// shows up there: Spotify, browsers, the Groove player and so on.
//
// The query runs on its own thread, because the WinRT calls block, and the
// result is published as a snapshot. Like the video decoder this lives in its
// own translation unit, the Windows headers clash with the engine ones.
class CWindowsMusic
{
public:
	CWindowsMusic();
	~CWindowsMusic();

	// Starts the worker. Safe to call more than once.
	void Start();
	void Stop();

	class CTrack
	{
	public:
		std::string m_Title;
		std::string m_Artist;
		bool m_Playing = false;
		// Position and length in seconds, both 0 when the player reports none.
		double m_Position = 0.0;
		double m_Duration = 0.0;
		// Counts up whenever the track changes, so the caller can tell a new song
		// from an update of the same one.
		uint64_t m_Revision = 0;

		bool operator==(const CTrack &Other) const
		{
			return m_Title == Other.m_Title && m_Artist == Other.m_Artist && m_Playing == Other.m_Playing;
		}
	};

	// Latest snapshot, empty title when nothing is playing.
	CTrack Track() const;

	// Sent to whichever player owns the session. They are queued and run on the
	// worker, so the caller never blocks.
	enum class ECommand
	{
		PLAY_PAUSE,
		NEXT,
		PREVIOUS,
	};
	void SendCommand(ECommand Command);

	// Album art of the current track as RGBA. Returns false when there is no new
	// artwork since the last call, a size of 0 when the new track has none.
	bool TakeArtwork(std::vector<uint8_t> &vRgba, int &Width, int &Height);

private:
	class CImpl;
	std::unique_ptr<CImpl> m_pImpl;
};

#endif
