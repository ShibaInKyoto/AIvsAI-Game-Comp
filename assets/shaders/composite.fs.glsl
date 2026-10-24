#version 330

// Final frame composite: scene + bloom, tone mapping and screen effects.

in vec2 fragTexCoord;

uniform sampler2D texture0;   // HDR scene
uniform sampler2D uBloom;     // bloom chain result
uniform vec2 uResolution;
uniform float uTime;
uniform float uBloomStrength;
uniform float uExposure;
uniform float uVignette;
uniform float uAberration;
uniform float uGrain;
uniform float uScanlines;
uniform float uSaturation;
uniform vec4 uFlash;          // rgb = colour, a = strength

out vec4 finalColor;

float Hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main()
{
    vec2 uv = fragTexCoord;
    vec2 centered = uv - 0.5;

    vec3 scene;
    vec3 bloom;
    if (uAberration > 0.0001) {
        vec2 offset = centered * uAberration;
        scene = vec3(texture(texture0, uv - offset).r,
                     texture(texture0, uv).g,
                     texture(texture0, uv + offset).b);
        bloom = vec3(texture(uBloom, uv - offset).r,
                     texture(uBloom, uv).g,
                     texture(uBloom, uv + offset).b);
    } else {
        scene = texture(texture0, uv).rgb;
        bloom = texture(uBloom, uv).rgb;
    }

    vec3 hdr = scene + bloom * uBloomStrength;
    hdr += uFlash.rgb * uFlash.a;

    // Exponential tone map: saturated colours roll smoothly toward white.
    vec3 color = vec3(1.0) - exp(-hdr * uExposure);

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, uSaturation);

    float vignette = smoothstep(0.95, 0.25, length(centered * vec2(1.0, 0.85)));
    color *= mix(1.0, vignette, uVignette);

    if (uScanlines > 0.0) {
        float line = 0.5 + 0.5 * sin(uv.y * uResolution.y * 3.14159);
        color *= 1.0 - uScanlines * (1.0 - line);
    }

    color += (Hash(uv * uResolution + fract(uTime) * 100.0) - 0.5) * uGrain;
    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
