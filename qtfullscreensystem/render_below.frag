uniform sampler2D inputTex;
uniform int fromInput;
uniform sampler2D desktop;
uniform sampler2D textinfo;
uniform sampler2D background;
uniform sampler2D foreground;
uniform vec2 gridTexStep;
uniform float distanceThreshold;
uniform float fade;
uniform float maxalpha;
uniform float belowalpha;
uniform vec2 inv_image_dimensions;

bool is_bg(vec4 bgcolors[9], vec4 color, float thres_sq);
float min_dist_to(vec4 bgcolors[9], vec4 color);
vec4 getBackgroundColor(vec2 coords);
void main()
{
  vec4 linkcolor;
  if(fromInput)
    linkcolor = texture2D(inputTex, gl_TexCoord[0].xy);
  else
    linkcolor = gl_Color;
  
  if(maxalpha >= 0)
    linkcolor.a = maxalpha;
  
  vec4 textinfo = texture2D(textinfo, gl_TexCoord[0].xy);
  
  if(textinfo.x > 0.5)
  {
    //there is text here
    
    //find distancies to surrounding bg colors
    
    /*vec4 bgcolors[9];
    int i = 0;
    for(int y = -1; y <= 1; ++y)
    for(int x = -1; x <= 1; ++x)
    {
      //calc dist to bg
      bgcolors[i++] = texture2D(background, gl_TexCoord[0].xy + vec2(x*gridTexStep.x, y*gridTexStep.y) );
    }*/
    if(fade == 0)
    {
      vec4 incolor = texture2D(desktop, gl_TexCoord[0].xy);
      //do not cover foreground
      //if(!is_bg(bgcolors, incolor, distanceThreshold*distanceThreshold) )
        linkcolor.a *= belowalpha;
    }
    else
    {
      //get distance to next foreground pix
      float color_dist_squ = distanceThreshold*distanceThreshold;
      float maxdist_sq = 2*fade*fade;
      float dist_sq = maxdist_sq;
      int ifade = ceil(fade);
      for(int y = -ifade; y <= ifade; ++y)
      for(int x = -ifade; x <= ifade; ++x)
      {
        vec4 incolor = getBackgroundColor(gl_TexCoord[0].xy + vec2(x*inv_image_dimensions.x, y*inv_image_dimensions.y));
        //if(!is_bg(bgcolors, incolor, color_dist_squ) )
          dist_sq = min(dist_sq, x*x+y*y); 
      }
      
      float alpha = 1;
      if(dist_sq < maxdist_sq)
        alpha = sqrt(dist_sq/maxdist_sq);
      linkcolor.a *= (belowalpha+alpha*(1-belowalpha));
    }
  }
  
  //draw the link
  //gl_FragColor = getBackgroundColor(gl_TexCoord[0].xy);// * 0.6 + 0.4 * linkcolor;
  //gl_FragColor = getBackgroundColor(gl_TexCoord[0].xy);// * 0.6 + 0.4 * linkcolor;
  //gl_FragColor.rg = gl_TexCoord[0].xy;
  gl_FragColor = texture2D(desktop, gl_TexCoord[0].xy);
  gl_FragColor.a = 1;
}
float min_dist_to(vec4 bgcolors[9], vec4 color)
{
  float mindist = 1000.0;
  for(int i = 0; i < 9; ++i)
  {
    vec3 dist3 = bgcolors[i].xyz - color.xyz;
    float thisdist = dot(dist3, dist3);
    mindist = min(mindist, thisdist);
  }
  return mindist;
}
bool is_bg(vec4 bgcolors[9], vec4 color, float thres_sq)
{
  float mindist = min_dist_to(bgcolors, color);
  return mindist <= thres_sq;
}
vec4 getBackgroundColor(vec2 coords)
{
  vec4 bg = texture2D(desktop, coords );
  /*vec4 fg = texture2D(foreground, coords );
  if(fg.x > 0.95 && fg.y > 0.95 && fg.z > 0.95)
    bg = fg;*/
  return bg;
}