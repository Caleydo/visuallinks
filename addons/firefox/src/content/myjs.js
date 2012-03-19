var stopped = true;
var socket = null;
var status = '';

var last_id = null;
var last_stamp = null;

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

//------------------------------------------------------------------------------
function onVisLinkButton()
{
  if( status == 'active')
    selectVisLink();
  else
  {
    // start client
    stopped = false;
    if( register() )
    {
      window.addEventListener('unload', stopVisLinks, false);
  //    window.addEventListener('scroll', windowChanged, false);
  //    window.addEventListener('resize', resize, false);
      window.addEventListener("DOMContentLoaded", windowChanged, false); 
      //setTimeout("triggerSearch()", 500);
    }
  }
}

//------------------------------------------------------------------------------
// This function is triggered by the user if he want's to get something linked
function selectVisLink()
{
	var	selid =	content.getSelection().toString();
	var	selectionId	= "" + selid +	"";

	if (selectionId	== null	|| selectionId == "") return;
	window.localSelectionId = selectionId;
	
	reportVisLinks(selectionId);
}

//------------------------------------------------------------------------------
function reportWindowChanged()
{
	send({'task': 'window-changed', 'name': window.visLinkAppName});
}

//------------------------------------------------------------------------------
function reportVisLinks(selectionId, found)
{
	var	doc	= content.document;
	var	bbs	= searchDocument(doc, selectionId);
//	var	xml	= generateBoundingBoxesXML(bbs,	true);
	
	last_id = selectionId;
	if( !found )
	{
	  d = new Date();
	  last_stamp = (d.getHours() * 60 + d.getMinutes()) * 60 + d.getSeconds();
	}

	send({ 'task': (found ? 'FOUND' : 'INITIATE'),
	       //'name': window.visLinkAppName,
	       'id': selectionId,
	       'stamp': last_stamp,
	       'regions': bbs });
	
//	if( found )
//    alert('send FOUND: '+selectionId);
}

//------------------------------------------------------------------------------
function pageLoaded()
{
	windowChanged(); 
}

//------------------------------------------------------------------------------
function windowChanged()
{
	if (window.localSelectionId != null)
	{
		reportWindowChanged(); 
		//reportVisLinks(window.localSelectionId);
	}
	else
	{
		reportWindowChanged(); 
//		clearVisualLinks();
		//reportVisLinks(window.localSelectionId);
	}
}

//------------------------------------------------------------------------------
function stopVisLinks()
{
	stopped = true;
	setStatus('');
	window.removeEventListener('unload', stopVisLinks, false);
	window.removeEventListener('scroll', clearVisualLinks, false);
	unregister();
}

