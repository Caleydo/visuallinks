
uniform sampler2D input0;
uniform sampler2D input1;
uniform int step;
uniform vec2 texincrease;
uniform float scalesaliency;

#define RESULTFUNCTION(a) a
//nonlinear resultconversion
//#define RESULTFUNCTION(a) min(a,1.0)
//combination function if two filters are used
//#define COMPFUNCTION(a,b) 0.5*(a.x+a.y+a.z+b.x+b.y+b.z)
//#define COMPFUNCITON(a,b) (a.x+a.y+a.z+b.x+b.y+b.z)
#define COMPFUNCITON(a,b) (max(a.x,b.x)+max(a.y,b.y)+max(a.z,b.z))
#if 0
const int samples = 5;
const float gauss_a[] = {0.9999590358044f,0.0000204820978f,0.0000000000000f,0.0000000000000f,0.0000000000000f };
const float gauss_b[] = {0.1989179613526f,0.1768569944064f,0.1242988073553f,0.0690570725796f,0.0303281449824f };
const float scale = 10.0f;
const int increase = 1;
#elif 0
//default
const int samples = 25;
const float gauss_a[] = {0.71999999999999997335, 0.07588744168454231165, 0.00008885505894240928, 0.00000000115576419973, 0.00000000000000016701, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.07119999999999999940, 0.06983633545029049872, 0.06590005675867093016, 0.05982642232441025276, 0.05225203073952692373, 0.04390522553835680269, 0.03549213695749796338, 0.02760266295682271137, 0.02065250394387213237, 0.01486611001934083284, 0.01029496486644911057, 0.00685891297523706905, 0.00439631309152829428, 0.00271097075590983824, 0.00160828847218672790, 0.00091792238901865285, 0.00050402360253430493, 0.00026625555841820574, 0.00013531607413451056, 0.00006616114203257432, 0.00003112142643624747, 0.00001408376900247840, 0.00000613170411217301, 0.00000256830373574763, 0.00000103493833724356, 0.00000040122267000040, 0.00000014964401433797, 0.00000005369528278094, 0.00000001853599122788, 0.00000000615599692354, 0.00000000196690720179, 0.00000000060460570994};
const float scale = 10;
const int increase = 1;
#elif 0
//normalized
const int samples = 32;
const float gauss_a[] = {0.71999999999999997335, 0.41024363380626455156, 0.07588744168454231165, 0.00455739510778973775, 0.00008885505894240928, 0.00000056242723739792, 0.00000000115576419973, 0.00000000000077106473, 0.00000000000000016701, 0.00000000000000000001, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.06687999586433583665, 0.06655743853427009693, 0.06559907059123312734, 0.06403235832400896999, 0.06190162252077820865, 0.05926595572954697355, 0.05619650109037312852, 0.05277326750980775721, 0.04908167977194816817, 0.04520907016560394426, 0.04124131042736003949, 0.03725976078604802816, 0.03333867939513911194, 0.02954319406840017620, 0.02592789303928312930, 0.02253604648930829235, 0.01939942947126892941, 0.01653868249869023047, 0.01396412045803759050, 0.01167688457992625481, 0.00967032595072470774, 0.00793151157729321370, 0.00644275381204627209, 0.00518307907960499550, 0.00412957024409744099, 0.00325853661286647006, 0.00254648473235372704, 0.00197088040946762034, 0.00151070683101836166, 0.00114683473351219023, 0.00086222816827737038, 0.00064201370684951293};
const float scale = 3;
const int increase = 1;
#elif 1
//wide
const int samples = 25;
const float gauss_a[] = {0.52891368894825740998, 0.21962444284528823135, 0.01572418950834420306, 0.00019410986023261906, 0.00000041316036791266, 0.00000000015162871173, 0.00000000000000959480, 0.00000000000000000010, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.03867493709904590987, 0.03850074569868817964, 0.03798286471346590254, 0.03713516367406158314, 0.03598007120582619872, 0.03454758904483667264, 0.03287399778594201488, 0.03100033225724211847, 0.02897071580766412413, 0.02683064749528345921, 0.02462533409890925451, 0.02239815055969986493, 0.02018929891916242114, 0.01803471846318258423, 0.01596528024037944735, 0.01400627908171355081, 0.01217721728265986313, 0.01049185755001950417, 0.00895850964989263095, 0.00758050601220125724, 0.00635681653711792690, 0.00528275182618551109, 0.00435070653797517588, 0.00355089983992623722, 0.00287207716844150012};
const float scale = 15;
const int increase = 1;
#elif 0
//wide trimmed bit
const int samples = 20;
const float gauss_a[] = {0.52891368894825740998, 0.21962444284528823135, 0.01572418950834420306, 0.00019410986023261906, 0.00000041316036791266, 0.00000000015162871173, 0.00000000000000959480, 0.00000000000000000010, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.04048996046654237263, 0.04030759422516922569, 0.03976540897056046048, 0.03887792513354668134, 0.03766862391983622632, 0.03616891505362778564, 0.03441678178612553318, 0.03245518471900441654, 0.03033031791973340996, 0.02808981572726492598, 0.02578100648455433630, 0.02344930072835940682, 0.02113678719090300584, 0.01888109179672069898, 0.01671453437958354826, 0.01466359686247379820, 0.01274869678791430705, 0.01098424274958584995, 0.00937893449275291335, 0.00793626083901213634};
const float scale = 15;
const int increase = 1;
#elif 0
//wide trimmed strong
const int samples = 15;
const float gauss_a[] = {0.52891368894825740998, 0.21962444284528823135, 0.01572418950834420306, 0.00019410986023261906, 0.00000041316036791266, 0.00000000015162871173, 0.00000000000000959480, 0.00000000000000000010, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.04556721768094781910, 0.04536198354086577733, 0.04475181071689939005, 0.04375304043598133436, 0.04239209834040896102, 0.04070433279119248238, 0.03873248996678965717, 0.03652491753328043766, 0.03413360208446915983, 0.03161215108914863098, 0.02901382764960584379, 0.02638973657774003681, 0.02378724434170815388, 0.02124869498615740079, 0.01881046110527875154};
const float scale = 15;
const int increase = 1;
#elif 0
const int samples = 21;
const float gauss_a[] = {0.21940706026857187405, 0.18861240823543559686, 0.11981998070369360987, 0.05625065109869652191, 0.01951484856739291868, 0.00500313406808613331, 0.00094789104266722909, 0.00013271316949624361, 0.00001373121434505601, 0.00000104988854004704, 0.00000005932213424076, 0.00000000247702085302, 0.00000000007643317169, 0.00000000000174290383, 0.00000000000002937002, 0.00000000000000036574, 0.00000000000000000337, 0.00000000000000000002, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.04565367883866663229, 0.04536844551112508700, 0.04452339348222435211, 0.04314980686910588337, 0.04129768371199078747, 0.03903271590907497063, 0.03643242561550659453, 0.03358177460290001637, 0.03056859212198161091, 0.02747916116828162669, 0.02439426541266179888, 0.02138593573383281224, 0.01851505512573524068, 0.01582989378178211240, 0.01336556220183371564, 0.01114429760715768616, 0.00917644383040898230, 0.00746195042119256972, 0.00599220340489806276, 0.00475200593308997086, 0.00372154813588288164};
const float scale = 15;
const int increase = 1;
#elif 0
const int samples = 17;
const float gauss_a[] = {0.23507899314489841269, 0.19761298319677125801, 0.11738711568902195082, 0.04927523384318571037, 0.01461639466436865514, 0.00306376276074623536, 0.00045380921465681934, 0.00004750012930470627, 0.00000351333513261534, 0.00000018363170650663, 0.00000000678233233871, 0.00000000017701650755, 0.00000000000326476386, 0.00000000000004254937, 0.00000000000000039187, 0.00000000000000000255, 0.00000000000000000001};
const float gauss_b[] = {0.04896035958691687812, 0.04863269740049515344, 0.04766280931138274640, 0.04608912190956264421, 0.04397286376629042093, 0.04139411344067647769, 0.03844677708760704804, 0.03523293420122213665, 0.03185702415308023366, 0.02842032995846117960, 0.02501615422398323699, 0.02172598595756121409, 0.01861683982834127712, 0.01573982679586343261, 0.01312990109478628063, 0.01080663490156771184, 0.00877580617566048847};
#define COMPENSATIONFILTER 0
const int comp_samples = 7;
const float gauss_comp_a[] = {0.45135148562989646503, 0.23799421895321939968, 0.03489160944531692621, 0.00142225890134349987, 0.00001611904774182925, 0.00000005079291829516, 0.00000000004450102489};
const float gauss_comp_b[] = {0.21167434930884193589, 0.18390586170416223233, 0.12060840867227204387, 0.05970549176391969776, 0.02231031227678914250, 0.00629291253400191420, 0.00133983839442318366};
//const float gauss_comp_a[] = {0.90425993963844442103, 0.04786293248270390849, 0.00000709769512507993, 0.00000000000294881415, 0.00000000000000000000, 0.00000000000000000000, 0.0};
//const float gauss_comp_b[] = {0.24696787615294649165, 0.20394534108986625554, 0.11485072849286301344, 0.04410627321497819636, 0.01155084127651780958, 0.00206287784930146972, 0.0};
//
const float scale = 15;
const int increase = 1;
#elif 1
const int samples = 17;
const float gauss_a[] = {0.36045445613276960728, 0.23965169565342483993, 0.07043213091494081313, 0.00915000570108176654, 0.00052545087046489396, 0.00001333837986627986, 0.00000014966982884440, 0.00000000074237896365, 0.00000000000162771088, 0.00000000000000157757, 0.00000000000000000068, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000, 0.00000000000000000000};
const float gauss_b[] = {0.04896035958691691975, 0.04863269740049518813, 0.04766280931138278110, 0.04608912190956268584, 0.04397286376629045562, 0.04139411344067651238, 0.03844677708760708273, 0.03523293420122217134, 0.03185702415308026142, 0.02842032995846120388, 0.02501615422398325780, 0.02172598595756123144, 0.01861683982834129447, 0.01573982679586344302, 0.01312990109478629278, 0.01080663490156772051, 0.00877580617566049541};
const float scale = 9;
const int increase = 1;
#endif


