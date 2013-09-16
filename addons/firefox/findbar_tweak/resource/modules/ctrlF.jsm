moduleAid.VERSION = '1.3.2';

this.ctrlF = function(event) {

  // Report selection to vislink extension
  var mainWindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation)
                         .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                         .rootTreeItem
                         .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindow);
  if( mainWindow.reportVisLinks )
    mainWindow.reportVisLinks(gFindBar._getInitialSelection());

	// See if there is text selection and if it's the same as the findbar's value
	if(/*prefAid.ctrlFClosesOnValue && prefAid.FAYTprefill &&*/ !gFindBar.hidden) {
		var selText = gFindBar._getInitialSelection();
		if(selText /*&& selText != gFindBar._findField.value*/) {
			gFindBar.onFindCommand();
			gFindBar._setHighlightTimeout();
			return;
		}
	}
	
	toggleFindBar(event);
};

moduleAid.LOADMODULE = function() {
	this.backups = {
		oncommand: $('cmd_find').getAttribute('oncommand')
	};
	setAttribute($('cmd_find'), 'oncommand', objName+'.ctrlF(event);');
};

moduleAid.UNLOADMODULE = function() {
	if(this.backups) {
		setAttribute($('cmd_find'), 'oncommand', this.backups.oncommand);
		delete this.backups;
	}
};
