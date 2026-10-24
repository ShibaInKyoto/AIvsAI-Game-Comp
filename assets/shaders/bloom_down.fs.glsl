#version 330

// 13-tap downsample (Jimenez, "Next Generation Post Processing in Call of Duty:
// Advanced Warfare", 2014). The first pass also applies a soft brightness
// threshold so only hot pixels contribute to the bloom.

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 uTexel;
uniform float uPrefilter;
uniform float uThreshold;
uniform float uKnee;

out vec4 finalColor;

vec3 Prefilter(vec3 c)
{
    c = min(c, vec3(32.0));
    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - uThreshold + uKnee, 0.0, 2.0 * uKnee);
    soft = soft * soft / (4.0 * uKnee + 1e-4);
    float contribution = max(soft, brightness - uThreshold) / max(brightness, 1e-4);
    return c * contribution;
}

void main()
{
    vec2 uv = fragTexCoord;
    vec3 a = texture(texture0, uv + uTexel * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(texture0, uv + uTexel * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(texture0, uv + uTexel * vec2( 2.0,  2.0)).rgb;
    vec3 d = texture(texture0, uv + uTexel * vec2(-2.0,  0.0)).rgb;
    vec3 e = texture(texture0, uv).rgb;
    vec3 f = texture(texture0, uv + uTexel * vec2( 2.0,  0.0)).rgb;
    vec3 g = texture(texture0, uv + uTexel * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(texture0, uv + uTexel * vec2( 0.0, -2.0)).rgb;
    vec3 i = texture(texture0, uv + uTexel * vec2( 2.0, -2.0)).rgb;
    vec3 j = texture(texture0, uv + uTexel * vec2(-1.0,  1.0)).rgb;
    vec3 k = texture(texture0, uv + uTexel * vec2( 1.0,  1.0)).rgb;
    vec3 l = texture(texture0, uv + uTexel * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(texture0, uv + uTexel * vec2( 1.0, -1.0)).rgb;

    vec3 result = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;

    if (uPrefilter > 0.5) {
        result = Prefilter(result);
    }
    finalColor = vec4(result, 1.0);
}
