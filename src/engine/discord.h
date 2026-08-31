#ifndef ENGINE_DISCORD_H
#define ENGINE_DISCORD_H

#include "kernel.h"

#include <base/types.h>

#include <engine/serverbrowser.h>

class IDiscord : public IInterface
{
	MACRO_INTERFACE("discord")
public:
	virtual void Update() = 0;

	virtual void ClearGameInfo() = 0;
	virtual void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) = 0;
	virtual void UpdateServerInfo(const CServerInfo &ServerInfo) = 0;
	virtual void UpdatePlayerCount(int Count) = 0;
};

// The Discord application the rich presence runs under. Zero uses DDNet's, which
// is what makes Discord show DDNet's name and artwork; pass your own to have it
// show yours.
IDiscord *CreateDiscord(int64_t AppId);

#endif // ENGINE_DISCORD_H
