uniform sampler2D screenshot;
uniform sampler2D links;
uniform sampler2D desktop;

const 

void main()
{
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
  vec4 screenshot_color = texture2D(screenshot, gl_TexCoord[1].xy);
#if !USE_DESKTOP_BLEND
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[1].xy);
#endif
  vec4 color;
  float blend_alpha = 0.5;

  if( link_color.a >= 0.001 )
  {
#if !USE_DESKTOP_BLEND
    float fac = min(2 * link_color.a, 1);
    link_color = fac * link_color + (1 - fac) * desktop_color;
#else
    blend_alpha = link_color.a;
#endif               
    color = (screenshot_color - blend_alpha * link_color) / (1 - blend_alpha);
  }
  else
    color = screenshot_color;

  color.a = 1;
  gl_FragColor = color;
}