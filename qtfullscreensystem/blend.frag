uniform sampler2D desktop;
uniform sampler2D links;

void main()
{
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[0].xy);
  vec4 color = vec4(0,0,0,0);
  /*
  if(link_color.a != 0 )
    gl_FragColor = link_color;
  else
    gl_FragColor = desktop_color * 0.001 + vec4(0,1,0,1);
	*/

  if( link_color.a >= 0.001 )
  {
    float fac = min(2.0 * link_color.a, 1.0);
    color = fac * link_color + (1.0 - fac) * desktop_color;
  }
  
  color.a = 1.0;
  gl_FragColor = color;
}
/*
// blend
0.5 * (desktop + (1 - link.a) * screenshot) + 0.5 * link.a * link;

// normal
link.a * link + (1 - link.a) * desktop;
*/