#version 330

in float fragHeight;

out vec4 outColor;

uniform sampler1D u_texture;


void main() {
    float h = fragHeight  + 0.0;
    outColor = vec4(texture(u_texture, h).rgb, 1);

}
