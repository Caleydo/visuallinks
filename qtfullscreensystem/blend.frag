uniform sampler2D desktop;
uniform sampler2D links;

void main()
{
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[1].xy);
  /*
  if(link_color.a != 0 )
    gl_FragColor = link_color;
  else
    gl_FragColor = desktop_color * 0.001 + vec4(0,1,0,1);
	*/

  if( link_color.a < 0.001 )
    gl_FragColor = vec4(0,0,0,0);
  else
  {
    float fac = min(2 * link_color.a, 1);
    gl_FragColor = fac * link_color + (1 - fac) * desktop_color;
  }

  gl_FragColor.a = 1;
}
/*
// blend
0.5 * (desktop + (1 - link.a) * screenshot) + 0.5 * link.a * link;

// normal
link.a * link + (1 - link.a) * desktop;
*/