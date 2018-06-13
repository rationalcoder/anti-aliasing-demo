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

uniform sampler2D u_diffuse;
uniform sampler2D u_normal;
uniform sampler2D u_specular;

void main()
{
    if (u_lit) {
        vec4 diffuse = texture(u_diffuse, v_uv);
        vec3 ambient = .1 * diffuse.rgb;
        //vec3 ambient = vec3(.01, .01, .01);

        vec3 n = u_hasNormalMap ? texture(u_normal, v_uv).xyz : normalize(v_normal);
        vec3 l = normalize(v_lightDir);
        vec3 e = normalize(v_eye);
        vec3 h = normalize(l + e);

        float nDotL = max(dot(n, l), 0.0);
        float hDotN = max(dot(h, n), 0.0);

        vec3 diffuseComponent  = nDotL * u_pointLightC.rgb * diffuse.rgb;
        vec3 specularComponent = float(nDotL > 0.0) * pow(hDotN, 20) * u_pointLightC.rgb * diffuse.rgb;

        float d  = v_lightDist;
        float d2 = d * d;
        float attenuation = 1.0; //1.0 / (1.0 + .02*d);
        gl_FragColor = vec4(max(ambient, attenuation * (diffuseComponent + specularComponent)), diffuse.a);
    }
    else {
        gl_FragColor = texture(u_diffuse, v_uv);
    }
}
