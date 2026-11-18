#version 120

varying vec2 texCoordsOut;

// Given a world position (for this lab, bottom-left is (0, 0), top-right of window is (1280, 720)),
// convert it to be in [-1, 1]^2, where y = -1 is the bottom and x = -1 is the left side.
vec2 cameraPos(vec2 worldPos) {
    // TODO
    return worldPos / vec2(1280.0, 720.0) * 2.0 - vec2(1.0, 1.0);
}

void main() {
    // output to gl_Position and to texCoordsOut.
    // In this case, using window.draw, SFML provides tex coords in gl_MultiTexCoord0.xy, and
    //  vertex positions in gl_Vertex.xy.
    // TODO
    gl_Position = vec4(cameraPos(gl_Vertex.xy), 0.0, 1.0);
    texCoordsOut = vec2(gl_MultiTexCoord0.x, 1.0 - gl_MultiTexCoord0.y);
}
