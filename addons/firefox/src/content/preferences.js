function changeSetting(val, type)
{
  if( !(val instanceof XULElement) )
    alert("Not supported! (changeSetting)");
  var key = val instanceof XULElement ? val.id : 'unknown';
  var val = val instanceof XULElement ? val.value : val;

  send({
    'task': 'SET',
    'id': '/config',
    'id': key,
    'val': val,
    'type': type
  });
}
var onChangeSetting = debounce(changeSetting, 400);

// =============================================================================

var p_status = document.getElementById("connection-status");
var box_settings = document.getElementById("server-settings-box");
var send = window.opener.send;

if( window.opener.status != 'active' )
{
  box_settings.style.display = 'none';
  p_status.style.color = 'red';
  p_status.textContent = 'Not connected!';
}
else
{
  box_settings.style.display = 'default';

  var socket = window.opener.socket;

  p_status.style.color = 'green';  
  p_status.textContent = 'Connected to ' + socket.url;
  
  var combo_routing = document.getElementById("default-routing-popup");
  while( combo_routing.numChildren > 1 )
    combo_routing.removeChild(combo_routing.lastChild);
  
  var routing = window.opener.routing;
  
  for(var router in routing.available)
  {
    var valid = routing.available[router][1];
    if( !valid )
      continue;

    var name = routing.available[router][0];
    var item = document.createElement("menuitem");
    item.setAttribute("label", name);
    item.setAttribute("value", name);
    combo_routing.appendChild(item);
    
    if( routing.default == name )
      combo_routing.parentNode.selectedItem = combo_routing.lastChild;
  }

  combo_routing.parentNode.addEventListener("select", function()
  {
    send({
      'task': 'SET',
      'id': '/config',
      'var': 'StaticCore:default-routing',
      'val': this.value,
      'type': 'String'
    });
    routing.default = this.value;
  });

  for(var id in window.opener.cfg)
  {
    var el = document.getElementById(id);
    if( !el )
      continue;

    el.value = window.opener.cfg[id];
  }
}
