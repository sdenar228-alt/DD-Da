#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/discord.h>

#if defined(CONF_DISCORD)
#include <discord_game_sdk.h>

typedef enum EDiscordResult(DISCORD_API *FDiscordCreate)(DiscordVersion, struct DiscordCreateParams *, struct IDiscordCore **);

#if defined(CONF_DISCORD_DYNAMIC)
#include <dlfcn.h>
static FDiscordCreate GetDiscordCreate()
{
	void *pSdk = dlopen("discord_game_sdk.so", RTLD_NOW);
	if(!pSdk)
	{
		return nullptr;
	}
	return (FDiscordCreate)dlsym(pSdk, "DiscordCreate");
}
#else
static FDiscordCreate GetDiscordCreate()
{
	return DiscordCreate;
}
#endif

class CDiscord : public IDiscord
{
	DiscordActivity m_Activity;
	bool m_UpdateActivity = false;
	int64_t m_LastActivityUpdate = 0;

	IDiscordCore *m_pCore;
	IDiscordActivityEvents m_ActivityEvents;
	IDiscordActivityManager *m_pActivityManager;

public:
	int64_t m_AppId = 0;
	FDiscordCreate m_pfnDiscordCreate = nullptr;
	bool m_Started = false;
	char m_aAssetName[64] = {};

	void SetCreateFunction(FDiscordCreate pfnDiscordCreate) { m_pfnDiscordCreate = pfnDiscordCreate; }

	// Which image Discord shows. It is looked up by name inside the application,
	// so under our own one it is whatever the setting says, and under DDNet's it
	// can only be theirs.
	const char *AssetName() const
	{
		if(m_AppId <= 0)
			return "ddnet_logo";
		return m_aAssetName[0] == 0 ? "leviathan_logo" : m_aAssetName;
	}

	bool Connect(int64_t AppId)
	{
		m_AppId = AppId;
		m_pCore = nullptr;
		mem_zero(&m_ActivityEvents, sizeof(m_ActivityEvents));

		m_ActivityEvents.on_activity_join = &CDiscord::OnActivityJoin;
		m_pActivityManager = nullptr;

		DiscordCreateParams Params;
		DiscordCreateParamsSetDefault(&Params);

		// Whose Discord application the presence belongs to. The name and the
		// artwork Discord shows belong to the application, not to us, so under the
		// default one it says DDNet. Make an application of your own at
		// discord.com/developers, upload an image asset named leviathan_logo, and
		// put its id in cl_discord_app_id to have it say Leviathan instead.
		Params.client_id = AppId > 0 ? AppId : 752165779117441075;
		Params.flags = EDiscordCreateFlags::DiscordCreateFlags_NoRequireDiscord;
		Params.event_data = this;
		Params.activity_events = &m_ActivityEvents;

		int Error = m_pfnDiscordCreate(DISCORD_VERSION, &Params, &m_pCore);
		if(Error != DiscordResult_Ok)
		{
			dbg_msg("discord", "error initializing discord instance, error=%d", Error);
			return true;
		}

		m_pActivityManager = m_pCore->get_activity_manager(m_pCore);

		// which application to launch when joining activity
		m_pActivityManager->register_command(m_pActivityManager, CONNECTLINK_DOUBLE_SLASH);
		m_pActivityManager->register_steam(m_pActivityManager, 412220); // steam id

		ClearGameInfo();

		return false;
	}

	void Start(int64_t AppId, const char *pAssetName) override
	{
		if(m_Started)
			return;
		m_Started = true;
		str_copy(m_aAssetName, pAssetName == nullptr ? "" : pAssetName);
		if(Connect(AppId))
		{
			// Connect returns true on failure, matching how it was called before.
			log_info("discord", "rich presence could not start");
			m_pCore = nullptr;
			return;
		}
		if(AppId > 0)
			log_info("discord", "rich presence running under application %lld", (long long)AppId);
		else
			log_info("discord", "rich presence running under the default application, set cl_discord_app_id for your own");
	}

	void Update() override
	{
		if(m_pCore == nullptr)
			return;
		// update every 5 seconds, rate limit is 5 updates per 20 seconds
		if(m_UpdateActivity && time_get() > m_LastActivityUpdate + time_freq() * 5)
		{
			m_UpdateActivity = false;
			m_LastActivityUpdate = time_get();

			m_pActivityManager->update_activity(m_pActivityManager, &m_Activity, nullptr, nullptr);
		}

		m_pCore->run_callbacks(m_pCore);
	}

