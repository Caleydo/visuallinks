uniform sampler2D inputTex;
uniform float scale;

const int size = 4;
//float weights[3] = float[](8/16.0, 3.5/16.0, 0.5/16.0);
//float weights[3] = float[](6/16.0, 4/16.0, 1/16.0);
float weights[4] = float[](0.383, 0.242, 0.061, 0.006);

void main()
{
  vec2 dims_inv = scale/textureSize(inputTex, 0);
  vec4 color;
  for(int i = -size + 1; i < size; ++i)
    color += weights[abs(i)] * texture2D(inputTex, gl_TexCoord[0].xy + vec2(0,i) * dims_inv);

  if( color.a > 0.0001 )
    color.rgb /= color.a;

  gl_FragColor = color;
}