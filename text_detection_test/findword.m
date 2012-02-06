function findword(image, text, threshold)

image = rgb2gray(image);
%tI = rgb2gray(bitmaptext(text,[],[1 1],struct('Color',[1 1 1 1],'FontSize',2)));
%tI = imcomplement(tI);
tI = text2image(text);
figure(1);
imshow(tI);

figure(3);
res = normxcorr2(tI,image);
imshow(res);
hold on;
maxv = max(max(res));
ids = (res > maxv*threshold);
xs = ones(size(res,1),1)*(1:size(res,2));
ys = (1:size(res,1))'*ones(1,size(res,2));
a = xs(ids);
b = ys(ids);
plot(a, b, 'ro');

figure(2);
imshow(image);
hold on;
plot(a, b, 'ro');

%[a b] = max(res);
%[maxval d] = max(a);
%pos = [d b(d)]
%plot(pos(1), pos(2), 'ro');
%plot(1:100, 1:100, 'ro');

end