#include "custom_music.h"

#if defined(CONF_PLATFORM_MACOS)

#include <base/log.h>

#import <Cocoa/Cocoa.h>

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// macOS publishes no counterpart to the Windows media session, so this reads
// two sources in turn.
//
// The first is MediaRemote, the private framework the media keys and Control
// Centre go through. It is the only one that sees every player, browsers
// included, and it is the only one that hands the cover art over as bytes.
// macOS 15.4 put its now playing information behind an entitlement that only
// Apple's own binaries carry, so on those releases it answers questions with
// nothing at all. It still accepts transport commands there, which is why the
// two halves are kept apart below.
//
// The second is AppleScript against the two players worth scripting. It only
// sees those two, it costs the user an automation prompt, and it must never be
// the thing that starts a player, so it is only reached for when MediaRemote
// has gone silent.

namespace {
// The artwork is only used as a small square, no need to keep it large.
constexpr uint32_t ARTWORK_SIZE = 128;

// MRMediaRemoteSendCommand's command numbers. The enum behind them is private
// and has never been published, but these three have held the same values
// since the framework first appeared and are what a media key sends.
constexpr int MEDIA_REMOTE_TOGGLE_PLAY_PAUSE = 2;
constexpr int MEDIA_REMOTE_NEXT_TRACK = 4;
constexpr int MEDIA_REMOTE_PREVIOUS_TRACK = 5;

// Where a poll's answer came from. Kept across polls so that a change of
// source can be logged, and so that a command knows which player to fall back
// to when MediaRemote will not carry it.
enum class ESource
{
	NONE,
	MEDIA_REMOTE,
	SPOTIFY,
	MUSIC,
};

const char *SourceName(ESource Source)
{
	switch(Source)
	{
	case ESource::MEDIA_REMOTE: return "the system media remote";
	case ESource::SPOTIFY: return "Spotify over AppleScript";
	case ESource::MUSIC: return "Music over AppleScript";
	case ESource::NONE: break;
	}
	return "nothing";
}

class CScriptedPlayer
{
public:
	ESource m_Source;
	// Addressed by identifier rather than by name, because the name is
	// localised and a name AppleScript cannot resolve opens a file picker.
	const char *m_pBundleId;
	// Spotify reports the track length in milliseconds, Music in seconds.
	double m_DurationScale;
	// Music hands the cover out as the bytes it was stored as. Spotify offers
	// nothing but an https URL, and fetching one behind the player's back is
	// not something a game client should be doing.
	bool m_HasArtwork;
};

constexpr CScriptedPlayer SCRIPTED_PLAYERS[] = {
	{ESource::SPOTIFY, "com.spotify.client", 1.0 / 1000.0, false},
	{ESource::MUSIC, "com.apple.Music", 1.0, true},
};
constexpr size_t NUM_SCRIPTED_PLAYERS = sizeof(SCRIPTED_PLAYERS) / sizeof(SCRIPTED_PLAYERS[0]);

const CScriptedPlayer *PlayerForSource(ESource Source)
{
	for(const CScriptedPlayer &Player : SCRIPTED_PLAYERS)
	{
		if(Player.m_Source == Source)
			return &Player;
	}
	return nullptr;
}

// The keys of MediaRemote's now playing dictionary, looked up by name for the
// same reason the functions are.
enum
{
	KEY_TITLE = 0,
	KEY_ARTIST,
	KEY_DURATION,
	KEY_ELAPSED,
	KEY_TIMESTAMP,
	KEY_RATE,
	KEY_ARTWORK,
	NUM_KEYS,
};

const char *const KEY_NAMES[NUM_KEYS] = {
	"kMRMediaRemoteNowPlayingInfoTitle",
	"kMRMediaRemoteNowPlayingInfoArtist",
	"kMRMediaRemoteNowPlayingInfoDuration",
	"kMRMediaRemoteNowPlayingInfoElapsedTime",
	"kMRMediaRemoteNowPlayingInfoTimestamp",
	"kMRMediaRemoteNowPlayingInfoPlaybackRate",
	"kMRMediaRemoteNowPlayingInfoArtworkData",
};

typedef void (*FMediaRemoteGetNowPlayingInfo)(dispatch_queue_t Queue, void (^Handler)(CFDictionaryRef pInfo));
typedef void (*FMediaRemoteGetNowPlayingApplicationIsPlaying)(dispatch_queue_t Queue, void (^Handler)(Boolean Playing));
typedef Boolean (*FMediaRemoteSendCommand)(int Command, CFDictionaryRef pUserInfo);

std::string ToUtf8(NSString *pString)
{
	if(pString == nil)
		return std::string();
	const char *pUtf8 = [pString UTF8String];
	if(pUtf8 == nullptr)
		return std::string();
	return std::string(pUtf8);
}

// Only asks about processes that already exist. Addressing an app over
// AppleScript is what would launch it, and a now playing display has no
// business starting Spotify by itself.
bool IsRunning(const char *pBundleId)
{
	NSString *pIdentifier = [NSString stringWithUTF8String:pBundleId];
	if(pIdentifier == nil)
		return false;
	return [[NSRunningApplication runningApplicationsWithBundleIdentifier:pIdentifier] count] > 0;
}

// MediaRemote answers on a queue of the caller's choosing, so every question
// turns into a wait. The block that carries the answer can still run after the
// wait has given up on it, so what it writes into is shared and outlives
// whichever of the two lets go of it last.
class CReply
{
public:
	CReply() :
		m_Done(dispatch_semaphore_create(0))
	{
	}

