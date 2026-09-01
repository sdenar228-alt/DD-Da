// Rich presence over Discord's own socket, rather than through the Game SDK.
//
// The SDK cannot show buttons: its activity structure has no field for them, and
// no version of it ever has. The buttons people mean when they say "a button on
// the profile" belong to the protocol Discord speaks over its own socket, which
// takes the activity as JSON and does accept up to two of them.
//
// So this is a second implementation of the same interface. It needs no library
// beside it, which is also why it is worth having: the SDK is three and a half
// megabytes of DLL that has to travel with the client.
//
// Discord offers that socket as a named pipe on Windows and as a unix domain
// socket on the unixes, and above the transport the two are the same protocol:
// the same little endian frames, the same JSON inside them. Only connecting,
// writing and reading differ, so that much is a small transport class with one
// implementation per family, and the presence is written once on top of it.
// Platforms with no Discord client to talk to keep using the SDK.

#include <base/log.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/discord.h>
#include <engine/serverbrowser.h>

// Discord ships a desktop client for Windows and for the desktop unixes, and
// nowhere else. Android and the web build have nothing at the other end of a
// socket, so they are left out here and fall back to the SDK like any other
// platform this file does not cover.
#if defined(CONF_FAMILY_WINDOWS)
#define DISCORD_IPC_NAMED_PIPE 1
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_EMSCRIPTEN)
#define DISCORD_IPC_UNIX_SOCKET 1
#endif

#if defined(DISCORD_IPC_NAMED_PIPE) || defined(DISCORD_IPC_UNIX_SOCKET)

#include <cstdio>

#if defined(DISCORD_IPC_NAMED_PIPE)
#include <base/windows.h>

#include <windows.h>
#else
#include <base/fs.h>
#include <base/mem.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#endif

namespace {

// The frame that wraps every message on the socket: an operation and a length,
// both little endian, then the payload.
enum class EOpcode : uint32_t
{
	HANDSHAKE = 0,
	FRAME = 1,
	CLOSE = 2,
	PING = 3,
	PONG = 4,
};

// Discord will not take more than a handful of updates in a short while, so the
// activity is only sent when it has changed, and never faster than this.
constexpr int64_t MIN_UPDATE_INTERVAL_MS = 2000;

// Discord numbers its sockets, and which one answers depends on how many copies
// of it are running.
constexpr int NUM_SOCKETS = 10;

// What goes on the profile under the picture. Two are allowed at most.
struct SButton
{
	const char *m_pLabel;
	const char *m_pUrl;
};

// The activity has to say which process it belongs to, and that is the one thing
// in the payload the two families spell differently.
unsigned long CurrentProcessId()
{
#if defined(DISCORD_IPC_NAMED_PIPE)
	return (unsigned long)GetCurrentProcessId();
#else
	return (unsigned long)getpid();
#endif
}

#if defined(DISCORD_IPC_NAMED_PIPE)

// The Windows half of the transport: Discord listens on a named pipe, which
// reads and writes like a file.
class CIpcSocket
{
	HANDLE m_Pipe = INVALID_HANDLE_VALUE;

public:
	bool Open()
	{
		for(int i = 0; i < NUM_SOCKETS; ++i)
		{
			char aPath[64];
			str_format(aPath, sizeof(aPath), "\\\\.\\pipe\\discord-ipc-%d", i);
			m_Pipe = CreateFileA(aPath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if(m_Pipe != INVALID_HANDLE_VALUE)
				return true;
		}
		return false;
	}

	bool Write(const void *pData, size_t Size)
	{
		DWORD Written = 0;
		return WriteFile(m_Pipe, pData, (DWORD)Size, &Written, nullptr) && Written == (DWORD)Size;
	}

	// Asking what is there before reading is what keeps this from blocking: the
	// pipe is opened for blocking reads, so a read with nothing behind it would
	// wait for Discord to say something.
	bool Drain()
	{
		while(true)
		{
			DWORD Available = 0;
			if(!PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &Available, nullptr))
				return false;
			if(Available == 0)
				return true;
			char aBuffer[2048];
			DWORD Read = 0;
			if(!ReadFile(m_Pipe, aBuffer, sizeof(aBuffer), &Read, nullptr) || Read == 0)
				return false;
		}
	}

