#version 120

// On ne calcule aucune couleur, mais les feuillages et les plantes doivent
// laisser passer la lumiere la ou leur texture est transparente.
uniform sampler2D uAtlas;

varying vec2 vUv;

void main()
{
    if (texture2D(uAtlas, vUv).a < 0.5)
        discard;

    gl_FragColor = vec4(1.0);
}
