#ifndef GAME_CLIENT_COMPONENTS_PARTICLES3D_H
#define GAME_CLIENT_COMPONENTS_PARTICLES3D_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

// Wireframe shapes drifting and tumbling behind the game: cubes, hearts,
// circles, hexagons, triangles. Pure decoration, drawn as lines over the map
// background and under everything that matters.
class CParticles3d : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override { m_Inited = false; }

private:
	static constexpr int MAX_PARTICLES = 100;

	class CShape
	{
	public:
		vec2 m_Pos;
		vec2 m_Drift;
		// The three tumble angles and how fast each one turns.
		vec3 m_Angle;
		vec3 m_Spin;
		// 0..1, scales the configured size so the field has depth.
		float m_Scale;
		int m_Kind;
		ColorRGBA m_RandomColor;
	};

	CShape m_aShapes[MAX_PARTICLES];
	bool m_Inited = false;
	float m_LastTime = 0.0f;

	void InitShapes();
	void DrawShape(const CShape &Shape, ColorRGBA Color, float SizeBase, vec2 Offset, float Alpha) const;
};

#endif
