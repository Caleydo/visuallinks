uniform sampler2D desktop;
uniform sampler2D links;

void main()
{
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[1].xy);

  if(link_color.a != 0 )
    gl_FragColor = (desktop_color - 0.6 * link_color) / 0.4;
  else
    gl_FragColor = desktop_color;

}
