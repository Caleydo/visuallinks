// http://code.google.com/p/jquery-debounce/source/browse/trunk/jquery.debounce.js
function debounce(fn, timeout, invokeAsap, ctx)
{
  if(arguments.length == 3 && typeof invokeAsap != 'boolean')
  {
    ctx = invokeAsap;
    invokeAsap = false;
  }
  var timer;

  return function()
  {
    var args = arguments;
    ctx = ctx || this;

    invokeAsap && !timer && fn.apply(ctx, args);

    clearTimeout(timer);
    timer = setTimeout(function(){
      invokeAsap || fn.apply(ctx, args);
      timer = null;
    }, timeout);
  };
}

function throttle(fn, timeout, ctx)
{
  var timer, args, needInvoke;

  return function()
  {
    args = arguments;
    needInvoke = true;
    ctx = ctx || this;

    timer || (function(){
      if(needInvoke)
      {
        fn.apply(ctx, args);
        needInvoke = false;
        timer = setTimeout(arguments.callee, timeout);
      }
      else
        timer = null;
    })();
  };
}

