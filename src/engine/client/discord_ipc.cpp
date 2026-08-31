// Rich presence over Discord's own socket, rather than through the Game SDK.
//
// The SDK cannot show buttons: its activity structure has no field for them, and
// no version of it ever has. The buttons people mean when they say "a button on
// the profile" belong to the protocol Discord speaks over a named pipe, which
// takes the activity as JSON and does accept up to two of them.
//
// So this is a second implementation of the same interface. It needs no library
// beside it, which is also why it is worth having: the SDK is three and a half
// megabytes of DLL that has to travel with the client.
//
// Windows only, because that is where the client is shipped. Everything else
// keeps using the SDK.

#include <base/log.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/discord.h>
#include <engine/serverbrowser.h>

#if defined(CONF_FAMILY_WINDOWS)

#include <base/windows.h>

#include <windows.h>

#include <cstdio>

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

// What goes on the profile under the picture. Two are allowed at most.
struct SButton
{
	const char *m_pLabel;
	const char *m_pUrl;
};

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
	HANDLE m_Pipe = INVALID_HANDLE_VALUE;
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
		DWORD Written = 0;
		if(!WriteFile(m_Pipe, aHeader, sizeof(aHeader), &Written, nullptr) || Written != sizeof(aHeader))
		{
			Disconnect();
			return false;
		}
		if(Length > 0 && (!WriteFile(m_Pipe, pPayload, Length, &Written, nullptr) || Written != Length))
		{
			Disconnect();
			return false;
		}
		return true;
	}

	// Discord answers every frame. Nothing here needs the answers, but they have
	// to be taken off the pipe or it fills up and the next write blocks.
	void DrainReplies()
	{
		if(!m_Connected)
			return;
		while(true)
		{
			DWORD Available = 0;
			if(!PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &Available, nullptr))
			{
				Disconnect();
				return;
			}
			if(Available == 0)
				return;
			char aBuffer[2048];
			DWORD Read = 0;
			if(!ReadFile(m_Pipe, aBuffer, sizeof(aBuffer), &Read, nullptr) || Read == 0)
			{
				Disconnect();
				return;
			}
		}
	}

	void Disconnect()
	{
		if(m_Pipe != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_Pipe);
			m_Pipe = INVALID_HANDLE_VALUE;
		}
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
			++m_Nonce, (unsigned long)GetCurrentProcessId(),
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

		// Discord numbers its sockets, and which one answers depends on how many
		// copies of it are running.
		for(int i = 0; i < 10 && !m_Connected; ++i)
		{
			char aPath[64];
			str_format(aPath, sizeof(aPath), "\\\\.\\pipe\\discord-ipc-%d", i);
			m_Pipe = CreateFileA(aPath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if(m_Pipe != INVALID_HANDLE_VALUE)
				m_Connected = true;
		}
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
