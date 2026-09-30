#version 120

// Flou gaussien separable (une passe horizontale puis une verticale).
uniform sampler2D uScene;
uniform vec2 uDirection;   // (1/largeur, 0) ou (0, 1/hauteur)

varying vec2 vUv;

void main()
{
    const float w0 = 0.227027;
    const float w1 = 0.194594;
    const float w2 = 0.121621;
    const float w3 = 0.054054;
    const float w4 = 0.016216;

    vec3 c = texture2D(uScene, vUv).rgb * w0;
    c += texture2D(uScene, vUv + uDirection * 1.0).rgb * w1;
    c += texture2D(uScene, vUv - uDirection * 1.0).rgb * w1;
    c += texture2D(uScene, vUv + uDirection * 2.0).rgb * w2;
    c += texture2D(uScene, vUv - uDirection * 2.0).rgb * w2;
    c += texture2D(uScene, vUv + uDirection * 3.0).rgb * w3;
    c += texture2D(uScene, vUv - uDirection * 3.0).rgb * w3;
    c += texture2D(uScene, vUv + uDirection * 4.0).rgb * w4;
    c += texture2D(uScene, vUv - uDirection * 4.0).rgb * w4;

    gl_FragColor = vec4(c, 1.0);
}
