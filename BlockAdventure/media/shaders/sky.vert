#version 120

// Quad plein ecran: gl_Vertex contient directement des coordonnees NDC.
// On interpole les directions des 4 coins de la camera pour obtenir, dans le
// fragment shader, le rayon exact qui traverse chaque pixel.

uniform vec3 uRayBL;
uniform vec3 uRayBR;
uniform vec3 uRayTL;
uniform vec3 uRayTR;

varying vec3 vDir;

void main()
{
    vec2 p = gl_Vertex.xy;
    float sx = p.x * 0.5 + 0.5;
    float sy = p.y * 0.5 + 0.5;

    vec3 bottom = mix(uRayBL, uRayBR, sx);
    vec3 top    = mix(uRayTL, uRayTR, sx);
    vDir = mix(bottom, top, sy);

    gl_Position = vec4(p, 0.0, 1.0);
}
