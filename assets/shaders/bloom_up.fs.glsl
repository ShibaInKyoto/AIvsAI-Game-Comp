#version 330

// 9-tap tent filter upsample, blended additively onto the next larger mip.

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 uTexel;
uniform float uRadius;

out vec4 finalColor;

void main()
{
    vec2 uv = fragTexCoord;
    vec4 d = vec4(uTexel, -uTexel.x, 0.0) * uRadius;

    vec3 s = texture(texture0, uv - d.xy).rgb;
    s += texture(texture0, uv - d.wy).rgb * 2.0;
    s += texture(texture0, uv - d.zy).rgb;
    s += texture(texture0, uv + d.zw).rgb * 2.0;
    s += texture(texture0, uv).rgb * 4.0;
    s += texture(texture0, uv + d.xw).rgb * 2.0;
    s += texture(texture0, uv + d.zy).rgb;
    s += texture(texture0, uv + d.wy).rgb * 2.0;
    s += texture(texture0, uv + d.xy).rgb;

    finalColor = vec4(s * (1.0 / 16.0), 1.0);
}
