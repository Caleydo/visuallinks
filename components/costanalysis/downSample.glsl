uniform sampler2D input0;
//todo: downsample should be fixed at compile time
uniform int samples;
uniform vec2 texincrease;

void main()
{
  int begin = - samples/2 + 1;
  int end = samples/2 + 1;

  vec4 res = vec4(0);

  vec2 sample_coord;  
  sample_coord.y = gl_TexCoord[0].y + begin*texincrease.y;
  for(int y = begin; y < end; ++y, sample_coord.y+=texincrease.y)
  {
    sample_coord.x = gl_TexCoord[0].x + begin*texincrease.x;
    for(int x = begin; x < end; ++x, sample_coord.x+=texincrease.x)
      res += texture2D(input0, sample_coord);
  }

  gl_FragColor = res/(samples*samples);
  //gl_FragColor = texture2D(input0, gl_TexCoord[0].xy);
}