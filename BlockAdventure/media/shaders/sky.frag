#version 120

// ---------------------------------------------------------------------------
//  Ciel procedural: degrade, soleil, lune, etoiles et nuages animes.
//  Tout est calcule analytiquement, aucune texture n'est necessaire.
// ---------------------------------------------------------------------------

uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform vec3  uZenith;
uniform vec3  uHorizon;
uniform vec3  uGround;
uniform vec3  uCamPos;
uniform float uTime;
uniform float uDayFactor;
uniform float uUnderwater;
uniform float uDirectOutput;

varying vec3 vDir;

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float ValueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = Hash(i);
    float b = Hash(i + vec2(1.0, 0.0));
    float c = Hash(i + vec2(0.0, 1.0));
    float d = Hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float Fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        v += ValueNoise(p) * a;
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

void main()
{
    vec3 d = normalize(vDir);

    // --- Degrade de base ---------------------------------------------------
    vec3 col;
    if (d.y >= 0.0)
        col = mix(uHorizon, uZenith, pow(d.y, 0.55));
    else
        col = mix(uHorizon, uGround, clamp(-d.y * 2.5, 0.0, 1.0));

    // --- Etoiles (avant le soleil pour qu'elles passent derriere) ----------
    float night = 1.0 - clamp(uDayFactor * 1.6, 0.0, 1.0);
    if (night > 0.01 && d.y > 0.02)
    {
        // Une etoile est un petit point tire au hasard dans sa cellule, et
        // non la cellule entiere: sinon on obtient de gros carres blancs.
        vec2 sp = d.xz / max(d.y + 0.25, 0.15) * 120.0;
        vec2 cell = floor(sp);
        float s = Hash(cell);
        if (s > 0.982)
        {
            vec2 centre = cell + vec2(Hash(cell + 13.7), Hash(cell + 71.3));
            float dist = length(sp - centre);
            float star = smoothstep(0.30, 0.02, dist);
            float twinkle = 0.6 + 0.4 * sin(uTime * 2.7 + s * 90.0);
            float horizon = smoothstep(0.02, 0.22, d.y);
            col += vec3(0.85, 0.90, 1.0) * star * twinkle * night * horizon * 1.4;
        }
    }

    // --- Soleil ------------------------------------------------------------
    float sd = max(dot(d, uSunDir), 0.0);
    col += uSunColor * pow(sd, 2200.0) * 14.0;
    col += uSunColor * pow(sd, 7.0) * 0.30 * uDayFactor;

    // --- Lune (a l'oppose du soleil) --------------------------------------
    vec3 moonDir = -uSunDir;
    float md = max(dot(d, moonDir), 0.0);
    col += vec3(0.90, 0.93, 1.00) * pow(md, 3000.0) * 9.0 * night;
    col += vec3(0.35, 0.40, 0.55) * pow(md, 60.0) * 0.25 * night;

    // --- Nuages ------------------------------------------------------------
    if (d.y > 0.015)
    {
        vec2 cp = uCamPos.xz + d.xz * (150.0 / d.y);
        float n = Fbm(cp * 0.0021 + vec2(uTime * 0.0045, uTime * 0.0022));
        float cover = smoothstep(0.44, 0.72, n);
        float fade = smoothstep(0.015, 0.20, d.y);

        // Les nuages sont eclaires par le soleil rasant a l'aube et au crepuscule
        vec3 cloudLit = mix(vec3(0.34, 0.36, 0.46), vec3(1.0, 0.98, 0.94), uDayFactor);
        cloudLit = mix(cloudLit, uSunColor, pow(sd, 4.0) * 0.5);

        col = mix(col, cloudLit, cover * fade * 0.8);
    }

    if (uUnderwater > 0.5)
        col = mix(col, vec3(0.05, 0.19, 0.38), 0.85);

    if (uDirectOutput > 0.5)
    {
        vec3 x = col * 1.05;
        col = clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
        col = pow(col, vec3(1.0 / 2.2));
    }

    gl_FragColor = vec4(col, 1.0);
}
