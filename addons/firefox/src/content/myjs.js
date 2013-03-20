var debug = false;

var stopped = true;
var socket = null;
var status = '';

var last_id = null;
var last_stamp = null;
var offset = [0,0];
var scale = 1;
var win = null;
var menu = null;
var items_routing = null;
var active_routes = new Object();
var timeout = null;
var routing = null;
var tile_requests = null;
var tile_timeout = false;

var prefs = Components.classes["@mozilla.org/fuel/application;1"]
                      .getService(Components.interfaces.fuelIApplication)
                      .prefs;

/**
 * Get value of a preference
 */
function getPref(key)
{
  return prefs.get("extensions.vislinks." + key).value;
}

/**
 * Set status icon
 */
function setStatus(stat)
{
  document.getElementById('vislink').setAttribute('class', stat);
  status = stat;
}

/** Send data via the global WebSocket
 *
 */
function send(data)
{
  try
  {
    if( !socket )
      throw "No socket available!";
  
    socket.send(JSON.stringify(data));
  }
  catch(e)
  {
    alert(e);
    throw e;
  }
}

function removeAllChildren(el)
{
  while( el.hasChildNodes() )
    el.removeChild(el.firstChild);
}

/**
 * Update scale factor (CSS pixels to hardware pixels)
 */
function updateScale()
{
  win = content.document.defaultView;
  var domWindowUtils =
    win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
       .getInterface(Components.interfaces.nsIDOMWindowUtils);
  scale = domWindowUtils.screenPixelsPerCSSPixel;
}

/**
 * Get the document region relative to the application window
 */
function getRegion()
{
  updateScale();
  var win = content.document.defaultView;

  return [
    Math.round((win.mozInnerScreenX - win.screenX) * scale),
    Math.round((win.mozInnerScreenY - win.screenY) * scale),
    Math.round(win.innerWidth * scale),
    Math.round(win.innerHeight * scale)
  ];
}

function getScrollRegion()
{
  var doc = content.document;
  return {
    x: content.scrollX,
    y: content.scrollY,
    width: doc.documentElement.scrollWidth,
    height: doc.documentElement.scrollHeight
  };
}

//------------------------------------------------------------------------------
function onVisLinkButton()
{
  menu = document.getElementById("vislink_menu");
  items_routing = document.getElementById("routing-selector");

  if( status == 'active')
    selectVisLink();
  else
  {
    // start client
    stopped = false;
    if( register() )
    {
      window.addEventListener('unload', stopVisLinks, false);
      window.addEventListener('scroll', windowChanged, false);
      window.addEventListener("DOMAttrModified", attrModified, false);
      window.addEventListener('resize', resize, false);
//      window.addEventListener("DOMContentLoaded", windowChanged, false);
    }
  }
}

function getSelectionId()
{
  var txt = document.getElementById("vislink-search-text");
  var selid = txt.value.trim();
  if( selid == "" )
    selid =	content.getSelection().toString().trim();
  else
    txt.reset();

	return selid//.replace( /\s+/g, ' ' )
              .toLowerCase();
}

function onStandardSearchButton(backwards = false)
{
	window.content.find(
	  getSelectionId(),
	  false, // aCaseSensitive
	  backwards, // aBackwards
	  true,  // aWrapAround,
    false, // aWholeWord,
    true,  // aSearchInFrames
    false  // aShowDialog
  );
}

//------------------------------------------------------------------------------
// This function is triggered by the user if he want's to get something linked
function selectVisLink()
{
  var	selectionId = getSelectionId();
	if( selectionId == null || selectionId == "" )
	  return;
	window.localSelectionId = selectionId;

	reportVisLinks(selectionId);
}

//------------------------------------------------------------------------------
function removeRouteData(id)
{
  var route = active_routes[id];
  
  if( route )
  {
    menu.removeChild(route.menu_item);
    delete active_routes[id];
  }
}

//------------------------------------------------------------------------------
function onAbort(id, stamp, send_msg = true)
{
  // TODO
  last_id = null;
  last_stamp = null;
  
  // abort all
  if( id == '' && stamp == -1 )
  {
    //menu
    for(var route_id in active_routes)
      removeRouteData(route_id);
  }
  else
  {
    removeRouteData(id);
  }
  
  if( send_msg )
    send({
      'task': 'ABORT',
      'id': id,
      'stamp': stamp
    });
}

//------------------------------------------------------------------------------
function abortAll()
{
  onAbort('', -1);
}

//------------------------------------------------------------------------------
function removeAllRouteData()
{
  last_id = null;
  last_stamp = null;
  
  // menu
  for(var route_id in active_routes)
    removeRouteData(route_id);
}

