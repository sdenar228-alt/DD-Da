#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_BACKGROUND_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <memory>
#include <string>

// Draws a user supplied image or video behind everything else, both in game and
// in the menus. Videos and non-PNG images are decoded with FFmpeg, which is
// already linked for the video recorder.
class CCustomBackground : public CComponent
{
public:
	CCustomBackground();
	~CCustomBackground();

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnShutdown() override;

	// Draws the background across the whole screen. Returns false when there is
	// nothing to draw, so that the caller can fall back to its own background.
	// Used by the menus, which draw their own background after the components.
	bool RenderFullscreen();

private:
	// FFmpeg lives behind a pimpl so that its headers do not leak into
	// gameclient.h, which every component includes.
	class CMedia;
	std::unique_ptr<CMedia> m_pMedia;

	IGraphics::CTextureHandle m_Texture;
	std::string m_LoadedFile;
	bool m_LoadFailed = false;
	// A still image only has to be uploaded once.
	bool m_IsStill = false;
	bool m_HasFrame = false;
	int m_Width = 0;
	int m_Height = 0;

	void Update();
	void Unload();
};

#endif