	~CReply()
	{
		if(m_pInfo != nullptr)
			CFRelease(m_pInfo);
		if(m_Done != nullptr)
			dispatch_release(m_Done);
	}

	dispatch_semaphore_t m_Done;
	std::mutex m_Lock;
	CFDictionaryRef m_pInfo = nullptr;
	bool m_Playing = false;
};

// Half a second is already longer than the media daemon has any business
// taking, and a poll that has not been answered by then is stale anyway.
constexpr int64_t REPLY_TIMEOUT_NS = 500 * NSEC_PER_MSEC;

// The same idea for the other source, which needs saying out loud because the
// default is so much worse: an Apple event with no timeout on it waits two
// minutes for the player to answer. Shutting the client down while one of those
// is in flight means joining a worker that is sitting inside it, so a stuck or
// busy player would hold the whole client open. Every script this file sends is
// wrapped in one of these.
constexpr int SCRIPT_TIMEOUT_SECONDS = 2;

class CMediaRemote
{
public:
	~CMediaRemote()
	{
		for(int i = 0; i < NUM_KEYS; ++i)
		{
			if(m_apKeys[i] != nullptr)
				CFRelease(m_apKeys[i]);
		}
	}

	void Open();

	// Whether the framework is on this system at all. It answering is a
	// separate question, and on macOS 15.4 and later the answer is no.
	bool Available() const { return m_pfnGetNowPlayingInfo != nullptr; }
	CFStringRef Key(int Index) const { return m_apKeys[Index]; }

