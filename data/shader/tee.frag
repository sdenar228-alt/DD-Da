// Custom tee shader.
//
// Used for the tee sprites while `cl_custom_tee_shader 1` is set. Only the
// OpenGL backend supports it; on Vulkan the setting has no effect.
//
// Available uniforms:
//   gTextureSampler  the tee sprite (only with TW_TEXTURED)
//   gVerticesColor   the color the game asked for, alpha included
//   gTime            seconds since the client started, for animations
// Available varyings:
//   texCoord         texture coordinate of the fragment
//   vertColor        per vertex color
//
// Keep `FragClr` as the output. Editing this file is enough, no rebuild needed;
// a copy in the 'shader' folder of your config directory takes precedence over
// this one.

#ifdef TW_TEXTURED
uniform sampler2D gTextureSampler;
#endif

uniform vec4 gVerticesColor;
uniform float gTime;

noperspective in vec2 texCoord;
noperspective in vec4 vertColor;

out vec4 FragClr;

// Hue shift, used by the rainbow example below.
vec3 HueShift(vec3 Color, float Angle)
{
	const vec3 K = vec3(0.57735, 0.57735, 0.57735);
	float CosAngle = cos(Angle);
	return Color * CosAngle + cross(K, Color) * sin(Angle) + K * dot(K, Color) * (1.0 - CosAngle);
}

void main()
{
#ifdef TW_TEXTURED
	vec4 tex = texture(gTextureSampler, texCoord);
	vec4 Color = tex * vertColor * gVerticesColor;
#else
	vec4 Color = vertColor * gVerticesColor;
#endif

	// ----- your effect goes here -----
	// This default keeps the tee exactly as the game draws it. Uncomment one of
	// the examples, or write your own.

	// Rainbow:
	// Color.rgb = HueShift(Color.rgb, gTime * 2.0);

	// Pulsing brightness:
	// Color.rgb *= 0.75 + 0.25 * sin(gTime * 6.0);

	// ---------------------------------

	FragClr = Color;
}