	void ClearGameInfo() override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, AssetName());
		str_copy(m_Activity.assets.large_text, "Leviathan");
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.details, "In the menus");
		m_Activity.instance = false;

		m_UpdateActivity = true;
	}

	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, AssetName());
		str_copy(m_Activity.assets.large_text, "Leviathan");
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.name, "Leviathan");
		m_Activity.instance = true;

		str_copy(m_Activity.details, ServerInfo.m_aName);
		str_copy(m_Activity.state, ServerInfo.m_aMap);
		m_Activity.party.size.current_size = ServerInfo.m_NumClients;
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		// private makes it so the game isn't public to join, but there's 'Ask to Join' button instead
		m_Activity.party.privacy = Registered ? DiscordActivityPartyPrivacy_Public : DiscordActivityPartyPrivacy_Private;

		if(!Registered)
		{
			// private parties have random id to not leak the server ip
			char aPartyId[sizeof(m_Activity.party.id)];
			secure_random_password(aPartyId, sizeof(aPartyId), 64);
			str_copy(m_Activity.party.id, aPartyId);
		}
		UpdateServerIp(ServerInfo);

		m_UpdateActivity = true;
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo) override
	{
		if(!m_Activity.instance)
			return;

		UpdateServerIp(ServerInfo);

		str_copy(m_Activity.details, ServerInfo.m_aName);
		str_copy(m_Activity.state, ServerInfo.m_aMap);
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		m_UpdateActivity = true;
	}

	void UpdatePlayerCount(int Count) override
	{
		if(!m_Activity.instance)
			return;

		if(m_Activity.party.size.current_size == Count)
			return;

		m_Activity.party.size.current_size = Count;
		m_UpdateActivity = true;
	}

	void UpdateServerIp(const CServerInfo &ServerInfo)
	{
		if(!m_Activity.instance)
			return;

		// secret is only shared when player is joining the game, or when they are invited for private games
		if(str_length(ServerInfo.m_aAddress) < (int)sizeof(m_Activity.secrets.join))
		{
			str_copy(m_Activity.secrets.join, ServerInfo.m_aAddress);
		}
		else
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ServerInfo.m_aAddresses[0], aAddr, sizeof(aAddr), true);
			str_copy(m_Activity.secrets.join, aAddr);
		}

		if(m_Activity.party.privacy == DiscordActivityPartyPrivacy_Public)
		{
			// id is sha256, because it didn't work with the ':' character
			char aPartyId[SHA256_MAXSTRSIZE];
			SHA256_DIGEST PartyIdSha256 = sha256(m_Activity.secrets.join, str_length(m_Activity.secrets.join));
			sha256_str(PartyIdSha256, aPartyId, sizeof(aPartyId));
			str_copy(m_Activity.party.id, aPartyId);
		}
	}

	static void DISCORD_CALLBACK OnActivityJoin(void *pEventData, const char *pSecret)
	{
		CDiscord *pSelf = static_cast<CDiscord *>(pEventData);
		IClient *pClient = pSelf->Kernel()->RequestInterface<IClient>();
		pClient->Connect(pSecret);
	}
};

static IDiscord *CreateDiscordImpl()
{
	FDiscordCreate pfnDiscordCreate = GetDiscordCreate();
	if(!pfnDiscordCreate)
	{
		return nullptr;
	}
	CDiscord *pDiscord = new CDiscord();
	pDiscord->SetCreateFunction(pfnDiscordCreate);
	return pDiscord;
}
#else
static IDiscord *CreateDiscordImpl()
{
	return nullptr;
}
#endif

class CDiscordStub : public IDiscord
{
	void Start(int64_t AppId, const char *pAssetName) override
	{
		(void)AppId;
		(void)pAssetName;
	}
	void Update() override {}
	void ClearGameInfo() override {}
	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override {}
	void UpdateServerInfo(const CServerInfo &ServerInfo) override {}
	void UpdatePlayerCount(int Count) override {}
};

// Defined in discord_ipc.cpp. Preferred where it exists, because the socket
// takes buttons and the SDK does not, and because it needs no library.
IDiscord *CreateDiscordIpc();

IDiscord *CreateDiscord()
{
	IDiscord *pDiscord = CreateDiscordIpc();
	if(pDiscord)
	{
		return pDiscord;
	}
	pDiscord = CreateDiscordImpl();
	if(pDiscord)
	{
		return pDiscord;
	}
	return new CDiscordStub();
}