	void Close()
	{
		if(m_Pipe != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_Pipe);
			m_Pipe = INVALID_HANDLE_VALUE;
		}
	}
};

#else

// The unix half of the transport: Discord listens on a unix domain socket, put
// in the session's runtime or temporary directory.
class CIpcSocket
{
	int m_Fd = -1;

	// Linux has no SO_NOSIGPIPE and wants the flag on every send instead, so
	// whichever of the two the system has is the one that gets used.
#if defined(MSG_NOSIGNAL)
	static constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
	static constexpr int SEND_FLAGS = 0;
#endif

	bool OpenSocket(const char *pDirectory, int Index)
	{
		sockaddr_un Address;
		mem_zero(&Address, sizeof(Address));
		Address.sun_family = AF_UNIX;

		// The address carries the path in a fixed and rather short array, 104
		// bytes on macOS. The per-user directory macOS hands out is long, so a
		// path that does not fit is a real possibility, and it has to be passed
		// over rather than truncated onto the name of something else.
		char aPath[256];
		str_format(aPath, sizeof(aPath), "%s/discord-ipc-%d", pDirectory, Index);
		if((size_t)str_length(aPath) >= sizeof(Address.sun_path))
			return false;
		str_copy(Address.sun_path, aPath);

		const int Fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if(Fd < 0)
			return false;
#if defined(SO_NOSIGPIPE)
		// Writing to a socket whose peer has gone away raises SIGPIPE, which
		// ends the process by default. Discord being closed in the middle of a
		// game must not take the client with it.
		const int NoSigPipe = 1;
		setsockopt(Fd, SOL_SOCKET, SO_NOSIGPIPE, &NoSigPipe, sizeof(NoSigPipe));
#endif
		// Connecting to a unix socket either answers or fails at once, so this
		// costs nothing when Discord is not running.
		if(connect(Fd, (const sockaddr *)&Address, sizeof(Address)) < 0)
		{
			close(Fd);
			return false;
		}
		m_Fd = Fd;
		return true;
	}

