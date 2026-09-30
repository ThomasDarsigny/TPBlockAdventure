#version 120

// Extraction des zones lumineuses pour le bloom.
uniform sampler2D uScene;
uniform float uThreshold;

varying vec2 vUv;

void main()
{
    vec3 c = texture2D(uScene, vUv).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float k = max(luma - uThreshold, 0.0) / max(luma, 0.0001);
    gl_FragColor = vec4(c * k, 1.0);
}
