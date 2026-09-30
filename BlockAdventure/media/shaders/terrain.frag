#version 120

uniform sampler2D uAtlas;

uniform vec3  uCamPos;
uniform vec3  uSunDir;        // direction vers le soleil, normalisee
uniform vec3  uSunColor;
uniform vec3  uSkyColor;      // couleur de l'horizon, sert au brouillard
uniform vec3  uAmbient;
uniform float uDayFactor;     // 0 = nuit, 1 = plein jour
uniform float uFogStart;
uniform float uFogEnd;
uniform float uAlphaTest;     // 0.5 en passe solide, 0.0 en passe transparente
uniform float uUnderwater;    // 1.0 si la camera est dans un liquide
uniform float uTime;
uniform float uDirectOutput;  // 1.0 quand le post-traitement est desactive

varying vec2  vUv;
varying vec3  vWorldPos;
varying vec3  vNormal;
varying float vSky;
varying float vBlockLight;
varying float vShade;
varying float vWave;
varying float vFogDist;

void main()
{
    vec4 texel = texture2D(uAtlas, vUv);
    if (texel.a < uAlphaTest)
        discard;

    // Les textures sont encodees en sRGB: on repasse en lineaire pour que
    // l'eclairage et le brouillard soient physiquement coherents. La
    // conversion inverse est faite par la passe de post-traitement.
    texel.rgb = pow(texel.rgb, vec3(2.2));

    // --- Eclairage ---------------------------------------------------------
    // La lumiere du ciel est modulee par l'heure, celle des blocs ne l'est
    // pas: une torche eclaire autant a minuit qu'a midi.
    float sky = vSky;
    float blk = vBlockLight;

    vec3 dayLight   = uSunColor * (sky * uDayFactor);
    vec3 nightLight = vec3(0.17, 0.21, 0.34) * sky * (1.0 - uDayFactor);
    vec3 torchLight = vec3(1.00, 0.71, 0.38) * (blk * blk);

    vec3 light = dayLight + nightLight + torchLight + uAmbient;

    // Legere contribution directionnelle: les faces tournees vers le soleil
    // sont un peu plus vives.
    float ndl = max(dot(vNormal, uSunDir), 0.0);
    light *= (0.86 + 0.26 * ndl * uDayFactor * sky);

    vec3 color = texel.rgb * light * vShade;

    // --- Reflets sur l'eau -------------------------------------------------
    if (vWave > 1.5)
    {
        vec3 V = normalize(uCamPos - vWorldPos);

        // Normale perturbee par de petites vagues
        // Ondulation fine et desalignee de la grille de blocs, sinon le
        // reflet se repete a l'identique sur chaque bloc.
        float t = uTime * 1.1;
        vec3 N = normalize(vNormal + vec3(
            sin(vWorldPos.x * 0.83 + vWorldPos.z * 0.41 + t) * 0.055,
            0.0,
            cos(vWorldPos.z * 0.91 + vWorldPos.x * 0.37 + t * 1.3) * 0.055));

        vec3 H = normalize(V + uSunDir);
        float spec = pow(max(dot(N, H), 0.0), 220.0) * uDayFactor * sky;
        color += uSunColor * spec * 0.65;

        // Fresnel: l'eau devient miroir quand on la regarde de biais
        float fres = pow(1.0 - max(dot(N, V), 0.0), 4.0);
        color = mix(color, uSkyColor * (0.35 + 0.65 * uDayFactor), fres * 0.28);
    }

    // --- Brouillard --------------------------------------------------------
    vec3  fogColor = uSkyColor;
    float fog = clamp((vFogDist - uFogStart) / max(uFogEnd - uFogStart, 1.0), 0.0, 1.0);
    fog = fog * fog;

    if (uUnderwater > 0.5)
    {
        color *= vec3(0.38, 0.66, 0.98);
        fogColor = vec3(0.05, 0.19, 0.38);
        fog = clamp(vFogDist / 26.0, 0.0, 1.0);
    }

    color = mix(color, fogColor, fog);

    if (uDirectOutput > 0.5)
    {
        // Pas de FBO disponible: on fait ici le tone mapping et le gamma
        vec3 x = color * 1.05;
        color = clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
        color = pow(color, vec3(1.0 / 2.2));
    }

    gl_FragColor = vec4(color, texel.a);
}