	bool OpenDirectory(const char *pDirectory)
	{
		if(pDirectory == nullptr || pDirectory[0] == '\0' || !fs_is_dir(pDirectory))
			return false;
		for(int i = 0; i < NUM_SOCKETS; ++i)
		{
			if(OpenSocket(pDirectory, i))
				return true;
		}
		return false;
	}

public:
	bool Open()
	{
		// Where the socket lives depends on what the session has: the runtime
		// directory when there is one, which is the usual case on Linux, and
		// the temporary directory otherwise. On macOS that is TMPDIR, a private
		// directory of the user's own rather than the shared /tmp.
		static const char *const s_apDirectoryVariables[] = {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"};
		for(const char *pVariable : s_apDirectoryVariables)
		{
			if(OpenDirectory(getenv(pVariable)))
				return true;
		}
		return OpenDirectory("/tmp");
	}

	bool Write(const void *pData, size_t Size)
	{
		const char *pCursor = (const char *)pData;
		size_t Left = Size;
		while(Left > 0)
		{
			// A socket takes as much as it has room for and a signal can cut a
			// write short, so neither a short write nor an interruption is a
			// failure: what is left of the frame still has to go out behind it.
			const ssize_t Written = send(m_Fd, pCursor, Left, SEND_FLAGS);
			if(Written <= 0)
			{
				if(Written < 0 && errno == EINTR)
					continue;
				return false;
			}
			pCursor += Written;
			Left -= (size_t)Written;
		}
		return true;
	}

	// Reading without waiting is what PeekNamedPipe does for the pipe. Asking
	// for it per read rather than putting the socket in non-blocking mode keeps
	// the writes above simple, since those may block as long as they like.
	bool Drain()
	{
		while(true)
		{
			char aBuffer[2048];
			const ssize_t Read = recv(m_Fd, aBuffer, sizeof(aBuffer), MSG_DONTWAIT);
			if(Read > 0)
				continue;
			// An open socket with nothing to say answers with an error below,
			// so an orderly zero is Discord having closed its end.
			if(Read == 0)
				return false;
			if(errno == EINTR)
				continue;
			// Which is to say: nothing pending, which is the normal answer.
			if(errno == EAGAIN)
				return true;
#if EWOULDBLOCK != EAGAIN
			if(errno == EWOULDBLOCK)
				return true;
#endif
			return false;
		}
	}

	void Close()
	{
		if(m_Fd >= 0)
		{
			close(m_Fd);
			m_Fd = -1;
		}
	}
};

#endif

// JSON has five characters that cannot be written as they are. The rest of what
// goes through here is server and map names, which are arbitrary bytes from the
// network, so this has to be right rather than nearly right.
void JsonEscape(char *pOut, size_t OutSize, const char *pIn)
{
	size_t Written = 0;
	for(const char *pCur = pIn; *pCur != '\0' && Written + 7 < OutSize; ++pCur)
	{
		const unsigned char Char = (unsigned char)*pCur;
		const char *pEscape = nullptr;
		switch(Char)
		{
		case '"': pEscape = "\\\""; break;
		case '\\': pEscape = "\\\\"; break;
		case '\n': pEscape = "\\n"; break;
		case '\r': pEscape = "\\r"; break;
		case '\t': pEscape = "\\t"; break;
		default: break;
		}
		if(pEscape != nullptr)
		{
			pOut[Written++] = pEscape[0];
			pOut[Written++] = pEscape[1];
			continue;
		}
		if(Char < 0x20)
		{
			// Control characters have to go as an escape, not as themselves.
			Written += (size_t)snprintf(pOut + Written, OutSize - Written, "\\u%04x", Char);
			continue;
		}
		pOut[Written++] = (char)Char;
	}
	pOut[Written] = '\0';
}

} // namespace

class CDiscordIpc : public IDiscord
{
	CIpcSocket m_Socket;
	bool m_Connected = false;
	int64_t m_AppId = 0;
	char m_aAsset[64] = {};

	// What the profile should say. Kept here and sent when it changes, because
	// the game calls in far more often than Discord will listen.
	char m_aDetails[128] = {};
	char m_aState[128] = {};
	int m_Players = 0;
	int m_MaxPlayers = 0;
	int64_t m_StartTimestamp = 0;
	bool m_Dirty = false;
	int64_t m_LastSent = 0;
	int m_Nonce = 0;

	static int64_t NowMs()
	{
		return time_get() * 1000 / time_freq();
	}

	bool Send(EOpcode Opcode, const char *pPayload)
	{
		if(!m_Connected)
			return false;
		const uint32_t Length = (uint32_t)str_length(pPayload);
		uint32_t aHeader[2] = {(uint32_t)Opcode, Length};
		if(!m_Socket.Write(aHeader, sizeof(aHeader)))
		{
			Disconnect();
			return false;
		}
		if(Length > 0 && !m_Socket.Write(pPayload, Length))
		{
			Disconnect();
			return false;
		}
		return true;
	}

	// Discord answers every frame. Nothing here needs the answers, but they have
	// to be taken off the socket or it fills up and the next write blocks.
	void DrainReplies()
	{
		if(!m_Connected)
			return;
		if(!m_Socket.Drain())
			Disconnect();
	}

	void Disconnect()
	{
		m_Socket.Close();
		m_Connected = false;
	}

