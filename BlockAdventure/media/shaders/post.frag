#version 120

// ---------------------------------------------------------------------------
//  Passe finale: composition du bloom, tone mapping ACES, etalonnage,
//  vignettage et effets sous-marins.
// ---------------------------------------------------------------------------

uniform sampler2D uScene;
uniform sampler2D uBloom;

uniform float uExposure;
uniform float uBloomStrength;
uniform float uSaturation;
uniform float uVignette;
uniform float uUnderwater;
uniform float uTime;
uniform float uDamage;      // flash rouge quand le joueur prend des degats

varying vec2 vUv;

// Approximation ACES filmique (Narkowicz)
vec3 ACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec2 uv = vUv;

    // Sous l'eau, l'image ondule doucement
    if (uUnderwater > 0.5)
    {
        uv.x += sin(uv.y * 26.0 + uTime * 2.1) * 0.0035;
        uv.y += cos(uv.x * 22.0 + uTime * 1.7) * 0.0035;
    }

    vec3 color = texture2D(uScene, uv).rgb;
    color += texture2D(uBloom, uv).rgb * uBloomStrength;

    color *= uExposure;
    color = ACES(color);

    // Saturation
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, uSaturation);

    // Vignettage
    vec2 d = vUv - 0.5;
    float vig = 1.0 - dot(d, d) * uVignette;
    color *= clamp(vig, 0.0, 1.0);

    if (uDamage > 0.001)
        color = mix(color, vec3(0.65, 0.05, 0.05), uDamage * 0.55);

    // Correction gamma
    color = pow(color, vec3(1.0 / 2.2));

    gl_FragColor = vec4(color, 1.0);
}
