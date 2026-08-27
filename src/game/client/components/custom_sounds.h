#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_SOUNDS_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_SOUNDS_H

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <string>
#include <vector>

// Replaces the built in sounds with the ones from a sound pack, and adds sounds
// for events that the vanilla client has none for, like a player joining.
//
// A pack is a folder inside `sounds`, holding files named after the sound sets
// of the game (for example `hook_attach_ground.wav` or `hammer_hit.opus`). The
// supported formats are wav, opus and wv.
class CCustomSounds : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnNewSnapshot() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	// Sound set names that are not part of the game data but are played by this
	// component. They live in the same pack folder.
	static const char *const EXTRA_SOUND_NAMES[];
	enum
	{
		EXTRA_PLAYER_JOIN = 0,
		EXTRA_PLAYER_LEAVE,
		NUM_EXTRA_SOUNDS,
	};

private:
	// The samples that were replaced, so that they can be put back.
	class CReplacedSet
	{
	public:
		int m_SetId;
		std::vector<int> m_vOriginalSampleIds;
		int m_LoadedSampleId;
	};
	std::vector<CReplacedSet> m_vReplacedSets;
	int m_aExtraSampleIds[NUM_EXTRA_SOUNDS] = {-1, -1};

	std::string m_LoadedPack;
	bool m_PackApplied = false;

	// Join and leave are detected by watching which clients are active.
	bool m_aWasActive[MAX_CLIENTS] = {};
	// Everybody "joins" at once right after connecting, that must stay silent.
	float m_QuietUntil = 0.0f;

	void UpdatePack();
	void ApplyPack();
	void RestoreOriginals();
	int LoadPackSample(const char *pPack, const char *pName);
};

#endif
