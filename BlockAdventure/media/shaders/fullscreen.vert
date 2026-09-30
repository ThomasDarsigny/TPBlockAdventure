#version 120

// Quad plein ecran commun aux passes de post-traitement.
varying vec2 vUv;

void main()
{
    vUv = gl_Vertex.xy * 0.5 + 0.5;
    gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
}
