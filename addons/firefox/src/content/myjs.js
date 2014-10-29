var debug = false;

var stopped = true;
var links_socket = null;
var ctrl_socket = null;
var ctrl_queue = null;
var status = '';
var status_sync = '';

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
var cfg = new Object();
var tile_requests = null;
var tile_timeout = false;
var do_report = true;

//var Cc = Components.classes;
//var Ci = Components.interfaces;

var prefs = Cc["@mozilla.org/fuel/application;1"]
              .getService(Ci.fuelIApplication)
              .prefs;

var session_store = Cc["@mozilla.org/browser/sessionstore;1"]
                      .getService(Ci.nsISessionStore);

const STATE_START = Ci.nsIWebProgressListener.STATE_START;
const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;
const LOCATION_CHANGE_ERROR_PAGE = Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE;
const LOCATION_CHANGE_SAME_DOCUMENT = Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT;

var myListener = {
  QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
                                         "nsISupportsWeakReference"]),

  onStateChange: function(aWebProgress, aRequest, aFlag, aStatus) {
    // If you use myListener for more than one tab/window, use
    // aWebProgress.DOMWindow to obtain the tab/window which triggers the state change
    if (aFlag & STATE_START) {
      // This fires when the load event is initiated
    }
    if (aFlag & STATE_STOP) {
      // This fires when the load finishes
    }
  },

  onLocationChange: function(progress, request, uri, flags)
  {
    if( !request )
      return;
    if( flags & (LOCATION_CHANGE_SAME_DOCUMENT | LOCATION_CHANGE_ERROR_PAGE) )
      return;

    var tab = gBrowser._getTabForContentWindow(progress.DOMWindow);
    var last_uri = session_store.getTabValue(tab, "hcd/last-uri");

    if( last_uri == uri.spec )
      return;

    console.log("change uri: " + content.document._hcd_tab_id);
    var msg = {
      'task': 'SYNC',
      'type': 'URI',
      'uri': uri.spec,
      'tab-id': session_store.getTabValue(tab, "hcd/tab-id")
    };
    ctrlSend(msg);

    last_uri = uri.spec;
    session_store.setTabValue(tab, "hcd/last-uri", last_uri);
  },

  onProgressChange: function(progress, request, curSelf, maxSelf, curTot, maxTot) {},
  onStatusChange: function(progress, request, status, msg) {},
  onSecurityChange: function(progress, request, state) {}
}
gBrowser.addProgressListener(myListener);

Array.prototype.contains = function(obj)
{
  return this.indexOf(obj) > -1;
};

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

function setStatusSync(stat)
{
  document.getElementById('vislink-sync').setAttribute('class', stat);
  status_sync = stat;

  var hidden = (stat == 'no-src');
  document.getElementById('vislink-sync').hidden = hidden;
  document.getElementById('vislink-sync-src').hidden = hidden;
}

/** Send data via the global WebSocket
 *
 */
function send(data)
{
  try
  {
    if( !links_socket )
      throw "No socket available!";

    links_socket.send(JSON.stringify(data));
  }
  catch(e)
  {
//    alert(e);
    links_socket = 0;
    stop();
    checkAutoConnect();
//    throw e;
  }
}

/** Send data to control center
 *
 *  @data
 *  @force  Connect if there is no connection (otherwise only send if there is
 *          already a connection active or beeing connected)
 */