//------------------------------------------------------------------------------
function reportVisLinks(id, found)
{
  if( debug )
    var start = Date.now();

  if( !found && getPref("replace-route") )
    abortAll();

  updateScale();
  
  offset[0] = win.mozInnerScreenX * scale;
  offset[1] = win.mozInnerScreenY * scale;
  
//  alert(offset[0] + "|" + offset[1]);

  var bbs = searchDocument(content.document, id);
  
  if( debug )
  {
    for(var i = 1; i < 10; i += 1)
      searchDocument(content.document, id);
    alert("time = " + (Date.now() - start) / 10);
  }

  last_id = id;
  if( !found )
  {
    d = new Date();
    last_stamp = (d.getHours() * 60 + d.getMinutes()) * 60 + d.getSeconds();
  }
  
  if( !active_routes[id] )
  {
    var item = document.createElement("menuitem");
    item.setAttribute("label", id);
    item.setAttribute("tooltiptext", "Remove routing for '"+id+"'");
    item.setAttribute("oncommand", "onAbort('"+id+"', "+last_stamp+")");
    menu.appendChild(item);
    
    active_routes[id] = {
      stamp: last_stamp,
      menu_item: item
    };
  }
  
  var reg = getScrollRegion();
  send({
    'task': (found ? 'FOUND' : 'INITIATE'),
    'title': document.title,
    'scroll-region': [reg.x, reg.y, reg.width, reg.height],
    'id': id,
    'stamp': last_stamp,
    'regions': bbs
  });
  //alert("time = " + (Date.now() - start) / 10);
// if( found )
//    alert('send FOUND: '+selectionId);
}

//------------------------------------------------------------------------------
function reportSelectRouting(routing)
{
  send({
    'task': 'SET',
    'id': '/routing',
    'val': routing
  });
}

//------------------------------------------------------------------------------
function windowChanged()
{
  if( timeout )
    clearTimeout(timeout);
  
  timeout = setTimeout(reroute, 500);
}

//------------------------------------------------------------------------------
function reroute()
{
  // trigger reroute
  for(var route_id in active_routes)
    reportVisLinks(route_id);  
}

//------------------------------------------------------------------------------
function stopVisLinks()
{
	stopped = true;
	setStatus('');
	window.removeEventListener('unload', stopVisLinks, false);
	window.removeEventListener('scroll', windowChanged, false);
	window.removeEventListener("DOMAttrModified", attrModified, false);
  window.removeEventListener('resize', resize, false);
}

//------------------------------------------------------------------------------
function register()
{
//	window.lastPointerID = null; 
//

	// Get the box object for the link button to get window handler from the
	// window at the position of the box
	var box = document.getElementById("vislink").boxObject;

	try
	{
	  tile_requests = new Queue();

      socket = new WebSocket('ws://localhost:4487', 'VLP');
      socket.binaryType = "arraybuffer";
      socket.onopen = function(event)
      {
        setStatus('active');
        send({
          task: 'REGISTER',
          name: "Firefox",
          pos: [box.screenX + box.width / 2, box.screenY + box.height / 2],
          region: getRegion()
        });
        send({
          task: 'GET',
          id: '/routing'
        });
      };
      socket.onclose = function(event)
      {
        setStatus(event.wasClean ? '' : 'error');
        removeAllRouteData();
      };
      socket.onerror = function(event)
      {
        setStatus('error');
        removeAllRouteData();
      };
      socket.onmessage = function(event)
      {
        var msg = JSON.parse(event.data);
        if( msg.task == 'REQUEST' )
        {
          //alert('id='+last_id+"|"+msg.id+"\nstamp="+last_stamp+"|"+msg.stamp);
          if( msg.id == last_id && msg.stamp == last_stamp )
            // already handled
            return;// alert('already handled...');
  
          last_id = msg.id;
          last_stamp = msg.stamp;
  
          setTimeout('reportVisLinks("'+msg.id+'", true)',0);
        }
        else if( msg.task == 'ABORT' )
        {
          onAbort(msg.id, msg.stamp, false);
        }
        else if( msg.task == 'GET-FOUND' )
        {
          if( msg.id == '/routing' )
          {
            removeAllChildren(items_routing);
            routing = msg.val;
            for(var router in msg.val.available)
            {
              var name = msg.val.available[router][0];
              var valid = msg.val.available[router][1];

              var item = document.createElement("menuitem");
              item.setAttribute("label", name);
              item.setAttribute("type", "radio");
              item.setAttribute("name", "routing-algorithm");
              item.setAttribute("tooltiptext", "Use '" + name + "' for routing.");

              // Mark available (Routers not able to route are disabled)
              if( !valid )
                item.setAttribute("disabled", true);
              else
                item.setAttribute("oncommand", "reportSelectRouting('"+name+"')");

              // Mark current router
              if( msg.val.active == name )
                item.setAttribute("checked", true);

              items_routing.appendChild(item);
            }
          }
        }
        else if( msg.task == 'GET' )
        {
          if( msg.id == 'preview-tile' )
          {
            tile_requests.enqueue(msg);
            if( !tile_timeout )
            {
              setTimeout('handleTileRequest()', 50);
              tile_timeout = true;
            }
          }
          else
            Application.console.warn("Unknown GET request: " + event.data);
        }
        else if( msg.task == 'SET' )
        {
          if( msg.id == 'scroll-y' )
            content.scrollTo(content.scrollX, msg.val);
        }
        else
          alert(event.data);
      }
	}
	catch (err)
	{
		alert("Could not establish connection to visdaemon. "+err);
		stopped = true;
		return false;
	}
	return true;
}

