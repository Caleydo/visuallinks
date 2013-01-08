/**
 *
 * @note Based on https://addons.mozilla.org/de/firefox/addon/screengrab-fix-version/
 */
function grab(size, region, req_id, sections)
{
  var canvas =
    document.createElementNS("http://www.w3.org/1999/xhtml", "html:canvas");

  canvas.width = size[0];
  canvas.height = size[1];
  canvas.style.width = canvas.style.maxwidth = canvas.width + "px";
  canvas.style.height = canvas.style.maxheight = canvas.height + "px";

  var context = canvas.getContext("2d");
  context.fillStyle = "#755";
  context.fillRect(0, 0, size[0], size[1]);
  context.scale( canvas.width / region.width,
                 canvas.height / region.height );

  for( var i = 0; i < sections[1].length; ++i )
  {
    if(    sections[1][i][1] <= region.y
        || sections[1][i][0] >= region.y + region.height )
      continue;

    var top = Math.max(region.y, sections[1][i][0]);
    var bottom = Math.min(region.y + region.height, sections[1][i][1]);
    var offset = Math.max(region.y - sections[1][i][0], 0);

    context.save();
    context.translate(0, top - region.y);
    context.drawWindow
    (
      content.window,
      region.x,
      sections[0][i][0] + offset,
      region.width,
      bottom - top,
      "#fff"
    );
    context.restore();
  }

  var image_data = context.getImageData(0, 0, canvas.width, canvas.height).data;
  var len = image_data.length;
  var bytearray = new Uint8Array(len + 2);
  
  bytearray[0] = 1;      // type
  bytearray[1] = req_id; // id

  for( var i = 0; i < len; ++i )
    bytearray[i + 2] = image_data[i];

  return bytearray.buffer;
}
