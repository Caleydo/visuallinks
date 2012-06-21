
scale = 1.5;
maxexp = 6;
printselection = 17;
print = 1;

%gaussian generation
g{1} = 1;
for i = 1:(maxexp)
  g{i+1} = gauss(2^i+1,scale,0,1);
end

%logic
if maxexp == 6
onesided = add_filters(2*g{1}, 2*g{2});
onesided = add_filters(onesided, 2*g{3});
onesided = add_filters(onesided, -g{4});
onesided = add_filters(onesided, -2*g{5});
onesided = add_filters(onesided, -2*g{6});
onesided = add_filters(onesided, -g{7});
elseif maxexp == 7
onesided = add_filters(2*g{1}, 2*g{2});
onesided = add_filters(onesided, 2*g{3});
onesided = add_filters(onesided, g{4});
onesided = add_filters(onesided, -g{5});
onesided = add_filters(onesided, -2*g{6});
onesided = add_filters(onesided, -2*g{7});
onesided = add_filters(onesided, -2*g{8});
end

filter = 1/maxexp*onesided([end:-1:2,1:end]);
x = (-length(onesided)+1):1:(length(onesided)-1);

figure(1);
plot(x,filter,'b');


%previous generation
s{1} = 1;
for i = 1:(maxexp)
  elements = 2^i;
  s{i+1} = ones(1,elements/2)./elements;
end

%logic
sonesided = add_filters(2*s{1}, 2*s{2});
sonesided = add_filters(sonesided, 2*s{3});
sonesided = add_filters(sonesided, -s{4});
sonesided = add_filters(sonesided, -2*s{5});
sonesided = add_filters(sonesided, -2*s{6});
sonesided = add_filters(sonesided, -s{7});

sfilter = 1/maxexp*sonesided([end:-1:2,1:end]);
sx = (-length(sonesided)+1):1:(length(sonesided)-1);

hold on
plot(sx,sfilter,'r');

%standard:
printselection = 9;
a = 0.015*gauss(2^maxexp+1,48,0,0);
b = 0.016*gauss(2^maxexp+1,4.45,0,0);
%wide
%a = gauss(2^maxexp+1,30,0,1);
%b = gauss(2^maxexp+1,2.15,0,0);
%smooth
%printselection = 17;
%a = gauss(2^maxexp+9,15,0,1);
%b = gauss(2^maxexp+9,2.95,0,0);
%compensation filter for smooth
%printselection = 6;
%a = gauss(15,12,0,1);
%b = gauss(2^maxexp+1,14,0,0);

%printselection = 17;
%a = gauss(2^maxexp+9,23,0,1);
%b = gauss(2^maxexp+9,2.95,0,0);

a = a(1:printselection);
b = b(1:printselection);
a_twosided = a([end:-1:2,1:end]);
b_twosided = b([end:-1:2,1:end]);
scaleb = sum(a_twosided)/sum(b_twosided);
b = scaleb*b;


comb_onesided = add_filters(a,-b);
comb_filter = comb_onesided([end:-1:2,1:end]);
comb_x = (-length(comb_onesided)+1):1:(length(comb_onesided)-1);
plot(comb_x,comb_filter,'g');

diff_onesided = add_filters(1/maxexp*onesided,-comb_onesided);
diff = diff_onesided([end:-1:2,1:end]);
diff_x = (-length(diff_onesided)+1):1:(length(diff_onesided)-1);
plot(diff_x,abs(diff),'k');



%a = 0.015*gauss(2^maxexp+1,48,0,0);
%b = 0.028*gauss(2^maxexp+1,2.15,0,0);
%comb_onesided = add_filters(a,-b);
%comb_filter = comb_onesided([end:-1:2,1:end]);
%comb_x = (-length(comb_onesided)+1):1:(length(comb_onesided)-1);
%plot(comb_x,comb_filter,'y');

hold off


if print
  fprintf('gauss_a[] = {');
  for i = 1:(printselection-1)
    fprintf('%.20f, ', a(i));
  end
  fprintf('%.20f};\n', a(printselection));
  fprintf('gauss_b[] = {');
  for i = 1:(printselection-1)
    fprintf('%.20f, ', b(i));
  end
  fprintf('%.20f};\n', b(printselection));
  offset = sum(comb_x);
  fprintf('offset = %f;\n',offset);
end




% c = gauss(7,2,0,1);
% for i = 1:4
%   fprintf('%.10f, ', c(i));
% end
% fprintf('\n');