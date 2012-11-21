/**
 *
 * @note Based on https://addons.mozilla.org/de/firefox/addon/screengrab-fix-version/
 */
function grab(size, region, offset, type = 0, req_id = 0)
{
  var canvas =
    document.createElementNS("http://www.w3.org/1999/xhtml", "html:canvas");

  canvas.width = size[0];
  canvas.height = size[1];
  canvas.style.width = canvas.style.maxwidth = canvas.width + "px";
  canvas.style.height = canvas.style.maxheight = canvas.height + "px";

  var context = canvas.getContext("2d");
  context.fillStyle = "#555";
  context.fillRect(0, 0, canvas.width, canvas.height);

  context.save();
  context.scale( canvas.width / region.width,
                 canvas.height / region.height );
  context.translate(offset[0], offset[1]);
  context.drawWindow(content.window, region.x, region.y, region.width, region.height, "#fff");
  context.restore();

  var image_data = context.getImageData(0, 0, canvas.width, canvas.height).data;
  var len = image_data.length;
  var bytearray = new Uint8Array(len + 2);
  
  bytearray[0] = type;   // type
  bytearray[1] = req_id; // id

  for( var i = 0; i < len; ++i )
    bytearray[i + 2] = image_data[i];

  return bytearray.buffer;
}
