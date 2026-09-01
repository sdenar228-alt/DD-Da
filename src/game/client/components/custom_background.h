#ifndef GAME_CLIENT_COMPONENTS_CUSTOM_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_CUSTOM_BACKGROUND_H

#include <base/detect.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <memory>
#include <string>
#include <vector>

// Draws a user supplied image or video behind everything else, both in game and
// in the menus. Videos and non-PNG images go to the codecs that ship with the
// system first, and fall back to FFmpeg, which is already linked for the video
// recorder.
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
	// The codecs that ship with the system, tried before FFmpeg. Present on
	// every platform; the ones without such codecs refuse to open anything.
	std::unique_ptr<class CSystemMedia> m_pSystemMedia;
	bool m_UsingSystemMedia = false;

	// Reused between video frames, a fresh one per frame was megabytes of
	// allocation and zero fill at the video's frame rate.
	// The size the texture was created at. A video keeps the same one for its
	// whole run, which is what lets every frame after the first be written into
	// the texture that is already there.
	int m_TextureWidth = 0;
	int m_TextureHeight = 0;
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
