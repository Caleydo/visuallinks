#version 130

uniform sampler2D xray;
uniform sampler2D links;
#if !USE_DESKTOP_BLEND
  uniform sampler2D desktop;
#endif
uniform vec2 mouse_pos;

void main()
{
  vec4 xray_color = texture2D(xray, gl_TexCoord[0].xy);
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
#if !USE_DESKTOP_BLEND
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[0].xy);
#endif
  vec4 color = vec4(0,0,0,0);

  color = 0.75 * xray_color + link_color;

#if !USE_DESKTOP_BLEND
  if( link_color.a >= 0.001 )
  {
    float fac = min(2.0 * link_color.a, 1.0);
    color = fac * link_color + (1.0 - fac) * desktop_color;
  }
  
  color.a = 1.0;
#endif
  vec2 tex_size = vec2(textureSize(links, 0));
  float d = length(mouse_pos - gl_TexCoord[0].xy * tex_size);
  color.r *= min(250.0 / d, 1.0);
  gl_FragColor = color;
}