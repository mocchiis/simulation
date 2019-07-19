#version 330

layout(location = 0) in vec3 in_position;

out float fragHeight;

uniform mat4 u_mvpMat;

void main() {
    gl_Position = u_mvpMat * vec4(in_position, 1.0);
    fragHeight = in_position.z;
}
