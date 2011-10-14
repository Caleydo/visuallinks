uniform sampler2D desktop;

void main()
{
  //draw the link
  //gl_FragColor = getBackgroundColor(gl_TexCoord[0].xy);// * 0.6 + 0.4 * linkcolor;
  //gl_FragColor = getBackgroundColor(gl_TexCoord[0].xy);// * 0.6 + 0.4 * linkcolor;
  //gl_FragColor.rg = gl_TexCoord[0].xy;
  gl_FragColor = texture2D(desktop, gl_TexCoord[0].xy) * 0.6 + gl_Color * 0.4;
  gl_FragColor.a = 1.0;
}