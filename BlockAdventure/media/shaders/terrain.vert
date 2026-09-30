#version 120

// ---------------------------------------------------------------------------
//  Shader de terrain.
//
//  Les attributs arrivent sous forme compacte (voir vertexbuffer.h):
//    gl_Vertex        : position en 1/8 de bloc
//    gl_Color.r       : lumiere du ciel   (0..1)
//    gl_Color.g       : lumiere des blocs (0..1)
//    gl_Color.b       : occlusion ambiante * ombrage de face
//    gl_Color.a * 255 : code = face (bits 0-2) + animation (bits 3-4)
//    gl_MultiTexCoord0: coordonnees d'atlas * 32767
// ---------------------------------------------------------------------------

uniform vec3  uChunkOrigin;
uniform float uTime;

varying vec2  vUv;
varying vec3  vWorldPos;
varying vec3  vNormal;
varying float vSky;
varying float vBlockLight;
varying float vShade;
varying float vWave;
varying float vFogDist;

vec3 FaceNormal(int f)
{
    if (f == 0) return vec3( 0.0,  1.0,  0.0);
    if (f == 1) return vec3( 0.0, -1.0,  0.0);
    if (f == 2) return vec3(-1.0,  0.0,  0.0);
    if (f == 3) return vec3( 1.0,  0.0,  0.0);
    if (f == 4) return vec3( 0.0,  0.0,  1.0);
    return vec3( 0.0,  0.0, -1.0);
}

void main()
{
    vec3 pos = gl_Vertex.xyz * 0.125;

    int code = int(gl_Color.a * 255.0 + 0.5);
    int face = code - (code / 8) * 8;
    int wave = code / 8;

    vNormal = FaceNormal(face);
    vWave   = float(wave);

    vec3 world = pos + uChunkOrigin;

    if (wave == 1)
    {
        // Feuillages et plantes: leger balancement, plus fort en hauteur
        float t = uTime * 1.7;
        float amp = 0.045;
        pos.x += sin(t + world.x * 0.65 + world.z * 0.45) * amp;
        pos.z += cos(t * 0.85 + world.x * 0.35 + world.z * 0.7) * amp * 0.8;
    }
    else if (wave == 2)
    {
        // Eau: seuls les sommets de la surface ondulent, sinon des trous
        // apparaitraient entre les faces laterales.
        float fy = fract(gl_Vertex.y * 0.125);
        if (fy > 0.8)
        {
            float t = uTime * 1.25;
            pos.y += (sin(world.x * 0.55 + t) + sin(world.z * 0.78 + t * 1.4)) * 0.028;
        }
    }

    vWorldPos = pos + uChunkOrigin;

    vUv         = gl_MultiTexCoord0.xy * (1.0 / 32767.0);
    vSky        = gl_Color.r;
    vBlockLight = gl_Color.g;
    vShade      = gl_Color.b;

    vec4 viewPos = gl_ModelViewMatrix * vec4(pos, 1.0);
    vFogDist = length(viewPos.xyz);

    gl_Position = gl_ProjectionMatrix * viewPos;
}