#if COMPENSATIONFILTER
uniform sampler2D input2;
uniform sampler2D input3;
#endif

void main()
{
  vec4 resa = vec4(0,0,0,0);
  vec4 resb = vec4(0,0,0,0);
  #if COMPENSATIONFILTER
  vec4 comp_resa = vec4(0,0,0,0);
  vec4 comp_resb = vec4(0,0,0,0);
  #endif
  if(step == 0)
  {
    vec2 sample_coord = gl_TexCoord[0].xy + vec2((-samples+1)*texincrease.x,0);
    for(int x = -samples+1; x < samples; x+=increase, sample_coord.x+=texincrease.x)
    {
      vec4 sample = texture2D(input0, sample_coord);
      int id = x;
      if(id < 0) id = -id;
      resa += gauss_a[id]*sample;
      resb += gauss_b[id]*sample;
      #if COMPENSATIONFILTER
      if(id < comp_samples)
      {
	comp_resa += gauss_comp_a[id]*sample;
	comp_resb += gauss_comp_b[id]*sample;
      }	
      #endif
    }
    gl_FragData[0].rgba = resa;
    gl_FragData[1].rgba = resb;
    #if COMPENSATIONFILTER
    gl_FragData[2].rgba = comp_resa;
    gl_FragData[3].rgba = comp_resb;
    #endif
  }
  else
  {
    vec2 sample_coord = gl_TexCoord[0].xy + vec2(0,(-samples+1)*texincrease.y);
    for(int y = -samples+1; y < samples; y+=increase, sample_coord.y+=texincrease.y)
    {
      int id = y;
      if(id < 0) id = -id;
      resa += gauss_a[id]*texture2D(input0, sample_coord);
      resb += gauss_b[id]*texture2D(input1, sample_coord);
      #if COMPENSATIONFILTER
      if(id < comp_samples)
      {
	comp_resa += gauss_comp_a[id]*texture2D(input2, sample_coord);
	comp_resb += gauss_comp_b[id]*texture2D(input3, sample_coord);
      }	
      #endif
    }
    
    #if COMPENSATIONFILTER
    vec4 result = abs(resa - resb);
    vec4 comp_result = abs(comp_resa - comp_resb);
    float saliency = RESULTFUNCTION(scale*0.333333333*(COMPFUNCITON(result,comp_result)));
    //debug
    //gl_FragData[0].rgba = vec4(scale*0.33*(result.x+result.y+result.z), scale*0.33*(comp_result.x+comp_result.y+comp_result.z), 0.0, 1.0);
    gl_FragData[0].rgba = scalesaliency*0.005*vec4(saliency, saliency, saliency, 1.0);
    #else
    vec4 result = resa - resb;
    float saliency = RESULTFUNCTION(scale*0.333333333*(abs(result.x) + abs(result.y) + abs(result.z)));
    gl_FragData[0].rgba = scalesaliency*0.005*vec4(saliency, saliency, saliency, 1.0);
    #endif
    
    gl_FragData[0].a = 1;// = texture2D(input1, gl_TexCoord[0].xy);
  } 
}
