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
    gl_FragColor = link_color.a * link_color + (1 - link_color.a) * desktop_color;

  gl_FragColor.a = 1;
}
