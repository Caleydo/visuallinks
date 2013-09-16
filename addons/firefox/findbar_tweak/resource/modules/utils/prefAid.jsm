moduleAid.VERSION = '2.0.4';
moduleAid.LAZY = true;

// prefAid - Object to contain and manage all preferences related to the add-on (and others if necessary)
// setDefaults(prefList, branch, trunk) - sets the add-on's preferences default values
//	prefList - (object) { prefName: defaultValue }, looks for 'trunk.branch.prefName'
//	(optional) branch - (string) defaults to objPathString
//	(optional) trunk - (string) defaults to 'extensions'
// listen(pref, handler) - add handler as a change event listener to pref
//	pref - (string) name of preference to append handler to
//	handler - (function) to be fired on change event
// unlisten(pref, handler) - remove handler as a change event listener of pref
//	see listen()
// listening(pref, handler) - returns (int) with corresponding listener index in _onChange[] if handler is registered as pref listener, returns (bool) false otherwise
//	see listen()
// reset(pref) - resets pref to default value
//	see listen()
this.prefAid = {
	_prefObjects: {},
	_onChange: {},
	length: 0,
	
	setDefaults: function(prefList, branch, trunk) {
		if(!branch) {
			branch = objPathString;
		}
		if(!trunk && trunk !== '') {
			trunk = 'extensions';
		}
		
		var readyList = [];
		
		var branchString = ((trunk) ? trunk+'.' : '') +branch+'.';
		var defaultBranch = Services.prefs.getDefaultBranch(branchString);
		for(var pref in prefList) {
			// When updating from a version with prefs of same name but different type would throw an error and stop.
			// In this case, we need to clear it before we can set its default value again.
			var savedPrefType = defaultBranch.getPrefType(pref);
			var prefType = typeof(prefList[pref]);
			var compareType = '';
			switch(savedPrefType) {
				case defaultBranch.PREF_STRING:
					compareType = 'string';
					break;
				case defaultBranch.PREF_INT:
					compareType = 'number';
					break;
				case defaultBranch.PREF_BOOL:
					compareType = 'boolean';
					break;
				default: break;
			}
			if(compareType && prefType != compareType) {
				defaultBranch.clearUserPref(pref);
			}
			
			switch(prefType) {
				case 'string':
					defaultBranch.setCharPref(pref, prefList[pref]);
					break;
				case 'boolean':
					defaultBranch.setBoolPref(pref, prefList[pref]);
					break;
				case 'number':
					defaultBranch.setIntPref(pref, prefList[pref]);
					break;
				default:
					Cu.reportError('Preferece '+pref+' is of unrecognizeable type!');
					break;
			}
			
			readyList.push(pref);
		}
		
		this.ready(readyList, branch, trunk);
	},
	
	ready: function(prefList, branch, trunk) {
		if(!branch) {
			branch = objPathString;
		}
		if(!trunk && trunk !== '') {
			trunk = 'extensions';
		}
		
		if(typeof(prefList) == 'string') {
			prefList = [prefList];
		}
		
		for(var i=0; i<prefList.length; i++) {
			if(!this._prefObjects[prefList[i]]) {
				this._setPref(prefList[i], branch, trunk);
			}
		}
	},
	
	_setPref: function(pref, branch, trunk) {
		var branchString = ((trunk) ? trunk+'.' : '') +branch+'.';
		
		this._prefObjects[pref] = Services.fuel.prefs.get(branchString+pref);
		this._onChange[pref] = [];
		this.__defineGetter__(pref, function() { return this._prefObjects[pref].value; });
		this.__defineSetter__(pref, function(v) { return this._prefObjects[pref].value = v; });
		this.length++;
		
		this._prefObjects[pref].events.addListener("change", this.prefChanged);
	},
	
	listen: function(pref, handler) {
		// failsafe
		if(typeof(this._onChange[pref]) == 'undefined') {
			Cu.reportError('Setting listener on unset preference: '+pref);
			return false;
		}
		
		if(this.listening(pref, handler) === false) {
			this._onChange[pref].push(handler);
			return true;
		}
		return false;
	},
	
	unlisten: function(pref, handler) {
		// failsafe
		if(typeof(this._onChange[pref]) == 'undefined') {
			Cu.reportError('Setting listener on unset preference: '+pref);
			return false;
		}
		
		var i = this.listening(pref, handler)
		if(i !== false) {
			this._onChange[pref].splice(i, 1);
			return true;
		}
		return false;
	},
	
	listening: function(pref, handler) {
		for(var i = 0; i < this._onChange[pref].length; i++) {
			if(compareFunction(this._onChange[pref][i], handler, true)) {
				return i;
			}
		}
		return false;
	},
	
	reset: function(pref) {
		this._prefObjects[pref].reset();
	},
	
	prefChanged: function(e) {
		var pref = e.data;
		if(pref.indexOf('.') > -1) {
			pref = pref.substr(pref.indexOf('.')+1);
			if(pref.indexOf('.') > -1) {
				pref = pref.substr(pref.indexOf('.')+1);
			}
		}
		
		if(typeof(prefAid._onChange[pref]) == 'undefined') {
			Cu.reportError('Setting listener on unset preference: '+pref);
		} else {
			for(var i = 0; i < prefAid._onChange[pref].length; i++) {
				prefAid._onChange[pref][i]();
			}
		}
	},
	
	clean: function() {
		for(var pref in this._prefObjects) {
			this._prefObjects[pref].events.removeListener("change", this.prefChanged);
		}
	}
};

moduleAid.UNLOADMODULE = function() {
	prefAid.clean();
};
