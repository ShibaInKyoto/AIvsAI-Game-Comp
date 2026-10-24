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
uniform vec4 uRipples[4];     // xy = centre (uv), z = radius (uv, height-relative), w = strength
uniform int uRippleCount;

out vec4 finalColor;

float Hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Expanding refraction rings (shockwaves) bend the image around them.
vec2 ApplyRipples(vec2 uv)
{
    float aspect = uResolution.x / uResolution.y;
    vec2 offset = vec2(0.0);
    for (int i = 0; i < 4; ++i) {
        if (i >= uRippleCount) {
            break;
        }
        vec4 r = uRipples[i];
        vec2 d = uv - r.xy;
        d.x *= aspect;
        float dist = length(d);
        float width = 0.04 + r.z * 0.15;
        float ring = 1.0 - clamp(abs(dist - r.z) / width, 0.0, 1.0);
        ring = ring * ring * (3.0 - 2.0 * ring);
        if (dist > 0.0001) {
            vec2 dir = d / dist;
            dir.x /= aspect;
            offset += dir * ring * r.w;
        }
    }
    return uv + offset;
}

void main()
{
    // Only the sharp scene is refracted; the soft bloom stays put, which avoids
    // stretching its low-resolution texels into visible blocks.
    vec2 uv = ApplyRipples(fragTexCoord);
    vec2 bloomUv = fragTexCoord;
    vec2 centered = uv - 0.5;

    vec3 scene;
    vec3 bloom;
    if (uAberration > 0.0001) {
        vec2 offset = centered * uAberration;
        scene = vec3(texture(texture0, uv - offset).r,
                     texture(texture0, uv).g,
                     texture(texture0, uv + offset).b);
        bloom = vec3(texture(uBloom, bloomUv - offset).r,
                     texture(uBloom, bloomUv).g,
                     texture(uBloom, bloomUv + offset).b);
    } else {
        scene = texture(texture0, uv).rgb;
        bloom = texture(uBloom, bloomUv).rgb;
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