	// The now playing dictionary, or nullptr when the framework is missing or
	// the daemon refuses us. An empty dictionary means it does talk to us and
	// nothing is playing, which is a different thing entirely and is what
	// keeps the AppleScript path from being reached on a healthy system.
	CFDictionaryRef CopyNowPlayingInfo() const;
	bool IsPlaying(bool Fallback) const;
	bool SendCommand(int Command) const;

private:
	FMediaRemoteGetNowPlayingInfo m_pfnGetNowPlayingInfo = nullptr;
	FMediaRemoteGetNowPlayingApplicationIsPlaying m_pfnGetIsPlaying = nullptr;
	FMediaRemoteSendCommand m_pfnSendCommand = nullptr;
	CFStringRef m_apKeys[NUM_KEYS] = {};
};

// The dictionary keys are exported constants rather than literals, so they are
// looked up the same way the functions are. Their string values happen to be
// their own symbol names, which is what the last resort here leans on. Either
// way the result is owned, so that there is exactly one release for it.
CFStringRef RetainKey(void *pHandle, const char *pName)
{
	CFStringRef *ppKey = (CFStringRef *)dlsym(pHandle, pName);
	if(ppKey != nullptr && *ppKey != nullptr)
		return (CFStringRef)CFRetain(*ppKey);
	return CFStringCreateWithCString(kCFAllocatorDefault, pName, kCFStringEncodingUTF8);
}

void CMediaRemote::Open()
{
	// Opened by hand rather than linked against. This is a private framework:
	// a link time dependency on one would stop the client starting at all on a
	// system where it has been renamed, moved or taken away, which is exactly
	// the kind of thing Apple does to private frameworks.
	void *pHandle = dlopen("/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote", RTLD_LAZY | RTLD_LOCAL);
	if(pHandle == nullptr)
		return;

	// The handle is deliberately never closed. A question that timed out can
	// still have a block waiting to run inside the framework, and unmapping it
	// underneath that block would take the client down with it.
	m_pfnGetNowPlayingInfo = (FMediaRemoteGetNowPlayingInfo)dlsym(pHandle, "MRMediaRemoteGetNowPlayingInfo");
	m_pfnGetIsPlaying = (FMediaRemoteGetNowPlayingApplicationIsPlaying)dlsym(pHandle, "MRMediaRemoteGetNowPlayingApplicationIsPlaying");
	m_pfnSendCommand = (FMediaRemoteSendCommand)dlsym(pHandle, "MRMediaRemoteSendCommand");

	for(int i = 0; i < NUM_KEYS; ++i)
		m_apKeys[i] = RetainKey(pHandle, KEY_NAMES[i]);
}

CFDictionaryRef CMediaRemote::CopyNowPlayingInfo() const
{
	if(m_pfnGetNowPlayingInfo == nullptr)
		return nullptr;

	auto pReply = std::make_shared<CReply>();
	// A global queue, so that the answer never has to wait on the thread that
	// is waiting for it.
	m_pfnGetNowPlayingInfo(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(CFDictionaryRef pInfo) {
		{
			const std::lock_guard<std::mutex> Guard(pReply->m_Lock);
			// Only the first answer is kept. Nothing promises that a private
			// framework calls its block exactly once, and a second one would
			// write over a dictionary that then never gets released.
			if(pInfo != nullptr && pReply->m_pInfo == nullptr)
				pReply->m_pInfo = (CFDictionaryRef)CFRetain(pInfo);
		}
		dispatch_semaphore_signal(pReply->m_Done);
	});

	if(dispatch_semaphore_wait(pReply->m_Done, dispatch_time(DISPATCH_TIME_NOW, REPLY_TIMEOUT_NS)) != 0)
		return nullptr;

	// Handed over rather than copied, so that the reply's destructor does not
	// release it a second time.
	const std::lock_guard<std::mutex> Guard(pReply->m_Lock);
	CFDictionaryRef pInfo = pReply->m_pInfo;
	pReply->m_pInfo = nullptr;
	return pInfo;
}

bool CMediaRemote::IsPlaying(bool Fallback) const
{
	if(m_pfnGetIsPlaying == nullptr)
		return Fallback;

	auto pReply = std::make_shared<CReply>();
	m_pfnGetIsPlaying(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(Boolean Playing) {
		{
			const std::lock_guard<std::mutex> Guard(pReply->m_Lock);
			pReply->m_Playing = Playing != 0;
		}
		dispatch_semaphore_signal(pReply->m_Done);
	});

	if(dispatch_semaphore_wait(pReply->m_Done, dispatch_time(DISPATCH_TIME_NOW, REPLY_TIMEOUT_NS)) != 0)
		return Fallback;

	const std::lock_guard<std::mutex> Guard(pReply->m_Lock);
	return pReply->m_Playing;
}

bool CMediaRemote::SendCommand(int Command) const
{
	if(m_pfnSendCommand == nullptr)
		return false;
	return m_pfnSendCommand(Command, nullptr) != 0;
}

// The dictionary is Core Foundation but everything in it is a Foundation
// object, so it is read as one. Nothing in it is guaranteed to be there, and a
// player that puts the wrong kind of value under a key must not take the
// client down.
id ValueOfClass(NSDictionary *pDict, CFStringRef Key, Class ExpectedClass)
{
	if(Key == nullptr)
		return nil;
	id pValue = [pDict objectForKey:(NSString *)Key];
	if(pValue == nil || ![pValue isKindOfClass:ExpectedClass])
		return nil;
	return pValue;
}

void ReadNowPlayingInfo(const CMediaRemote &MediaRemote, CFDictionaryRef pInfo, CSystemMusic::CTrack *pTrack, NSData **ppArtwork)
{
	NSDictionary *pDict = (NSDictionary *)pInfo;

	pTrack->m_Title = ToUtf8((NSString *)ValueOfClass(pDict, MediaRemote.Key(KEY_TITLE), [NSString class]));
	pTrack->m_Artist = ToUtf8((NSString *)ValueOfClass(pDict, MediaRemote.Key(KEY_ARTIST), [NSString class]));

	NSNumber *pRate = (NSNumber *)ValueOfClass(pDict, MediaRemote.Key(KEY_RATE), [NSNumber class]);
	const double Rate = pRate != nil ? [pRate doubleValue] : 0.0;
	// The rate is the only hint some players give, and it stands in for the
	// separate question when that one cannot be asked.
	pTrack->m_Playing = MediaRemote.IsPlaying(Rate > 0.0);

	NSNumber *pDuration = (NSNumber *)ValueOfClass(pDict, MediaRemote.Key(KEY_DURATION), [NSNumber class]);
	const double Duration = pDuration != nil ? [pDuration doubleValue] : 0.0;
	if(Duration > 0.0)
	{
		pTrack->m_Duration = Duration;

		NSNumber *pElapsed = (NSNumber *)ValueOfClass(pDict, MediaRemote.Key(KEY_ELAPSED), [NSNumber class]);
		double Position = pElapsed != nil ? [pElapsed doubleValue] : 0.0;

		// Players publish the position only now and then, not continuously, so
		// the reported value has to be moved forward by however long ago it
		// was published, at whatever speed it is being played back at.
		NSDate *pTimestamp = (NSDate *)ValueOfClass(pDict, MediaRemote.Key(KEY_TIMESTAMP), [NSDate class]);
		if(pTrack->m_Playing && pTimestamp != nil)
		{
			// timeIntervalSinceNow counts backwards for a date in the past.
			const double Published = [pTimestamp timeIntervalSinceNow];
			// A stale timestamp would otherwise run the bar off the end of the
			// track.
			const double Elapsed = std::clamp(-Published, 0.0, 60.0);
			Position += Elapsed * (Rate > 0.0 ? Rate : 1.0);
		}
		pTrack->m_Position = std::clamp(Position, 0.0, pTrack->m_Duration);
	}

	NSData *pArtwork = (NSData *)ValueOfClass(pDict, MediaRemote.Key(KEY_ARTWORK), [NSData class]);
	if(pArtwork != nil && [pArtwork length] > 0)
	{
		// The dictionary is released as soon as this returns, so the bytes are
		// kept alive for the rest of the poll on their own account.
		*ppArtwork = [[pArtwork retain] autorelease];
	}
}

// Both players answer the same questions, and answering with a list rather
// than a formatted string keeps numbers as numbers, with no decimal separator
// of the user's locale to trip over. A player that is between tracks has no
// current track to ask about, hence the try.
NSString *PollScriptSource(const CScriptedPlayer &Player)
{
	return [NSString stringWithFormat:@"with timeout of %d seconds\n"
					  @"tell application id \"%s\"\n"
					  @"  try\n"
					  @"    if player state is stopped then return {\"\", \"\", 0, 0, false}\n"
					  @"    return {name of current track, artist of current track, duration of current track, player position, player state is playing}\n"
					  @"  on error\n"
					  @"    return {\"\", \"\", 0, 0, false}\n"
					  @"  end try\n"
					  @"end tell\n"
					  @"end timeout",
			 SCRIPT_TIMEOUT_SECONDS, Player.m_pBundleId];
}

bool ReadScriptedPlayer(NSAppleScript *pScript, const CScriptedPlayer &Player, CSystemMusic::CTrack *pTrack)
{
	if(pScript == nil)
		return false;

	// Asked again here, not only before the script was compiled: a player that
	// quit in between would be started right back up by the `tell`, which is the
	// one thing this must never do.
	if(!IsRunning(Player.m_pBundleId))
		return false;

	NSDictionary *pError = nil;
	NSAppleEventDescriptor *pResult = [pScript executeAndReturnError:&pError];
	// A refused automation prompt, a player on its way out, a script the
	// player's dictionary no longer understands: either way there is nothing
	// to show and the next source gets its turn.
	if(pResult == nil || [pResult numberOfItems] < 5)
		return false;

	// The list is one based, and asking an absent item for its value answers
	// nil rather than raising, so the count above is the only guard needed.
	pTrack->m_Title = ToUtf8([[pResult descriptorAtIndex:1] stringValue]);
	pTrack->m_Artist = ToUtf8([[pResult descriptorAtIndex:2] stringValue]);
	pTrack->m_Playing = [[pResult descriptorAtIndex:5] booleanValue] != 0;

	const double Duration = [[pResult descriptorAtIndex:3] doubleValue] * Player.m_DurationScale;
	if(Duration > 0.0)
	{
		pTrack->m_Duration = Duration;
		// Unlike the media remote, the player answers with where it is right
		// now, so there is nothing to carry forward.
		pTrack->m_Position = std::clamp([[pResult descriptorAtIndex:4] doubleValue], 0.0, Duration);
	}
	return true;
}

// The cover comes back inside the answer, so this is a round trip with a whole
// image in it and is only worth making when the track has actually changed.
NSData *ScriptedArtwork(const CScriptedPlayer &Player)
{
	if(!Player.m_HasArtwork || !IsRunning(Player.m_pBundleId))
		return nil;

	NSString *pSource = [NSString stringWithFormat:@"with timeout of %d seconds\n"
							  @"tell application id \"%s\"\n"
							  @"  try\n"
							  @"    if player state is stopped then return missing value\n"
							  @"    return raw data of artwork 1 of current track\n"
							  @"  on error\n"
							  @"    return missing value\n"
							  @"  end try\n"
							  @"end tell\n"
							  @"end timeout",
					   SCRIPT_TIMEOUT_SECONDS, Player.m_pBundleId];

	NSAppleScript *pScript = [[NSAppleScript alloc] initWithSource:pSource];
	NSDictionary *pError = nil;
	NSAppleEventDescriptor *pResult = [pScript executeAndReturnError:&pError];
	[pScript release];
	if(pResult == nil)
		return nil;

	// A track with no cover answers `missing value`, which arrives as a four
	// byte type code rather than as image data; the size check throws it out
	// along with anything else too small to be an image.
	NSData *pData = [pResult data];
	if(pData == nil || [pData length] < 64)
		return nil;
	return pData;
}

// The media remote keeps taking commands on the releases that stopped
// answering questions, and it reaches every player rather than only the
// scriptable two, so it is tried first whatever the island is reading from.
// False when there was nobody to carry the command, so that the caller can keep
// it for the next poll rather than swallow it. That happens on the very first
// poll after a bind is pressed, before any source has been found.
bool RunCommand(const CMediaRemote &MediaRemote, ESource Source, CSystemMusic::ECommand Command)
{
	int Code = MEDIA_REMOTE_TOGGLE_PLAY_PAUSE;
	const char *pVerb = "playpause";
	switch(Command)
	{
	case CSystemMusic::ECommand::PLAY_PAUSE:
		break;
	case CSystemMusic::ECommand::NEXT:
		Code = MEDIA_REMOTE_NEXT_TRACK;
		pVerb = "next track";
		break;
	case CSystemMusic::ECommand::PREVIOUS:
		Code = MEDIA_REMOTE_PREVIOUS_TRACK;
		pVerb = "previous track";
		break;
	}

	if(MediaRemote.SendCommand(Code))
		return true;

	const CScriptedPlayer *pPlayer = PlayerForSource(Source);
	if(pPlayer == nullptr || !IsRunning(pPlayer->m_pBundleId))
		return false;

	// The block form rather than the one line one, because `with timeout` has to
	// wrap something.
	NSString *pSource = [NSString stringWithFormat:@"with timeout of %d seconds\n"
									   @"tell application id \"%s\" to %s\n"
									   @"end timeout",
					   SCRIPT_TIMEOUT_SECONDS, pPlayer->m_pBundleId, pVerb];
	NSAppleScript *pScript = [[NSAppleScript alloc] initWithSource:pSource];
	NSDictionary *pError = nil;
	// The player may refuse, nothing to do about it. It was asked, which is all
	// this reports.
	[pScript executeAndReturnError:&pError];
	[pScript release];
	return true;
}
} // namespace

