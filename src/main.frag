#version 120

// DONT TOUCH

uniform sampler2D texture;

varying vec2 texCoordsOut;

void main() { gl_FragColor = texture2D(texture, texCoordsOut); }