	void SendActivity()
	{
		char aDetails[256];
		char aState[256];
		char aAsset[128];
		JsonEscape(aDetails, sizeof(aDetails), m_aDetails);
		JsonEscape(aState, sizeof(aState), m_aState);
		JsonEscape(aAsset, sizeof(aAsset), m_aAsset);

		char aParty[128] = "";
		if(m_MaxPlayers > 0)
		{
			str_format(aParty, sizeof(aParty), ",\"party\":{\"id\":\"leviathan\",\"size\":[%d,%d]}",
				m_Players, m_MaxPlayers);
		}

		// The buttons are the whole reason this exists. Discord allows two.
		static const SButton s_aButtons[] = {
			{"Telegram", "https://t.me/leviathanddnet"},
		};
		char aButtons[512] = ",\"buttons\":[";
		for(size_t i = 0; i < std::size(s_aButtons); ++i)
		{
			char aOne[256];
			str_format(aOne, sizeof(aOne), "%s{\"label\":\"%s\",\"url\":\"%s\"}",
				i == 0 ? "" : ",", s_aButtons[i].m_pLabel, s_aButtons[i].m_pUrl);
			str_append(aButtons, aOne);
		}
		str_append(aButtons, "]");

		char aPayload[2048];
		str_format(aPayload, sizeof(aPayload),
			"{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%d\",\"args\":{\"pid\":%lu,\"activity\":{"
			"\"details\":\"%s\",\"state\":\"%s\","
			"\"timestamps\":{\"start\":%lld},"
			"\"assets\":{\"large_image\":\"%s\",\"large_text\":\"Leviathan\"}"
			"%s%s}}}",
			++m_Nonce, CurrentProcessId(),
			aDetails, aState, (long long)m_StartTimestamp, aAsset, aParty, aButtons);

		Send(EOpcode::FRAME, aPayload);
		m_LastSent = NowMs();
		m_Dirty = false;
	}

public:
	void Start(int64_t AppId, const char *pAssetName) override
	{
		m_AppId = AppId;
		str_copy(m_aAsset, pAssetName == nullptr || pAssetName[0] == '\0' ? "leviathan_logo" : pAssetName);
		m_StartTimestamp = time_timestamp();

		m_Connected = m_Socket.Open();
		if(!m_Connected)
		{
			log_info("discord", "rich presence is not running: Discord is not listening");
			return;
		}

		char aHandshake[128];
		str_format(aHandshake, sizeof(aHandshake), "{\"v\":1,\"client_id\":\"%lld\"}", (long long)m_AppId);
		if(!Send(EOpcode::HANDSHAKE, aHandshake))
		{
			log_info("discord", "rich presence is not running: Discord refused the handshake");
			return;
		}
		log_info("discord", "rich presence running under application %lld, over the socket", (long long)m_AppId);
		ClearGameInfo();
	}

	void Update() override
	{
		if(!m_Connected)
			return;
		DrainReplies();
		if(m_Dirty && NowMs() - m_LastSent >= MIN_UPDATE_INTERVAL_MS)
			SendActivity();
	}

	void ClearGameInfo() override
	{
		str_copy(m_aDetails, "In the menus");
		m_aState[0] = '\0';
		m_Players = 0;
		m_MaxPlayers = 0;
		m_Dirty = true;
	}

	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override
	{
		(void)Registered;
		str_copy(m_aDetails, ServerInfo.m_aName);
		str_copy(m_aState, ServerInfo.m_aMap);
		m_Players = ServerInfo.m_NumClients;
		m_MaxPlayers = ServerInfo.m_MaxClients;
		m_Dirty = true;
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo) override
	{
		SetGameInfo(ServerInfo, false);
	}

	void UpdatePlayerCount(int Count) override
	{
		if(Count == m_Players)
			return;
		m_Players = Count;
		m_Dirty = true;
	}

	~CDiscordIpc() override
	{
		Disconnect();
	}
};

IDiscord *CreateDiscordIpc()
{
	return new CDiscordIpc();
}

#else

IDiscord *CreateDiscordIpc()
{
	return nullptr;
}

#endif