class CSystemMusic::CImpl
{
public:
	~CImpl() { Stop(); }

	void Start();
	void Stop();

	mutable std::mutex m_Lock;
	CTrack m_Track;
	// Commands from the render thread, drained by the worker.
	std::vector<CSystemMusic::ECommand> m_vPending;
	std::vector<uint8_t> m_vArtwork;
	int m_ArtworkWidth = 0;
	int m_ArtworkHeight = 0;
	bool m_ArtworkFresh = false;

private:
	std::thread m_Thread;
	std::atomic_bool m_Running{false};

	void Run();
	// Decodes whatever encoded image the player handed over into a small RGBA
	// square.
	bool ReadArtwork(NSData *pData);
	// Publishes an empty square, so the island drops the cover it still has.
	void ClearArtwork();
};

void CSystemMusic::CImpl::Start()
{
	if(m_Running.exchange(true))
		return;
	m_Thread = std::thread([this] { Run(); });
}

void CSystemMusic::CImpl::Stop()
{
	if(!m_Running.exchange(false))
		return;
	if(m_Thread.joinable())
		m_Thread.join();
}

void CSystemMusic::CImpl::ClearArtwork()
{
	const std::lock_guard<std::mutex> Guard(m_Lock);
	m_vArtwork.clear();
	m_ArtworkWidth = 0;
	m_ArtworkHeight = 0;
	m_ArtworkFresh = true;
}