function handleTileRequest()
{
  if( tile_requests.isEmpty() )
  {
    tile_timeout = false;
    return;
  }
  
  var req = tile_requests.dequeue();
  var mapping = [req.sections_src, req.sections_dest];
  socket.send( grab(req.size, req.src, req.req_id, mapping) );
  
  // We need to wait a bit before sending the next tile to not congest the
  // receiver queue.
  setTimeout('handleTileRequest()', 200);
  tile_timeout = true;
}

//------------------------------------------------------------------------------
function attrModified(e)
{
  if( e.attrName == "screenX" || e.attrName == "screenY" )
    windowChanged();
}

//------------------------------------------------------------------------------
function resize()
{
  send({task: 'RESIZE', region: getRegion()});
	//register();
	//windowChanged();
}

//------------------------------------------------------------------------------
function getMapAssociatedImage(node)
{
	// step through previous siblings and find image associated with map 
	var sibling = node; 
	while(sibling = sibling.previousSibling)
	{
		//alert(sibling.nodeName); 
		if(sibling.nodeName == "IMG")
			return sibling; 
	}
}

//------------------------------------------------------------------------------
function searchAreaTitles(doc, id)
{
	// find are node which title contains id 
	var	areanodes =	doc.evaluate("//area[contains(@title,'"+id+"')]", doc, null, XPathResult.ANY_TYPE, null);
	
	// copy found nodes to results array 
	var	result = new Array();
	try {  
   		var thisNode = areanodes.iterateNext();  
     
   		while (thisNode) {  
   		    //sourceString = thisNode.getAttribute('title'); 
   		    //coordsString = thisNode.getAttribute('coords'); 
     		//alert( sourceString + " - coords: " + coordsString + " - node name: " + thisNode.parentNode.nodeName );  
     		result[result.length] =	thisNode;
     		thisNode = areanodes.iterateNext();  
   		}   
 	}  
 	catch (e) {  
   		alert( 'Error: Document tree modified during iteration ' + e );  
 	} 
 	
 // create bounding box array 
	var	bbs	= new Array();
	for(var	i=0; i<result.length; i++) {
	
		// find image associated with area element 
		var	r =	result[i];
		var img = getMapAssociatedImage(r.parentNode); 
		if(img != null){
			
			// find bounding box by interpreting the area's coord attribute and the image outline 
			var bb = findAreaBoundingBox(doc, img, r.getAttribute('coords')); 
			//var	bb = findObjectBoundingBox(imgArray[0]);
			if (bb != null)	{
				bbs[bbs.length]	= bb;
			}
		}
		
	}
	
	//alert("area bounding boxes: " + bbs.length); 
	
	return bbs;
}

//------------------------------------------------------------------------------
function searchDocument(doc, id) {
	var	textnodes =	doc.evaluate("//body//*/text()", doc, null,	XPathResult.ANY_TYPE, null);
	var	result = new Array();
	while(node = textnodes.iterateNext()) {
		var	s =	node.nodeValue;
		var	i =	s.toLowerCase().indexOf(id);
		if (i != -1) {
			result[result.length] =	node.parentNode;
		}
	}
//	  alert("hits: " + result.length);

	var	bbs	= new Array();
	for(var	i=0; i<result.length; i++) {
		var	r =	result[i];
		for	(var j=0; j<r.childNodes.length; j++) {
			currentNode	= r.childNodes[j];
			sourceString = currentNode.nodeValue;
			if (sourceString !=	null) {
				var	idx	= sourceString.toLowerCase().indexOf(id);
				while (idx >= 0) {
					var	s1 = sourceString.substring(0, idx);
					var	s2 = sourceString.substring(idx, idx + id.length);
					var	s3 = sourceString.substr(idx + id.length);
					
					var	d2 = doc.createElement("SPAN");
					d2.style.outline = "2px	solid red";
					var	t2 = doc.createTextNode(s2);
					d2.appendChild(t2);
					var	t3 = doc.createTextNode(s3);
					
					currentNode.nodeValue =	s1;
					var	nextSib	= currentNode.nextSibling
					if (nextSib	== null) {
						r.appendChild(d2);
						r.appendChild(t3);
					} else {
						r.insertBefore(d2, nextSib);
						r.insertBefore(t3, nextSib);
					}
					
					var	bb = findBoundingBox(doc, d2);
					if (bb != null)	{
						bbs[bbs.length]	= bb;
					}
					
					r.removeChild(t3);
					r.removeChild(d2);
					currentNode.nodeValue =	sourceString;
					
					var	idx	= sourceString.toLowerCase().indexOf(id, id.length + idx);
				};
			}
		}
	}
	
	// find area bounding boxes 
	var areaBBS = searchAreaTitles(doc, id); 
	bbs = bbs.concat(areaBBS); 
	
	return bbs;
}

