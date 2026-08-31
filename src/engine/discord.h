#ifndef ENGINE_DISCORD_H
#define ENGINE_DISCORD_H

#include "kernel.h"

#include <base/types.h>

#include <engine/serverbrowser.h>

class IDiscord : public IInterface
{
	MACRO_INTERFACE("discord")
public:
	// Connects to Discord. Separate from construction because the application id
	// comes from the configuration, and the configuration file is executed long
	// after the interfaces are built.
	virtual void Start(int64_t AppId, const char *pAssetName) = 0;

	virtual void Update() = 0;

	virtual void ClearGameInfo() = 0;
	virtual void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) = 0;
	virtual void UpdateServerInfo(const CServerInfo &ServerInfo) = 0;
	virtual void UpdatePlayerCount(int Count) = 0;
};

IDiscord *CreateDiscord();

#endif // ENGINE_DISCORD_H
