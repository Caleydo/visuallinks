function c = add_filters(a, b)
  maxlen = max(length(a), length(b));
  c = zeros(1,maxlen);
  c(1:length(a)) = a;
  c(1:length(b)) = c(1:length(b)) + b;
end