//------------------------------------------------------------------------------
function register()
{
	if (window.visLinkAppName == null)
	{
		window.visLinkAppName =	"firefox-" + (new Date()).getTime();
  }
	window.lastPointerID = null; 

	var win = content.document.defaultView;
	var x = win.screenX + (win.outerWidth - win.innerWidth) / 2;
	var y = win.screenY + (win.outerHeight - win.innerHeight);
	var w = win.innerWidth;
	var h = win.innerHeight;

	try
	{
    socket = new WebSocket('ws://localhost:4487', 'VLP');
    socket.onopen = function(event)
    {
      setStatus('active');
//      send({
//        'task': 'register',
//        'name': window.visLinkAppName,
//        'bounding-box' : {'x': x, 'y': y, 'w': w, 'h': h}
//      });
    };
    socket.onclose = function(event)
    {
      setStatus(event.wasClean ? '' : 'error');
    };
    socket.onerror = function(event)
    {
      setStatus('error');
    };
    socket.onmessage = function(event)
    {
      msg = JSON.parse(event.data);
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

//------------------------------------------------------------------------------
function unregister()
{
	try
	{
	  send({'task': 'unregister', 'name': window.visLinkAppName});
	}
	catch (err)
	{
		// vis link server no reachable, nothing to do
	}
}

//------------------------------------------------------------------------------
function clearVisualLinks()
{
	try
	{
	  send({'task': 'clear-links',
	        'name': window.visLinkAppName});
	}
	catch (err)
	{
		alert("Connection to visdaemon lost, stopping");
		stopVisLinks();
	}
}

//------------------------------------------------------------------------------
function validateWindowPosition()
{
	var oldPos = window.oldPos;
	if (oldPos != null)
	{
		if (window.screenX != oldPos.x || window.screenY != oldPos.y)
		{
			register();
			oldPos.x = window.screenX;
			oldPos.y = window.screenY;
			windowChanged();
		}
	}
	else
	{
		window.oldPos = new Object();
		window.oldPos.x = window.screenX;
		window.oldPos.y = window.screenY;
	}
}

//------------------------------------------------------------------------------
function resize()
{
	register();
	windowChanged();
}

//------------------------------------------------------------------------------
function triggerSearch()
{
	validateWindowPosition();
	if(stopped)
	  return;

	var	id = getId();
	if(stopped)
	  return; // second check, because getId() might have lost connection to daemon

	if( id != null )
	{
		window.localSelectionId = id;
		var	doc	= content.document;
		var	bbs	= searchDocument(doc, id);
/*		var	xml	= generateBoundingBoxesXML(bbs,	false);
		sendBoundingBoxes(xml);*/
		send({'task': 'report-links',
          'name': window.visLinkAppName,
          'pointer': window.lastPointerID,
          'bounding-boxes': bbs});
	}

//	setTimeout("triggerSearch()", 200);
}

//------------------------------------------------------------------------------
function getId()
{
	try
	{
		send({'task': 'propagation',
          'name': window.visLinkAppName});
	}
	catch (err)
	{
		alert("Connection to visdaemon lost, stopping");
		stopVisLinks();
		return null;
	}
/* TODO evaluate response
	var pointers = xmlDoc.getElementsByTagName("pointer"); 
	var	ids	= xmlDoc.getElementsByTagName("id");
	if(pointers.length > 0){
		if(pointers[0].childNodes[0] != null){
			window.lastPointerID = pointers[0].childNodes[0].nodeValue; 
		}
	}
	if (ids.length > 0)	{
		if (ids[0].childNodes[0] !=	null) {
			return ids[0].childNodes[0].nodeValue;
		}
	}*/
	return null;
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
		var	i =	s.indexOf(id);
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
				var	idx	= sourceString.indexOf(id);
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
					
					var	idx	= sourceString.indexOf(id, id.length + idx);
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
function findBoundingBox(doc, obj) {
	var	w =	obj.offsetWidth + 2;
	var	h =	obj.offsetHeight + 2;
	var	curleft	= curtop = 0;

	if (obj.offsetParent) {
		do {
			curleft	+= obj.offsetLeft;
			curtop += obj.offsetTop;
		} while	(obj = obj.offsetParent);
	}

	var	body = doc.getElementsByTagName("body")[0];
	var	win	= body.ownerDocument.defaultView;
	// y offset is very OS / decorator specific
	var	yoffset	= win.outerHeight -	win.innerHeight	- 2;

	var	ret	= null;
	// check if	visible
	//if (((curtop - win.pageYOffset)	> 0) &&	((curtop - win.pageYOffset)	< win.innerHeight) && 
	//	((curleft -	win.pageXOffset) > 0) && ((curleft - win.pageXOffset) <	win.innerWidth)) {
			
	y = curtop + win.screenY + yoffset - win.pageYOffset;
	x = curleft + win.screenX + 1 - win.pageXOffset;
	
//		ret	= new Object();
//		ret.x =	finalleft;
//		ret.y =	finaltop;
//		ret.width =	w +	2;
//		ret.height = h + 2;
	//}
	return [ [x,     y],
	         [x + w, y],
	         [x + w, y + h],
	         [x,     y + h] ];
}

//------------------------------------------------------------------------------
function generateBoundingBoxesXML(bbs, source) {
	var	xml	= "<boundingBoxList>";
	for	(var i = 0;	i<bbs.length; i++) {
		xml	+= "<boundingBox";
		xml	+= " x=\""+bbs[i].x+"\"";
		xml	+= " y=\""+bbs[i].y+"\"";
		xml	+= " width=\""+bbs[i].width+"\"";
		xml	+= " height=\""+bbs[i].height+"\"";
		xml	+= " source=\""+source+"\"";
		xml	+= " />\n";
		source = false;
	}
	xml	+= "</boundingBoxList>\n";

	xml	= escape(xml);
	
	return xml;
}

//------------------------------------------------------------------------------
function sendBoundingBoxes(xml)
{
  send({'task': 'report-links',
        'name': window.visLinkAppName,
        'pointer': window.lastPointerID,
        'xml': xml});
}

// window.addEventListener('load', myjsStart, false);
/*
function myjsStart() {
	alert("myjsStart");
	var	appcontent = window.document.getElementById("appcontent");
	if (appcontent)	{
		appcontent.addEventListener("DOMContentLoaded",	startVisLinks, false);
	}
} 
*/
