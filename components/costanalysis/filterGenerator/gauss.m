function filter = gauss(N, scale, twosided, normalized)
 % [filter] = gauss(N, scale, twosided, normalized)
  if(nargin < 4) 
      normalized = 0; 
  end
  if(nargin < 3) 
      twosided = 1; 
  end
  if(nargin < 2) 
      scale = 1; 
  end
  if(nargin < 1) 
      error('usage: [filter] = gauss(N, scale, doublesided, normalized)') 
  end

  if(N < 2)
    n = 0;
  else
    n = -1:(2/(N-1)):1;
  end
  
  filter = scale*exp(-(n*scale).^2);
  
  if(normalized)
    filter = filter/sum(filter);
  end
  
  if(~twosided)
      filter = filter(n>=0);
  end

  end
