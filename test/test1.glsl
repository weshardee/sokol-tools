@lib bla
/*
    this is a comment
*/
@type float float
@type vec2 hmm_vec2
@type vec3 hmm_vec3
@type vec4 hmm_vec4
@type mat4 hmm_mat4

@vs my_vs
uniform params {
    mat4 mvp;
};
uniform params2 {
    vec2 bla;
};

layout(location=0) in vec4 position;
layout(location=1) in vec2 texcoord0;
layout(location=2) in vec4 color0;

layout(location=0) out vec2 uv;
layout(location=1) out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = color0;
    color.xy += bla;
}
@end

// the fragment shader
@fs my_fs
uniform sampler2D tex0;
uniform sampler2D tex1;

layout(location=0) in vec2 uv;
layout(location=1) in vec4 color;

layout(location=0) out vec4 fragColor;

void main() {
    fragColor = texture(tex0, uv) * texture(tex1, uv*2.0) * color;
}
@end

/* and the program */
@program my_prog my_vs my_fs

