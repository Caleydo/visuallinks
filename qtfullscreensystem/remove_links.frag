uniform sampler2D desktop;
uniform sampler2D links;
uniform vec2 offset; // offset for window position

void main()
{
  vec4 desktop_color = texture2D(desktop, gl_TexCoord[0].xy - offset);
  vec4 link_color = texture2D(links, gl_TexCoord[0].xy);
  
  if( gl_TexCoord[0].y > offset.y && link_color.rgb != vec3(0,0,0) )
    gl_FragColor = (desktop_color - 0.5 * link_color) / 0.5;
  else
    gl_FragColor = desktop_color;
}
