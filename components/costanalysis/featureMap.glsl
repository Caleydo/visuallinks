uniform sampler2D input0;

void main()
{
	vec3 XYZ;
	vec3 CIELab;
	vec3 tmpXYZ;
	vec3 refwhite=vec3(95.047, 100.000, 108.883);
	mat3 MATsRGB2XYZ = mat3 ( 	0.4124564,  0.3575761,  0.1804375,
	 							0.2126729,  0.7151522,  0.0721750,
	 							0.0193339,  0.1191920,  0.9503041 );

 	// Retreieve values
	vec3 RGB = texture2D(input0, gl_TexCoord[0].xy).rgb;
	
	 //encode that pixel has been RED only
	float a = 1.0;
	if(RGB.r == 1.0 && RGB.g == 0 && RGB.b == 0)
	  a = 0.0;

	
	// RGB to XYZ
	if ( RGB.r > 0.04045 ) RGB.r = pow( RGB.r * 0.94786729857819905213270142180095 + 0.05213270142180094786729857819905, 2.4);
	else                   RGB.r = RGB.r * 0.07739938080495356037151702786378;
	if ( RGB.g > 0.04045 ) RGB.g = pow( RGB.g * 0.94786729857819905213270142180095 + 0.05213270142180094786729857819905, 2.4);
	else                   RGB.g = RGB.g * 0.07739938080495356037151702786378;
	if ( RGB.b > 0.04045 ) RGB.b = pow( RGB.b * 0.94786729857819905213270142180095 + 0.05213270142180094786729857819905, 2.4);
	else                   RGB.b = RGB.b * 0.07739938080495356037151702786378;
	RGB*=100.0;

	// XYZ 2 LAB
	//Observer. = 2°, Illuminant = D65
	XYZ = RGB*MATsRGB2XYZ;
	tmpXYZ=XYZ/refwhite;
	if ( tmpXYZ.x > 0.008856 )	tmpXYZ.x = pow(tmpXYZ.x, ( 0.33333333333333333333333333333333 ));
	else                    	tmpXYZ.x = ( 7.787 * tmpXYZ.x ) + ( 0.13793103448275862068965517241379 );
	if ( tmpXYZ.y > 0.008856 )	tmpXYZ.y = pow(tmpXYZ.y, ( 0.33333333333333333333333333333333 ));
	else                    	tmpXYZ.y = ( 7.787 * tmpXYZ.y ) + ( 0.13793103448275862068965517241379 );
	if ( tmpXYZ.z > 0.008856 )	tmpXYZ.z = pow(tmpXYZ.z, ( 0.33333333333333333333333333333333 ));
	else                    	tmpXYZ.z = ( 7.787 * tmpXYZ.z ) + ( 0.13793103448275862068965517241379 );
	CIELab.x = ( 116.0 * tmpXYZ.y ) - 16.0;
	CIELab.y = 500.0 * ( tmpXYZ.x - tmpXYZ.y );
	CIELab.z = 200.0 * ( tmpXYZ.y - tmpXYZ.z );

	// Remember that we want gray to be the neutral
	CIELab.x-=50.0;

  	gl_FragColor.rgb = CIELab;
  	//gl_FragColor.a = 1.0;
	gl_FragColor.a = a;
}
