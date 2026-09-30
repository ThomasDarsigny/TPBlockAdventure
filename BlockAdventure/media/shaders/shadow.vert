#version 120

// Passe de profondeur vue du soleil. Le format de sommet etant compact
// (positions en 1/8 de bloc), il faut le meme decodage que terrain.vert.
varying vec2 vUv;

void main()
{
    vUv = gl_MultiTexCoord0.xy * (1.0 / 32767.0);
    gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.xyz * 0.125, 1.0);
}
