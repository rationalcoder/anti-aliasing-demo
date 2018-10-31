#version 330

in vec3 v_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_eye;
in vec3 v_lightDir;
in float v_lightDist;

uniform vec4 u_pointLightC;
uniform float u_specularExp;

uniform bool u_hasNormalMap;
uniform bool u_hasSpecularMap;
uniform bool u_lit;
uniform bool u_solid;
uniform vec4 u_color;

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform sampler2D u_specular;

void main()
{
    if (u_lit) {
        vec4 diffuse = texture(u_diffuse, v_uv);
        vec3 ambient = .01 * diffuse.rgb;
        //vec3 ambient = vec3(.01, .01, .01);

        // TODO: normal mapping support
        vec3 n = normalize(v_normal);
        vec3 l = normalize(v_lightDir);
        vec3 e = normalize(v_eye);
        vec3 h = normalize(l + e);

        float nDotL = max(dot(n, l), 0.0);
        float hDotN = max(dot(h, n), 0.0);

        vec3 diffuseComponent  = nDotL * u_pointLightC.rgb * diffuse.rgb;
        vec3 specularComponent = float(nDotL > 0.0) * pow(hDotN, 20) * u_pointLightC.rgb * diffuse.rgb;

        float d  = v_lightDist;
        float d2 = d * d;
        float attenuation =  1.0 / (1.0 + .02*d + .1*d2);
        gl_FragColor = vec4(max(ambient, attenuation * (diffuseComponent + specularComponent)), diffuse.a);
    }
    else {
        gl_FragColor = u_solid ? u_color : texture(u_diffuse, v_uv);
    }
}
