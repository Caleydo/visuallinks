uniform vec2 inv_image_dimensions;

void main()
{
  gl_TexCoord[0].xy = gl_Vertex.xy * inv_image_dimensions;
  gl_Position = ftransform();
  gl_FrontColor = gl_Color;
}