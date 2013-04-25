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
      window.removeEventListener('resize', function(){ this._onResize(); }, false);
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
      window.addEventListener('resize', function(){ $this._onResize(); }, false);

      _socket.onopen = function(event)
      {
        $this._setStatus('active');
        $this.send({
          task: 'REGISTER',
          name: application_name,
          pos: options.pos || [],
          viewport: $this.getRegion()
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
      {
        var msg = JSON.parse(event.data);

        if( typeof($this.onmessage) === 'function' )
	        $this.onmessage(msg);
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
  
  this.reportLink = function(id, stamp, regions)
  {
    if( regions.length > 0 )
      this.send({
        'task': 'FOUND',
        'title': document.title,
        'id': id,
        'stamp': stamp,
        'regions': regions
      });
  };
}
