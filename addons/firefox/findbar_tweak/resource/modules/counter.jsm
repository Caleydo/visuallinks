moduleAid.VERSION = '1.1.5';

this.REDOINGHIGHLIGHTS = false;
this.__defineGetter__('counter', function() { return gFindBar._findStatusDesc.textContent; });
this.__defineSetter__('counter', function(v) { return gFindBar._findStatusDesc.textContent = v; });

this.moveHighlightsArray = function(level, highlights) {
	if(!prefAid.useCounter) { return; }
	
	for(var l=0; l<level.length; l++) {
		if(typeof(level[l].highlights) != 'undefined') {
			for(var i=0; i<level[l].highlights.length; i++) {
				highlights.push(level[l].highlights[i]);
			}
		}
		if(typeof(level[l].levels) != 'undefined') {
			moveHighlightsArray(level[l].levels, highlights);
		}
	}
};

this.fillHighlightCounter = function(e) {
	if(e && e.detail && e.detail.res && e.detail.res == gFindBar.nsITypeAheadFind.FIND_NOTFOUND) {
		return;
	}
	
	if(!contentWindow) { return; } // Usually triggered when a selection is on a frame and the frame closes
	
	// Special routine for PDF.JS
	if(isPDFJS) {
		// 'complete' is for FF22 (maybe 23?) and below, 'interactive' is for FF23 (maybe 24?) and above
		// I hope adding this doesn't break anything else.
		if(contentDocument.readyState != 'complete' && contentDocument.readyState != 'interactive') { return; }
		
		// We need this to access protected properties, hidden from privileged code
		var unWrap = XPCNativeWrapper.unwrap(contentWindow);
		var matches = unWrap.PDFFindController.pageMatches;
		
		var selected = 0;
		var total = 0;
		for(var p=0; p<matches.length; p++) {
			if(unWrap.PDFFindController.selected.pageIdx > -1 && unWrap.PDFFindController.selected.matchIdx > -1 && unWrap.PDFFindController.selected.pageIdx == p) {
				selected = total +unWrap.PDFFindController.selected.matchIdx +1;
			}
			total += matches[p].length;
		}
		
		if(total == 0) { return; }
		
		counter = stringsAid.get('counter', 'counterFormat', [ ["$hit$", selected], ["$total$", total] ]);
		gFindBar._findStatusDesc.hidden = false;
		gFindBar._findStatusIcon.hidden = false;
		
		dispatch(gFindBar, { type: 'HighlightCounterUpdated', cancelable: false });
		return;
	}
	
	// Normal HTML files
	if(!linkedPanel._counterHighlights || linkedPanel._counterHighlights.length == 0) { return; }
		
	var editableNode = gFindBar.browser._fastFind.foundEditable;
	var controller = (editableNode && editableNode.editor) ? editableNode.editor.selectionController : null;
	if(!controller) {
		controller = gFindBar._getSelectionController(contentWindow);
	}
	var sel = controller.getSelection(gFindBar.nsISelectionController.SELECTION_NORMAL);
	
	var h = 0;
	if(sel.rangeCount == 1) {
		var cRange = sel.getRangeAt(0);
		
		// Most times we don't need to start from the beginning of the array, it's faster to resume from a previous point
		var start = linkedPanel._currentHighlight || 0;
		if(start >= linkedPanel._counterHighlights.length) {
			start = 0;
		}
		linkedPanel._currentHighlight = 0;
		
		for(var i=start; i<linkedPanel._counterHighlights.length; i++) {
			if(compareRanges(cRange, linkedPanel._counterHighlights[i])) {
				h = i+1;
				linkedPanel._currentHighlight = i;
				break;
			}
		}
		
		if(h == 0 && start > 0) {
			for(var i=0; i<start; i++) {
				if(compareRanges(cRange, linkedPanel._counterHighlights[i])) {
					h = i+1;
					linkedPanel._currentHighlight = i;
					break;
				}
			}
		}
		
		if(!timerAid.delayHighlight && !REDOINGHIGHLIGHTS && h == 0 && linkedPanel._counterHighlights.length > 0) {
			REDOINGHIGHLIGHTS = true;
			reHighlight(documentHighlighted);
			REDOINGHIGHLIGHTS = false;
			return;
		}
	}
	
	counter = stringsAid.get('counter', 'counterFormat', [ ["$hit$", h], ["$total$", linkedPanel._counterHighlights.length] ]);
	gFindBar._findStatusDesc.hidden = false;
	gFindBar._findStatusIcon.hidden = false;
	
	dispatch(gFindBar, { type: 'HighlightCounterUpdated', cancelable: false });
};

moduleAid.LOADMODULE = function() {
	listenerAid.add(window, 'SelectedFIThit', fillHighlightCounter);
	listenerAid.add(window, 'UpdatedStatusFindBar', fillHighlightCounter);
	listenerAid.add(window, 'UpdatedPDFMatches', fillHighlightCounter);
	
	observerAid.notify('ReHighlightAll');
};

moduleAid.UNLOADMODULE = function() {
	listenerAid.remove(window, 'SelectedFIThit', fillHighlightCounter);
	listenerAid.remove(window, 'UpdatedStatusFindBar', fillHighlightCounter);
	listenerAid.remove(window, 'UpdatedPDFMatches', fillHighlightCounter);
	
	if(!UNLOADED && !window.closed && !window.willClose) {
		observerAid.notify('ReHighlightAll');
	}
};