//------------------------------------------------------------------------------
function findAreaBoundingBox(doc, img, areaCoords){

	var coords = new Array(); 
	
	// parse the coords attribute, separated by comma
	var sepIndex = areaCoords.indexOf(','); 
	var counter = 0; 
	while(sepIndex >= 0){
		var numString = areaCoords; 
		numString = numString.substring(0, sepIndex); 
		coords[coords.length] = parseInt(numString); 
		areaCoords = areaCoords.substring(sepIndex + 1, areaCoords.length); 
		//alert("numString: " + numString + " - coords: " + areaCoords + " sepIndex: " + sepIndex + ", length: " + areaCoords.length);  
		sepIndex = areaCoords.indexOf(','); 
		counter = counter + 1; 
	}
	
	// last element
	coords[coords.length] = parseInt(areaCoords); 
	
	//alert(coords.length + " coords found - last: " + coords[coords.length-1]); 
	
	var x = 0; 
	var y = 0; 
	var w = 0; 
	var h = 0; 
	
	if(coords.length == 3){
		// 3 coords elements: circle: x, y, radius
		x = coords[0]; 
		y = coords[1]; 
		w = coords[2]; 
		h = coords[2]; 
	}
	else{if(coords.length == 4){
		// 4 elements: rectangle: x1, y1, x2, y2 
		x = coords[0]; 
		y = coords[1]; 
		w = coords[2] - x; 
		h = coords[3] - y; 
	}
	else{if(coords.length > 4){
		// more than 4 elements: polygon 
		var minX = coords[0]; 
		var minY = coords[1]; 
		var maxX = coords[0]; 
		var maxY = coords[1]; 
		for(var i = 2; i <= coords.length; i=i+2){
			var curX = coords[i]; 
			var curY = coords[i+1]; 
			if(curX < minX) minX = curX; 
			if(curX > maxX) maxX = curX; 
			if(curY < minY) minY = curY; 
			if(curY > maxY) maxY = curY; 
		}
		x = minX; 
		y = minY; 
		w = maxX - minX; 
		h = maxY - minY; 
	};};}; // funny syntax.. 
	
	if(w < 10) w = 10; 
	if(h < 10) h = 10; 
	
	//alert("Area: " + x + ", " + y + ", " + w + ", " + h); 
	
	// find the image's bounding box 
	ret = findBoundingBox(doc, img); 
	
	// add x, y to image bounding box and set to area's width / height 
	ret.x += x; 
	ret.y += y; 
	ret.width = w; 
	ret.height = h; 
	
	//alert("Area bounding box: " + ret.x + ", " + ret.y + "  [" + ret.width + " x " + ret.height + "]"); 
	
	return ret; 
}

//------------------------------------------------------------------------------
function findBoundingBox(doc, obj)
{
  var w = obj.offsetWidth;
  var h = obj.offsetHeight;
  var curleft = -1;
  var curtop = -1;
  
  if( w == 0 || h == 0 )
    return null;
  
  w += 2;
  h += 1;
  
  if( obj.offsetParent )
  {
  	do
  	{
  	  curleft += obj.offsetLeft;
  	  curtop  += obj.offsetTop;
  	} while( obj = obj.offsetParent );
  }
  
  x = curleft - win.pageXOffset;     
  y = curtop - win.pageYOffset;
  
  var outside = false;

  // check if	visible
  if(    (x + 0.5 * w < 0) || (x + 0.5 * w > win.innerWidth)
      || (y + 0.5 * h < 0) || (y + 0.5 * h > win.innerHeight) )
    outside = true;
  
  x *= scale;
  y *= scale;
  w *= scale;
  h *= scale;

  x += offset[0];
  y += offset[1];
  
  return [ [x,     y],
           [x + w, y],
           [x + w, y + h],
           [x,     y + h],
           {outside: outside} ];
}