bool CSystemMusic::CImpl::ReadArtwork(NSData *pData)
{
	if(pData == nil || [pData length] == 0 || [pData length] > 8 * 1024 * 1024)
		return false;

	// ImageIO decodes whatever the player stored the cover as, usually jpeg or
	// png.
	CGImageSourceRef pImageSource = CGImageSourceCreateWithData((CFDataRef)pData, nullptr);
	if(pImageSource == nullptr)
		return false;
	CGImageRef pImage = CGImageSourceCreateImageAtIndex(pImageSource, 0, nullptr);
	CFRelease(pImageSource);
	if(pImage == nullptr)
		return false;

	std::vector<uint8_t> vRgba((size_t)ARTWORK_SIZE * ARTWORK_SIZE * 4);
	CGColorSpaceRef pColorSpace = CGColorSpaceCreateDeviceRGB();
	// Core Graphics has no straight alpha layout for a bitmap context, only a
	// premultiplied one. Cover art is opaque, so the two are the same bytes.
	CGContextRef pContext = CGBitmapContextCreate(vRgba.data(), ARTWORK_SIZE, ARTWORK_SIZE, 8, ARTWORK_SIZE * 4, pColorSpace,
		(CGBitmapInfo)((uint32_t)kCGImageAlphaPremultipliedLast | (uint32_t)kCGBitmapByteOrder32Big));
	CGColorSpaceRelease(pColorSpace);
	if(pContext == nullptr)
	{
		CGImageRelease(pImage);
		return false;
	}

	CGContextSetInterpolationQuality(pContext, kCGInterpolationHigh);
	// Stretched into the square rather than fitted into it, the same as on
	// Windows: the island draws a square and covers are square to begin with.
	CGContextDrawImage(pContext, CGRectMake(0.0, 0.0, (CGFloat)ARTWORK_SIZE, (CGFloat)ARTWORK_SIZE), pImage);
	// Released before the buffer is handed on, because the context writes into
	// it for as long as it lives.
	CGContextRelease(pContext);
	CGImageRelease(pImage);

	{
		const std::lock_guard<std::mutex> Guard(m_Lock);
		m_vArtwork = std::move(vRgba);
		m_ArtworkWidth = (int)ARTWORK_SIZE;
		m_ArtworkHeight = (int)ARTWORK_SIZE;
		m_ArtworkFresh = true;
	}
	return true;
}

