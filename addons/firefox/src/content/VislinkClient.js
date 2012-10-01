/**
 * @param application_name  Name of the application (passed to vislink server)
 */
function VislinkClient(application_name)
{
// private:
  var _socket = 0;
  var _status = '';
  
  
	this._setStatus = function(status)
	{
	  if( _status == status )
	    return;

	  _status = status;

	  if( typeof(this.onstatus) === 'function' )
	    this.onstatus(status);
	  
	  if( status == 'error' )
	  {
	    _socket = 0;
      window.removeEventListener('resize', function() this._onResize(), false);
	  }
	};
	
	this._onResize = function()
	{
    this.send({task: 'RESIZE', region: this.getRegion()});
	};

// public:
  this.connect = function(options)
  {
    var options = options || {};
    var url = options.url || 'ws://localhost:4487';
    var $this = this;

    try
	  {
      _socket = new WebSocket(url, 'VLP');
      window.addEventListener('resize', function() $this._onResize(), false);

      _socket.onopen = function(event)
      {
        $this._setStatus('active');
        $this.send({
          task: 'REGISTER',
          name: application_name,
          pos: options.pos || [],
          region: $this.getRegion()
        });/*
        send({
          task: 'GET',
          id: '/routing'
        });*/
      };
      _socket.onclose = function(event)
      {
        $this._setStatus(event.wasClean ? '' : 'error');
//        removeAllRouteData();
      };
      _socket.onerror = function(event)
      {
        $this._setStatus('error');
//        removeAllRouteData();
      };
      _socket.onmessage = function(event)
      {/*
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
        else if( msg.task == 'GET-FOUND')
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
        else*/
          alert(event.data);
      }
	  }
	  catch (err)
	  {
		  alert("Could not establish connection to visdaemon. " + err);
		  $this._setStatus('error');
		  return false;
	  }
	  
	  return true;
	}
	
	/**
	 * Send data to the vislink server
	 *
	 * @param data  Data to be send
	 */
	this.send = function(data)
  {
    try
    {
      if( !_socket )
        throw "No socket available!";
    
      _socket.send(JSON.stringify(data));
    }
    catch(e)
    {
      alert(e);
//      throw e;
    }
  }
  
  this.replaceLink = function(id, regions)
  {
    this.send({
      'task': 'INITIATE',
      'title': document.title,
      'id': id,
      'stamp': 1,//last_stamp,
      'regions': regions
    });
  };
}