function ctrlSend(data, force = false)
{
  if( !ctrl_queue && !ctrl_socket && !force )
    return;

  var encoded_data = data instanceof ArrayBuffer
                   ? data // binary data
                   : JSON.stringify(data);

  if( ctrl_queue )
  {
    ctrl_queue.push(encoded_data);
    return;
  }

  if( ctrl_socket )
  {
    ctrl_socket.send(encoded_data);
    return;
  }

  ctrl_queue = [encoded_data];
  ctrl_socket = new WebSocket('ws://localhost:24803', 'VLP');
  ctrl_socket.binaryType = "arraybuffer";
  ctrl_socket.onopen = function(event)
  {
    for(var i = 0; i < ctrl_queue.length; i += 1)
    {
      ctrl_socket.send(ctrl_queue[i]);
      console.log("send", ctrl_queue[i], typeof(ctrl_queue[i]));
    }
    ctrl_queue = null;
  }
  ctrl_socket.onclose = function(event)
  {
    console.log(event);
    ctrl_socket = null;
  }
  ctrl_socket.onerror = function(event) { alert(event); ctrl_socket = null; }
  ctrl_socket.onmessage = function(event)
  {
    var msg = JSON.parse(event.data);
    if( msg.task == 'SYNC' )
    {
      if( status_sync == 'sync' )
         handleSyncMsg(msg);
    }
    else
      alert(event.data);
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

  offset[0] = win.mozInnerScreenX * scale;
  offset[1] = win.mozInnerScreenY * scale;
}

/**
 * Get the document region relative to the application window
 *
 * @param ref "rel" or "abs"
 */
function getViewport(ref = "rel")
{
  updateScale();
  var win = content.document.defaultView;
  var vp = [
    Math.round((win.mozInnerScreenX - win.screenX) * scale),
    Math.round((win.mozInnerScreenY - win.screenY) * scale),
    Math.round(win.innerWidth * scale),
    Math.round(win.innerHeight * scale)
  ];

  if( ref == "abs" )
  {
    vp[0] += win.screenX;
    vp[1] += win.screenY;
  }

  return vp;
}

/**
 * Get the scroll region relative to the document region
 */
function getScrollRegion()
{
  var doc = content.document;
  return {
    x: -content.scrollX, // coordinates relative to
    y: -content.scrollY, // top left corner of viewport
    width: doc.documentElement.scrollWidth,
    height: doc.documentElement.scrollHeight
  };
}

/**
 * XPath for given DOM Element
 */
function getXPathForElement(el, root)
{
  var xpath = '';
  while( el && el !== root )
  {
    var sibling = el;
	  var pos = 0;

    do
    {
		  if(    sibling.nodeType === Element.ELEMENT_NODE
          && sibling.nodeName === el.nodeName )
			  pos += 1;

		  sibling = sibling.previousSibling;
	  } while( sibling );

	  xpath = "/" + el.nodeName + "[" + pos + "]" + xpath;
	  el = el.parentNode;
  }
  if( el )
    xpath = "." + xpath;
  return xpath;
}

/**
 * Get DOM Element for given XPath
 */
function getElementForXPath(xpath, root)
{
  return root.ownerDocument
             .evaluate( xpath,
                        root,
                        null,
                        XPathResult.FIRST_ORDERED_NODE_TYPE,
                        null )
             .singleNodeValue;
}

function checkAutoConnect(event)
{
  var doc = event ? event.originalTarget : content.document;
  var loc = doc.defaultView ? doc.defaultView.location : null;

  if( loc && loc.host == "localhost" && loc.pathname.startsWith("/userstudy/") )
    start(true);
}

/**
 * Page/Tab load hook
 */
function onPageLoad(event)
{
  setStatusSync("no-src");

  if( !stopped )
    setTimeout("resize();", 300);
  else
    checkAutoConnect(event);

  var doc = content.document;
  var view = doc.defaultView;
  var location = view ? view.location : null;

  var tab = gBrowser.selectedTab;
  var src_id = doc._hcd_src_id;

  var hcd_str = "#hidden-content-data=";
  var hcd_len = hcd_str.length;
  var hcd_pos = location.hash.indexOf(hcd_str);

  if( hcd_pos >= 0 )
  {
    var hcd = location.hash.substr(hcd_pos + hcd_len);
    location.hash = location.hash.substr(0, hcd_pos);

    var pos;
    var data = JSON.parse(hcd);
    if( data )
    {
      var scroll = data['scroll'];
      if( scroll instanceof Array )
        content.scrollTo(scroll[0], scroll[1]);

      var scrollables = data['elements-scroll'];
      if( scrollables instanceof Object )
      {
        for(var xpath in scrollables)
        {
          var el = getElementForXPath(xpath, doc.body);
          if( !el )
            continue;

          var offset = scrollables[xpath];
          el.scrollLeft = offset[0];
          el.scrollTop = offset[1];
        }
      }

      src_id = data['src-id'];
      pos = data['pos'];
/*      if( pos instanceof Array )
        window.moveTo(pos[0], pos[1]);*/

      var color = data['color'];
      if( typeof(color) === 'string' )
        document.getElementById('vislink-sync-src').style.color = color;
    }

    // Automatically connect new windows created by links system.
    if( stopped )
      setTimeout('start(true, "' + src_id + '");', 0);

    var lock_file = Components.classes["@mozilla.org/file/directory_service;1"]
                              .getService( Components.interfaces.nsIProperties)
                              .get("ProfD", Components.interfaces.nsIFile);
    lock_file.append("lock");

    // <profile-path>/lock is a symlink to 127.0.1.1:+<pid>
    var pid = lock_file.target.split('+')[1];
    var msg = {
      task: "REGISTER",
      pid: pid,
      title: document.title
    };
    if( pos instanceof Array )
      msg["pos"] = pos;
    if( src_id )
      msg["src-id"] = src_id;

    ctrlSend(msg, true);
  }

  if( !src_id )
    src_id = session_store.getTabValue(tab, "hcd/src-id");

  if( src_id )
  {
    doc._hcd_src_id = src_id;
    session_store.setTabValue(tab, "hcd/src-id", src_id);
    setStatusSync("sync");
  }

  content.addEventListener("keydown", onKeyDown, false);
  content.addEventListener("keyup", onKeyUp, false);
  content.addEventListener('scroll', onScroll, false);

  console.log("tab: id=" + doc._hcd_tab_id
              + ", src=" + doc._hcd_src_id);

  for(var xpath in doc._hcd_scroll_listener)
  {
    if( !doc._hcd_scroll_listener.hasOwnProperty(xpath) )
      continue;

    var sh = doc._hcd_scroll_listener[ xpath ];
    sh[0].removeEventListener("scroll", sh[1], false);
  }
  doc._hcd_scroll_listener = {};

  var elements = doc.body.getElementsByTagName("*");
  for(var i = 0; i < elements.length; i += 1)
  {
    var el = elements[i];
    if(    el.scrollWidth <= el.clientWidth
        && el.scrollHeight <= el.clientHeight )
      continue;

    if(    el.style.overflow  != "scroll"
        && el.style.overflowX != "scroll"
        && el.style.overflowY != "scroll" )
      continue;

    var xpath = getXPathForElement(el, doc.body);
    var cb = throttle(onElementScrollImpl, 100);
    el.addEventListener("scroll", cb, false);

    doc._hcd_scroll_listener[ xpath ] = [el, cb];
  }
}

var tab_changed = false;
var tab_event = 0;
function onTabChange(e)
{
  tab_changed = true;
  tab_event = e;
  setTimeout("onTabChangeImpl();", 1);
}

function onTabChangeImpl()
{
  if( !tab_changed )
    return;
  tab_changed = false;

  if(     content.document.readyState != "complete"
      || !content.document.body )
    return;

  onPageLoad(tab_event);
}

function onLoad(e)
{
  var doc = e.originalTarget;
  var view = doc.defaultView;

  // Ignore frame load events
  if( view && view.frameElement )
    return;

  var tab_id = doc._hcd_tab_id;
  if( !tab_id )
  {
    var tab = gBrowser.selectedTab;
    tab_id = session_store.getTabValue(tab, "hcd/tab-id");

    if( !tab_id )
    {
      // Unique tab identifier (eg. for synchronized scrolling)
      tab_id = Sha1.hash(Date.now() + location.href);
      session_store.setTabValue(tab, "hcd/tab-id", tab_id);
      console.log("new id: " + tab_id);
    }
    else
      console.log("restored id: " + tab_id);

    doc._hcd_tab_id = tab_id;
  }
  else
    console.log("has id: " + tab_id);

  // Ignore background tab load events
  if( doc != content.document )
    return;

  tab_changed = false;
  onPageLoad(e);
}

function onUnload(e)
{
  var tab_id = e.originalTarget['_hcd_tab_id'];
//  if( tab_id )
//    alert('close: ' + tab_id);

  if(    e.originalTarget.defaultView.frameElement
      || e.originalTarget != content.document )
    // Ignore frame and background tab unload events
    return;

  content.removeEventListener("keydown", onKeyDown, false);
  content.removeEventListener("keyup", onKeyUp, false);
  content.removeEventListener('scroll', onScroll, false);

  tab_changed = true;
  tab_event = e;
  setTimeout("onTabChangeImpl();", 1);
}

var last_ctrl_down = 0;
function onKeyDown(e)
{
  // [Ctrl]
  if( e.keyCode == 17 )
    last_ctrl_down = e.timeStamp;
}

function onKeyUp(e)
{
  // [Ctrl]
  if( e.keyCode == 17 )
  {
    if( e.timeStamp - last_ctrl_down > 300 )
      return;

    if( status == 'active' )
      selectVisLink();
  }
}

function _getCurrentTabData(e)
{
  var tab = gBrowser.tabContainer._getDragTargetTab(e);
  var browser = tab ? tab.linkedBrowser
                    : gBrowser;
  var scroll = getScrollRegion();
  var doc = content.document;

  // positions of all scrollable html elements
  var scrollables = {};
  var listener = doc._hcd_scroll_listener;
  for(var xpath in listener)
  {
    if( !listener.hasOwnProperty(xpath) )
      continue;

    var el = listener[ xpath ][0];
    if( el.scrollTop == 0 && el.scrollLeft == 0 )
      continue;

    scrollables[ xpath ] = [el.scrollLeft, el.scrollTop];
  }

  return {
    "url": browser.currentURI.spec,
    "scroll": [-scroll.x, -scroll.y],
    "elements-scroll": scrollables,
    "view": getViewport("abs"),
    "tab-id": doc._hcd_tab_id,
    "screenPos": [e.screenX, e.screenY]
  };
}

function _websocketDrag(e)
{
  e.stopImmediatePropagation();
  e.preventDefault();

  var data = _getCurrentTabData(e);
  data.task = 'drag';

  ctrlSend(data, true);

  // Send preview image
  var reg = {
    x: data.scroll[0],
    y: data.scroll[1],
    width: data.view[2],
    height: data.view[3]
  };
  var regions = [[reg.y, reg.y + reg.height]];
  ctrlSend(grab([reg.width, reg.height], reg, 0, [regions, regions]), true);
}

function onDocumentClick(e)
{
  if( !e.ctrlKey )
    return;

  e.preventDefault();
  e.stopImmediatePropagation();
}

function onTabDblClick(e)
{
  if( e.ctrlKey )
    return _websocketDrag(e);
}

/*function onDragStart(e)
{
  return _websocketDrag(e);

  var dt = e.dataTransfer;
  if( tab )
    dt.mozSetDataAt(TAB_DROP_TYPE, tab, 0);
  dt.mozSetDataAt("text/plain", JSON.stringify(_getCurrentTabData()), 0);
}*/

/**
 * Load window hook
 */
window.addEventListener("load", function window_load()
{
  gBrowser.addEventListener("load", onLoad, true);
  gBrowser.addEventListener("beforeunload", onUnload, true);
  gBrowser.addEventListener("mousedown", onDocumentClick, true);
  gBrowser.addEventListener("click", onTabDblClick, true);

  var container = gBrowser.tabContainer;
  container.addEventListener("TabSelect", onTabChange, false);

  // Drag tabs from address bar/identity icon
  var ibox = document.getElementById("identity-box");
//  ibox.addEventListener('dragstart', onDragStart, false);
  document.addEventListener('click', onTabDblClick, true);

  // Drag tabs from tab bar
//  gBrowser.tabContainer.addEventListener('dragstart', onDragStart, true);
//  gBrowser.tabContainer.addEventListener('click', onTabDblClick, true);
});

/**
 * https://github.com/danro/jquery-easing/blob/master/jquery.easing.js
 *
 * @param t current time
 * @param b begInnIng value
 * @param c change In value
 * @param d duration
 */
function easeInOutQuint(t, b, c, d)
{
  if ((t/=d/2) < 1) return c/2*t*t*t*t*t + b;
  return c/2*((t-=2)*t*t*t*t + 2) + b;
}

/**
 *
 */
function smoothScrollTo(y_target)
{
  var x_cur = content.scrollX;
  var y_cur = content.scrollY;
  var delta = y_target - y_cur;
  var duration = Math.max(400, Math.min(Math.abs(delta), 2000));
  for( var t = 0; t <= duration; t += 100 )
  {
    if( t + 99 > duration )
      t = duration;

    var y = easeInOutQuint(t, y_cur, delta, duration);
    setTimeout("content.scrollTo("+x_cur+","+y+")", t);
  }
}

function start(match_title = false, src_id = 0)
{
  // start client
  stopped = false;
  if( register(match_title, src_id) )
  {
//    window.addEventListener('unload', stopVisLinks, false);
//    window.addEventListener("DOMAttrModified", attrModified, false);
    window.addEventListener('resize', resize, false);
//    window.addEventListener("DOMContentLoaded", windowChanged, false);
  }
}

//------------------------------------------------------------------------------
function onVisLinkButton(ev)
{
  if( ev.target.id != 'vislink' )
    // Do not use event if not button itself but an entry from the menu has
    // been activated.
    return;

  if( status == 'active')
    selectVisLink();
  else
    start();
}

//------------------------------------------------------------------------------
function onTabSyncButton(ev)
{
  if( ev.target.id != 'vislink-sync' )
    // Ignore clicks outside the button (eg for dropdown menu)
    return;

  setStatusSync(status_sync == 'sync' ? '' : 'sync');
}

function getSelectionId()
{
  var txt = document.getElementById("vislink-search-text");
  var selid = txt != null ? txt.value.trim() : "";
  if( selid == "" )
  {
    var selection = content.getSelection();
    selid = selection.toString().trim();
    selection.removeAllRanges();
  }
  else
    txt.reset();

  return selid.replace(/\s+/g, ' ')
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
  if( !do_report || status != 'active' || !id.length )
    return;

  if( debug )
    var start = Date.now();

  if( !found && getPref("replace-route") )
    abortAll();

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
  var msg = {
    'task': (found ? 'FOUND' : 'INITIATE'),
    'title': document.title,
    'scroll-region': [reg.x, reg.y, reg.width, reg.height],
    'id': id,
    'stamp': last_stamp,
  };

  if( bbs.length > 0 )
    msg['regions'] = bbs;

  send(msg);
  onScroll();
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
function onScrollImpl()
{
  var scroll = getScrollRegion();
  var view = getViewport();

  var msg = {
    'task': 'SYNC',
    'type': 'SCROLL',
    'pos': [scroll.x, scroll.y],
    'pos-rel': [0, 0],
    'tab-id': content.document._hcd_tab_id
  };

  var scroll_w = scroll.width - view[2];
  if( scroll_w > 1 )
    msg['pos-rel'][0] = scroll.x / scroll_w;

  var scroll_h = scroll.height - view[3];
  if( scroll_h > 1 )
    msg['pos-rel'][1] = scroll.y / scroll_h;

  send(msg);
  ctrlSend(msg);
}
var onScroll = throttle(onScrollImpl, 100);

//------------------------------------------------------------------------------
function onElementScrollImpl(e)
{
  var msg = {
    'task': 'SYNC',
    'type': 'SCROLL',
    'pos': [e.target.scrollLeft, e.target.scrollTop],
    'xpath': getXPathForElement(e.target, content.document.body),
    'tab-id': content.document._hcd_tab_id
  };
  send(msg);
  ctrlSend(msg);
}
var scroll_handlers = {};

//------------------------------------------------------------------------------
function handleSyncMsg(msg)
{
  console.log(JSON.stringify(msg));

  if( msg['tab-id'] != content.document._hcd_src_id )
    return;

  var type = msg['type'];

  if( type == 'SCROLL' )
  {
    if( msg['xpath'] )
    {
      var el = getElementForXPath(msg['xpath'], content.document.body);
      el.scrollLeft = msg.pos[0];
      el.scrollTop = msg.pos[1];
    }
    else
      content.scrollTo(-msg.pos[0], -msg.pos[1]);
  }
  else if( type == 'URI' )
  {
    content.document.location.href = msg['uri'];
  }
}

//------------------------------------------------------------------------------
function reroute()
{
  // trigger reroute
  for(var route_id in active_routes)
    reportVisLinks(route_id);
}

//------------------------------------------------------------------------------
function stop()
{
	stopped = true;
//	setStatus('');
//	window.removeEventListener('unload', stopVisLinks, false);
	window.removeEventListener("DOMAttrModified", attrModified, false);
  window.removeEventListener('resize', resize, false);
}

//------------------------------------------------------------------------------
function register(match_title = false, src_id = 0)
{
  menu = document.getElementById("vislink_menu");
  items_routing = document.getElementById("routing-selector");

    // Get the box object for the link button to get window handler from the
    // window at the position of the box
    var box = document.getElementById("vislink").boxObject;

    try
    {
      tile_requests = new Queue();

      links_socket = new WebSocket('ws://localhost:4487', 'VLP');
      links_socket.binaryType = "arraybuffer";
      links_socket.onopen = function(event)
      {
        setStatus('active');
        var cmds = ['open-url'];
        if( src_id )
          cmds.push('scroll');
        var msg = {
          task: 'REGISTER',
          name: "Firefox",
          cmds: cmds,
          viewport: getViewport()
        };
        if( match_title )
          msg.title = gBrowser.contentTitle;
        else
          msg.pos = [box.screenX + box.width / 2, box.screenY + box.height / 2];
        if( src_id )
          msg['src-id'] = src_id;
        send(msg);

        var props = {
          'CPURouting:SegmentLength': 'Integer',
          'CPURouting:NumIterations': 'Integer',
          'CPURouting:NumSteps': 'Integer',
          'CPURouting:NumSimplify': 'Integer',
          'CPURouting:NumLinear': 'Integer',
          'CPURouting:StepSize': 'Float',
          'CPURouting:SpringConstant': 'Float',
          'CPURouting:AngleCompatWeight': 'Float',
          '/routing': 'String'
        };
        for(var name in props)
          send({
            task: 'GET',
            id: name,
            type: props[name]
          });
      };
      links_socket.onclose = function(event)
      {
        setStatus(event.wasClean ? '' : 'error');
        removeAllRouteData();
        stop();
      };
      links_socket.onerror = function(event)
      {
        setStatus('error');
        removeAllRouteData();
        stop();
      };
      links_socket.onmessage = function(event)
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

          do_report = false;

          gFindBar._findField.value = msg.id;
          gFindBar.open(gFindBar.FIND_TYPEAHEAD);
          gFindBar._find();

          do_report = true;

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

              if( typeof(name) == 'undefined' )
                continue;

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
          else
            cfg[msg.id] = msg.val;
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
            smoothScrollTo(msg.val);
        }
        else if( msg.task == 'CMD' )
        {
          if( msg.cmd == 'open-url' )
          {
            var flags = 'menubar,toolbar,location,status,scrollbars';
            var url = msg.url;

            delete msg.cmd;
            delete msg.task;
            delete msg.url;

            if( typeof(msg.view) != 'undefined' )
            {
              flags += ',width=' + msg.view[0] + ',height=' + msg.view[1];
              delete msg.view;
            }

            url += '#hidden-content-data=' + JSON.stringify(msg);
            window.open(url, '_blank', flags);
          }
          else
            alert("Unknown command: " + event.data);
        }
        else if( msg.task == 'SYNC' )
          handleSyncMsg(msg);
        else
          alert("Unknown message: " + event.data);
      }
	}
	catch (err)
	{
		alert("Could not establish connection to visdaemon. "+err);
    stop();
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
  links_socket.send( grab(req.size, req.src, req.req_id, mapping) );

  // We need to wait a bit before sending the next tile to not congest the
  // receiver queue.
  setTimeout('handleTileRequest()', 200);
  tile_timeout = true;
}

//------------------------------------------------------------------------------
function attrModified(e)
{
  if( e.attrName.lastIndexOf('treestyletab', 0) === 0 )
    return;
  if( [ 'actiontype', 'afterselected', 'align',
        'beforeselected', 'busy', 'buttonover',
        'class', 'collapsed', 'crop', 'curpos',
        'dir', 'disabled',
        'fadein', 'feed', 'focused', 'forwarddisabled',
        'hidden',
        'ignorefocus', 'image', 'inactive',
        'label', 'last-tab', 'level', 'linkedpanel',
        'maxpos', 'maxwidth', 'minwidth',
        'nomatch',
        'ordinal',
        'pageincrement', 'parentfocused', 'pending', 'previoustype', 'progress',
        'src',
        'selected', 'style',
        'text', 'title', 'tooltiptext', 'type',
        'url',
        'value',
        'width' ].contains(e.attrName.trim()) )
    return;
  alert("'" + e.attrName + "': " + e.prevValue + " -> " + e.newValue + " " + content.document.readyState);
/*  if( e.attrName == "screenX" || e.attrName == "screenY" )
    windowChanged();*/
}

//------------------------------------------------------------------------------
function resize()
{
  send({task: 'RESIZE', viewport: getViewport()});

  var reg = getScrollRegion();
  for(var route_id in active_routes)
  {
    var msg = {
      'task': 'UPDATE',
      'scroll-region': [reg.x, reg.y, reg.width, reg.height],
      'id': route_id,
      'stamp': active_routes[route_id].stamp,
    };

    var bbs = searchDocument(content.document, route_id);
    if( bbs.length > 0 )
      msg['regions'] = bbs;

    send(msg);
  }
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
function searchDocument(doc, id)
{
  updateScale();

	var id_regex = id.replace(/[\s-._]+/g, "[\\s-._]+");
	var	bbs	= new Array();
	var	textnodes =	doc.evaluate("//body//*/text()", doc, null,	XPathResult.ANY_TYPE, null);
	while(node = textnodes.iterateNext())
	{
		var	s =	node.nodeValue;

    var reg = new RegExp(id_regex, "ig");
		var m;
		while( (m = reg.exec(s)) !== null )
		{
			var range = document.createRange();
			range.setStart(node, m.index);
			range.setEnd(node, m.index + m[0].length);

			var rects = range.getClientRects();
			for(var i = 0; i < rects.length; ++i)
			{
				var bb = rects[i];
				if( bb.width > 2 && bb.height > 2 )
		    {
		      var l = Math.round(offset[0] + scale * (bb.left - 1));
		      var r = Math.round(offset[0] + scale * (bb.right + 1));
		      var t = Math.round(offset[1] + scale * (bb.top - 1));
		      var b = Math.round(offset[1] + scale * (bb.bottom));
					bbs[bbs.length] = [[l, t], [r, t], [r, b], [l, b]];
		    }
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
  /*
  var outside = false;

  // check if	visible
  if(    (x + 0.5 * w < 0) || (x + 0.5 * w > win.innerWidth)
      || (y + 0.5 * h < 0) || (y + 0.5 * h > win.innerHeight) )
    outside = true;*/

  x *= scale;
  y *= scale;
  w *= scale;
  h *= scale;

  x += offset[0];
  y += offset[1];

  return [ [x,     y],
           [x + w, y],
           [x + w, y + h],
           [x,     y + h]/*,
           {outside: outside}*/ ];
}
