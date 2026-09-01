#include "custom_sounds.h"

#include <base/log.h>
#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/sound.h>
#include <engine/storage.h>

#include <generated/client_data.h>

#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

#include <utility>

const char *const CCustomSounds::EXTRA_SOUND_NAMES[] = {
	"player_join",
	"player_leave",
};

// Tried in this order, the first file that exists wins.
static const char *const SUPPORTED_EXTENSIONS[] = {".wav", ".opus", ".wv"};

int CCustomSounds::LoadPackSample(const char *pPack, const char *pName)
{
	for(const char *pExtension : SUPPORTED_EXTENSIONS)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "sounds/%s/%s%s", pPack, pName, pExtension);
		if(!Storage()->FileExists(aPath, IStorage::TYPE_ALL))
			continue;

		int SampleId = -1;
		if(str_comp(pExtension, ".wav") == 0)
			SampleId = Sound()->LoadWav(aPath);
		else if(str_comp(pExtension, ".opus") == 0)
			SampleId = Sound()->LoadOpus(aPath);
		else
			SampleId = Sound()->LoadWV(aPath);

		if(SampleId >= 0)
			return SampleId;
		log_error("customsounds", "Could not load '%s'", aPath);
	}
	return -1;
}

void CCustomSounds::RestoreOriginals()
{
	for(const CReplacedSet &Replaced : m_vReplacedSets)
	{
		CDataSoundset &Set = g_pData->m_aSounds[Replaced.m_SetId];
		for(int i = 0; i < Set.m_NumSounds && i < (int)Replaced.m_vOriginalSampleIds.size(); ++i)
		{
			Set.m_aSounds[i].m_Id = Replaced.m_vOriginalSampleIds[i];
		}
		if(Replaced.m_LoadedSampleId >= 0)
			Sound()->UnloadSample(Replaced.m_LoadedSampleId);
	}
	m_vReplacedSets.clear();

	for(int &SampleId : m_aExtraSampleIds)
	{
		if(SampleId >= 0)
			Sound()->UnloadSample(SampleId);
		SampleId = -1;
	}
}

void CCustomSounds::ApplyPack()
{
	const char *pPack = g_Config.m_ClCustomSoundPack;
	if(pPack[0] == '\0')
		return;

	for(int SetId = 0; SetId < g_pData->m_NumSounds; ++SetId)
	{
		CDataSoundset &Set = g_pData->m_aSounds[SetId];
		if(Set.m_pName == nullptr || Set.m_NumSounds <= 0)
			continue;

		const int SampleId = LoadPackSample(pPack, Set.m_pName);
		if(SampleId < 0)
			continue;

		CReplacedSet Replaced;
		Replaced.m_SetId = SetId;
		Replaced.m_LoadedSampleId = SampleId;
		Replaced.m_vOriginalSampleIds.reserve(Set.m_NumSounds);
		for(int i = 0; i < Set.m_NumSounds; ++i)
		{
			Replaced.m_vOriginalSampleIds.push_back(Set.m_aSounds[i].m_Id);
			// A pack provides one file per sound, so every random variant of the
			// set points at it.
			Set.m_aSounds[i].m_Id = SampleId;
		}
		m_vReplacedSets.push_back(std::move(Replaced));
	}

	for(int i = 0; i < NUM_EXTRA_SOUNDS; ++i)
	{
		m_aExtraSampleIds[i] = LoadPackSample(pPack, EXTRA_SOUND_NAMES[i]);
	}

	log_info("customsounds", "Sound pack '%s': replaced %d sounds", pPack, (int)m_vReplacedSets.size());
}

void CCustomSounds::UpdatePack()
{
	if(m_PackApplied && m_LoadedPack == g_Config.m_ClCustomSoundPack)
		return;

	// The default samples are loaded by a background job at startup; overriding
	// them before that would be undone again.
	if(GameClient()->m_Sounds.IsLoading())
		return;

	RestoreOriginals();
	m_LoadedPack = g_Config.m_ClCustomSoundPack;
	ApplyPack();
	m_PackApplied = true;
}

void CCustomSounds::OnRender()
{
	UpdatePack();
}

void CCustomSounds::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		// Everybody shows up as newly joined right after connecting.
		m_QuietUntil = Client()->LocalTime() + 2.0f;
		for(bool &WasActive : m_aWasActive)
			WasActive = false;
	}
}

void CCustomSounds::OnNewSnapshot()
{
	const bool Quiet = Client()->LocalTime() < m_QuietUntil;
	const float Volume = g_Config.m_ClCustomSoundEventVolume / 100.0f;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const bool Active = GameClient()->m_aClients[ClientId].m_Active;
		if(Active == m_aWasActive[ClientId])
			continue;
		m_aWasActive[ClientId] = Active;

		if(Quiet || Volume <= 0.0f)
			continue;
		// Your own connect is covered by the quiet period, but a dummy joining
		// later should not make a noise either.
		if(ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1])
			continue;

		const int Extra = Active ? EXTRA_PLAYER_JOIN : EXTRA_PLAYER_LEAVE;
		const bool Enabled = Active ? g_Config.m_ClCustomSoundJoin != 0 : g_Config.m_ClCustomSoundLeave != 0;
		if(!Enabled || m_aExtraSampleIds[Extra] < 0)
			continue;
		GameClient()->m_Sounds.PlaySample(CSounds::CHN_GLOBAL, m_aExtraSampleIds[Extra], 0, Volume);
	}
}

void CCustomSounds::OnShutdown()
{
	RestoreOriginals();
	m_PackApplied = false;
	m_LoadedPack.clear();
}