void CSystemMusic::CImpl::Run()
{
	@autoreleasepool
	{
		// AppleScript expects a run loop on the thread that drives it, and
		// merely asking for the current one is what creates it. Nothing else
		// here needs one, but the fallback path misbehaves without it.
		(void)[NSRunLoop currentRunLoop];

		CMediaRemote MediaRemote;
		MediaRemote.Open();

		// Compiling a script costs more than the round trip to the player
		// does, so the one each player is polled with is kept for as long as
		// the worker runs. Only the worker ever touches them.
		NSAppleScript *apPollScripts[NUM_SCRIPTED_PLAYERS] = {};

		uint64_t Revision = 0;
		std::string LastKey;
		ESource Source = ESource::NONE;

		// Which source works is settled by the machine rather than by the
		// build, so it is worth a line in the log: an island that never turns
		// up is otherwise indistinguishable from a player that publishes
		// nothing. A few polls have to agree before anything is said, so that
		// a daemon still starting up is not reported as a refusal.
		ESource LoggedSource = ESource::NONE;
		bool LoggedRefused = false;
		bool LoggedNothing = false;
		int RefusedPolls = 0;
		int SilentPolls = 0;

		while(m_Running.load())
		{
			// A poll allocates a dictionary, a descriptor and a whole cover.
			// Without a pool of its own the worker would hold on to every one
			// of them until the client shuts down.
			@autoreleasepool
			{
				// Run whatever the island asked for before reading the state
				// back, so the display reflects it right away. The source of
				// the previous poll is the one that knows which player to talk
				// to if the media remote will not carry the command.
				std::vector<CSystemMusic::ECommand> vCommands;
				{
					const std::lock_guard<std::mutex> Guard(m_Lock);
					vCommands.swap(m_vPending);
				}
				std::vector<CSystemMusic::ECommand> vUnhandled;
				for(const CSystemMusic::ECommand Command : vCommands)
				{
					if(!m_Running.load())
						break;
					if(!RunCommand(MediaRemote, Source, Command))
						vUnhandled.push_back(Command);
				}
				// A press that arrived before any source had been found would
				// otherwise be drained and dropped, which is exactly what a bind
				// pressed right after startup does. Put it back instead, still
				// one of each.
				if(!vUnhandled.empty())
				{
					const std::lock_guard<std::mutex> Guard(m_Lock);
					for(const CSystemMusic::ECommand Command : vUnhandled)
					{
						if(std::find(m_vPending.begin(), m_vPending.end(), Command) == m_vPending.end())
							m_vPending.push_back(Command);
					}
				}

				CTrack Track;
				const CScriptedPlayer *pPlayer = nullptr;
				NSData *pArtwork = nil;
				Source = ESource::NONE;

				CFDictionaryRef pInfo = MediaRemote.CopyNowPlayingInfo();
				if(pInfo != nullptr)
				{
					Source = ESource::MEDIA_REMOTE;
					ReadNowPlayingInfo(MediaRemote, pInfo, &Track, &pArtwork);
					CFRelease(pInfo);
				}
				else
				{
					// Reached only when the media remote is gone or refuses
					// us, never merely because it reports nothing playing:
					// addressing a player is what raises the automation
					// prompt, and there is no reason to put that in front of
					// someone whose media remote answers perfectly well.
					for(size_t i = 0; i < NUM_SCRIPTED_PLAYERS; ++i)
					{
						if(!m_Running.load())
							break;
						if(!IsRunning(SCRIPTED_PLAYERS[i].m_pBundleId))
							continue;
						if(apPollScripts[i] == nil)
							apPollScripts[i] = [[NSAppleScript alloc] initWithSource:PollScriptSource(SCRIPTED_PLAYERS[i])];
						if(!ReadScriptedPlayer(apPollScripts[i], SCRIPTED_PLAYERS[i], &Track))
							continue;
						Source = SCRIPTED_PLAYERS[i].m_Source;
						pPlayer = &SCRIPTED_PLAYERS[i];
						// A player that is running but idle still counts as
						// the source, so that its transport buttons keep
						// working; one that is actually playing ends the
						// search.
						if(!Track.m_Title.empty())
							break;
					}
				}

				if(Source == ESource::NONE)
				{
					LastKey.clear();
				}
				else
				{
					const std::string Key = Track.m_Title + "\x1f" + Track.m_Artist;
					if(Key != LastKey)
					{
						LastKey = Key;
						++Revision;
						if(pArtwork == nil && pPlayer != nullptr && m_Running.load())
							pArtwork = ScriptedArtwork(*pPlayer);
						// A track whose source publishes no art has to say so,
						// otherwise the cover of the previous one stays up.
						if(!ReadArtwork(pArtwork))
							ClearArtwork();
					}
				}
				Track.m_Revision = Revision;

				{
					const std::lock_guard<std::mutex> Guard(m_Lock);
					m_Track = Track;
				}

				if(Source != ESource::NONE && Source != LoggedSource)
				{
					LoggedSource = Source;
					log_info("musicisland", "Reading now playing from %s", SourceName(Source));
				}

				// The framework is there but the daemon will not talk to us:
				// macOS 15.4 put the now playing information behind an
				// entitlement that only Apple's own binaries carry. Worth
				// saying, because what is left sees far fewer players.
				RefusedPolls = MediaRemote.Available() && Source != ESource::MEDIA_REMOTE ? RefusedPolls + 1 : 0;
				if(!LoggedRefused && RefusedPolls >= 4)
				{
					LoggedRefused = true;
					log_info("musicisland", "The system media remote refuses the now playing information, falling back to AppleScript");
				}

				SilentPolls = Source == ESource::NONE ? SilentPolls + 1 : 0;
				if(!LoggedNothing && SilentPolls >= 4)
				{
					LoggedNothing = true;
					log_info("musicisland", "No now playing source is available, the island stays hidden");
				}
			}

			// Polling twice a second is plenty for a now playing display.
			for(int i = 0; i < 5 && m_Running.load(); ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		for(NSAppleScript *pScript : apPollScripts)
			[pScript release];
	}
}

CSystemMusic::CSystemMusic() :
	m_pImpl(std::make_unique<CImpl>())
{
}

CSystemMusic::~CSystemMusic() = default;

void CSystemMusic::SendCommand(ECommand Command)
{
	const std::lock_guard<std::mutex> Guard(m_pImpl->m_Lock);
	// One of each is enough, a burst of clicks should not queue up.
	if(std::find(m_pImpl->m_vPending.begin(), m_pImpl->m_vPending.end(), Command) == m_pImpl->m_vPending.end())
		m_pImpl->m_vPending.push_back(Command);
}

void CSystemMusic::Start() { m_pImpl->Start(); }
void CSystemMusic::Stop() { m_pImpl->Stop(); }

CSystemMusic::CTrack CSystemMusic::Track() const
{
	const std::lock_guard<std::mutex> Guard(m_pImpl->m_Lock);
	return m_pImpl->m_Track;
}

bool CSystemMusic::TakeArtwork(std::vector<uint8_t> &vRgba, int &Width, int &Height)
{
	const std::lock_guard<std::mutex> Guard(m_pImpl->m_Lock);
	if(!m_pImpl->m_ArtworkFresh)
		return false;
	m_pImpl->m_ArtworkFresh = false;
	vRgba = m_pImpl->m_vArtwork;
	Width = m_pImpl->m_ArtworkWidth;
	Height = m_pImpl->m_ArtworkHeight;
	return true;
}

#endif
