#version 330

// Additive "neon" shading for the glow sprite batch.
// The atlas alpha channel holds the glow falloff profile and the red channel a
// "hot core" mask that is pushed toward white for the classic neon look.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float uBoost;
uniform float uWhiteCore;

out vec4 finalColor;

void main()
{
    vec4 t = texture(texture0, fragTexCoord);
    float a = t.a * fragColor.a;
    vec3 tint = mix(fragColor.rgb, vec3(1.0), clamp(t.r * uWhiteCore, 0.0, 1.0));
    finalColor = vec4(tint * a * uBoost, 0.0);
}
