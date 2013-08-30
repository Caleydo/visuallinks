moduleAid.VERSION = '1.2.1';

this.SHORT_DELAY = 25;
this.LONG_DELAY = 1500;

this.__defineGetter__('documentHighlighted', function() {
	return (contentDocument && trueAttribute(contentDocument.documentElement, 'highlighted'));
});
this.__defineSetter__('documentHighlighted', function(v) {
	if(contentDocument) { toggleAttribute(contentDocument.documentElement, 'highlighted', v); }
});
this.__defineGetter__('documentReHighlight', function() {
	return (contentDocument && trueAttribute(contentDocument.documentElement, 'reHighlight'));
});
this.__defineSetter__('documentReHighlight', function(v) {
	if(contentDocument) { toggleAttribute(contentDocument.documentElement, 'reHighlight', v); }
});

this.emptyNoFindUpdating = function(e) {
	if(e.detail.res == gFindBar.nsITypeAheadFind.FIND_NOTFOUND && !gFindBar._findField.value) {
		e.preventDefault();
		e.stopPropagation();
		gFindBar._updateStatusUI(gFindBar.nsITypeAheadFind.FIND_FOUND);
	}
};

this.keepStatusUI = function(e) {
	linkedPanel._statusUI = e.detail.res;
};

this.escHighlights = function(e) {
	if(e.keyCode == e.DOM_VK_ESCAPE) {
		gFindBar.toggleHighlight(false);
	}
};

this.highlightOnClose = function(e) {
	// Cancel a delayed highlight when closing the find bar
	timerAid.cancel('delayHighlight');
	
	// If we're closing another findbar, do nothing else
	if(e.type == 'ClosedFindBarAnotherTab') { return; }
	
	// To remove the grid and the esc key listener if there are no highlights
	if(documentHighlighted && (!gFindBar._findField.value || linkedPanel._notFoundHighlights)) {
		gFindBar.toggleHighlight(false);
	}
};

this.highlightsOnToggling = function(e) {
	var aHighlight = e.detail;
	// Remove highlights when hitting Esc
	if(aHighlight) {
		listenerAid.add(contentDocument, 'keyup', escHighlights);
	} else {
		listenerAid.remove(contentDocument, 'keyup', escHighlights);
	}
	
	documentHighlighted = aHighlight;
	documentReHighlight = false;
	
	// This is only used by gFindBar.close(), to remove the grid and the esc event if they're not needed
	linkedPanel._notFoundHighlights = false;
	
	// Make sure we cancel any highlight timer that might be running
	timerAid.cancel('delayHighlight');
};

// Called on newly opened tabs, for the bugfix in highlightsTabSelected()
this.highlightsTabOpened = function(e) {
	if(e.target != gBrowser.mCurrentTab) {
		$(e.target.linkedPanel)._neverCurrent = true;
	}
};

// Handler for when switching tabs
this.highlightsTabSelected = function(e) {
	// Bugfix: it would call highlights on new tabs always, because the tab would have a "rehighlight" attribute (all lowercase) which I can't find out where I'm setting!
	// This could cause a noticeable slowdown when switching to new tabs for the first time.
	if(!perTabFB && linkedPanel._neverCurrent) {
		delete linkedPanel._neverCurrent;
		linkedPanel._statusUI = gFindBar.nsITypeAheadFind.FIND_FOUND; // Simulate an empty search, to clear the find bar when switching to the new tab
		documentReHighlight = false;
	}
	
	// The objective was to aSync only the reHighlight() part, but because the findField value could change in the meantime, that would fail,
	// so I'm aSync'ing this whole bit.
	timerAid.init('highlightsTabSelected', function() {
		if(perTabFB) {
			if(!gFindBarInitialized || findBarHidden) {
				documentReHighlight = false;
			}
			
			if(documentReHighlight) {
				var originalValue = null;
				if(gFindBar._keepCurrentValue && linkedPanel._findWord != gFindBar._findField.value) {
					originalValue = gFindBar._findField.value;
					gFindBar._findField.value = linkedPanel._findWord;
				}
				
				reHighlight(documentHighlighted);
				
				if(originalValue) {
					gFindBar._findField.value = originalValue;
				}
			}
			return;
		}
		
		var originalValue = gFindBar._findField.value;
		
		if(linkedPanel._findWord && (documentHighlighted || documentReHighlight)) {
			gFindBar._findField.value = linkedPanel._findWord;
			gFindBar._enableFindButtons(gFindBar._findField.value);
			if(linkedPanel._statusUI != undefined) {
				gFindBar._updateStatusUI(linkedPanel._statusUI);
			}
			if(findBarHidden) {
				documentReHighlight = false;
			}
		}
		
		if(documentReHighlight) {
			gFindBar.getElement("highlight").checked = documentHighlighted;
			reHighlight(documentHighlighted);
		} else if(linkedPanel._statusUI != undefined && (!linkedPanel._findWord || (!documentHighlighted && !documentReHighlight)) ) {
			gFindBar._updateStatusUI(linkedPanel._statusUI);
		}
		
		if(gFindBar._keepCurrentValue) {
			gFindBar._findField.value = originalValue;
		}
	}, 0);
};

// Commands a reHighlight if needed on any tab, triggered from frames as well
// Mainly for back/forward actions
this.highlightsContentLoaded = function(e) {
	// this is the content document of the loaded page.
	var doc = e.originalTarget;
	if(doc instanceof window.HTMLDocument) {
		// is this an inner frame?
		// Find the root document:
		while(doc.defaultView.frameElement) {
			doc = doc.defaultView.frameElement.ownerDocument;
		}
		
		if(doc == contentDocument) {
			// Bugfix: don't do immediately! Pages with lots of frames will trigger this each time a frame is loaded, can slowdown page load
			timerAid.init('highlightsContentLoaded', function() {
				if(doc == contentDocument) { reHighlight(documentHighlighted); }
				else { setAttribute(doc.documentElement, 'reHighlight', 'true'); }
			}, 1000);
		} else {
			setAttribute(doc.documentElement, 'reHighlight', 'true');
		}
	}
};

// Tab progress listeners, handles opening and closing of pages and location changes
this.highlightsProgressListener = {
	// Commands a reHighlight if needed, triggered from history navigation as well
	onLocationChange: function(browser, webProgress, request, location) {
		// Bugfix: because of the change in bug 253793, the state of the highlight all button will be also be reset when new content through ajax is also loaded.
		// This isn't a problem in firefox, and it is probably better because new content also doesn't trigger a re-highlight,
		// but since we do trigger a re-highlight in that case, it's better if we try to keep the button state as well.
		// I'm using the same conditioning as in the original browser's onLocationChange handler.
		if(gFindBarInitialized && webProgress.isTopLevel) {
			gFindBar.getElement("highlight").checked = documentHighlighted;
		}
		
		// Frames don't need to trigger this
		if(webProgress.DOMWindow == browser.contentWindow) {
			// No need to call if there is nothing to find
			if(browser == gBrowser.mCurrentBrowser) {
				// Bugfix: This used to be (request && !request.isPending()),
				// I'm not sure why I made it that way before, maybe I saw it in an example somewhere?
				// But by also reHighlighting when !request, we successfully reHighlight when there is dynamic content loaded (e.g. AJAX)
				// e.g. "Show more" button in deviantart
				if(!request || !request.isPending()) {
					reHighlight(documentHighlighted);
				}
			}
			else if(browser.contentDocument) {
				setAttribute(browser.contentDocument.documentElement, 'reHighlight', 'true');
			}
		}
	}
};

// ReDo highlights when hitting FindAgain if necessary (should rarely be triggered actually)
this.highlightsFindAgain = function() {
	if(documentReHighlight && (!perTabFB || viewSource || gFindBarInitialized)) {
		reHighlight(documentHighlighted);
	}
};

// This always calls toggleHighlight() at least once with a false argument, then with a true argument if reDo is true.
// This way we ensure the old highlights are removed before adding new ones.
this.reHighlight = function(reDo) {
	// If there's no need to even call toggleHighlight(false) if we shouldn't, this can save a few ms when selecting tabs or loading documents
	if(!documentHighlighted && ((perTabFB && !gFindBarInitialized) || gFindBar.hidden || !gFindBar._findField.value)) {
		documentReHighlight = false;
		return;
	}
	
	gFindBar.toggleHighlight(false);
	if(reDo && dispatch(gFindBar, { type: 'WillReHighlight' })) {
		gFindBar.toggleHighlight(true);
	}
};

// Add the reHighlight attribute to all tabs
this.reHighlightAll = function() {
	// Timer prevents unnecessary multiple rehighlights
	timerAid.init('reHighlightAll', function() {
		// This happens sometimes when opening new windows, I can't find out how this is getting called before viewSource is defined but it makes no functional difference
		if(typeof(viewSource) == 'undefined') { return; }
		
		if(!viewSource) {
			for(var i=0; i<gBrowser.tabContainer.childNodes.length; i++) {
				setAttribute(gBrowser.tabContainer.childNodes[i].linkedBrowser.contentDocument.documentElement, 'reHighlight', 'true');
			}
		}
		
		reHighlight(documentHighlighted);
	}, 100);
};

// Trigger highlights when hitting Find Again
this.highlightOnFindAgain = function(e) {
	if(!prefAid.highlightOnFindAgain || isPDFJS || (documentHighlighted && linkedPanel._findWord && linkedPanel._findWord == gFindBar._findField.value)) { return; }
	
	gFindBar._setHighlightTimeout();
};

this.highlightsInit = function(bar) {
	bar.__setHighlightTimeout = bar._setHighlightTimeout;
	bar._setHighlightTimeout = function() {
		// We want this to be updated regardless of what happens
		linkedPanel._findWord = this._findField.value;
		
		// Just reset any highlights and the counter if it's not supposed to highlight
		if(!this.getElement("highlight").checked || !this._findField.value) {
			this.toggleHighlight(false);
			return;
		}
		
		var delay = SHORT_DELAY;
		
		// Delay highlights if search term is too short
		if(this._findField.value && prefAid.minNoDelay > 0 && this._findField.value.length < prefAid.minNoDelay) {
			delay = LONG_DELAY;
		}
			
		// Remove highlights when hitting Esc
		// Needs to be both in here and in toggleHighlight() because the delay could prevent it from being set
		if(!documentHighlighted) {
			listenerAid.add(contentDocument, 'keyup', escHighlights);
		}
		
		// Make sure it triggers the highlight if we switch tabs meanwhile
		documentHighlighted = true;
		documentReHighlight = true;
		
		var panelCalled = linkedPanel;
		timerAid.init('delayHighlight', function() {
			// We don't want to highlight pages that aren't supposed to be highlighted (happens when switching tabs when delaying highlights)
			if(linkedPanel == panelCalled) {
				reHighlight(gFindBar.getElement("highlight").checked);
			}
		}, delay);
	};
	
	bar._toggleHighlight = bar.toggleHighlight;
	bar.toggleHighlight = function(aHighlight) {
		// Bugfix: with PDF.JS find would not work because it would hang when checking for PDFView.pdfDocument.numPages when PDFView.pdfDocument was still null.
		if(isPDFJS && contentDocument.readyState != 'complete') {
			return;
		}
		
		var suffix = (perTabFB && !viewSource && this.linkedPanel != gBrowser.mCurrentTab.linkedPanel) ? 'AnotherTab' : '';
		
		if(dispatch(this, { type: 'WillToggleHighlight'+suffix, detail: aHighlight })) {
			this._toggleHighlight(aHighlight);
			dispatch(this, { type: 'ToggledHighlight'+suffix, detail: aHighlight, cancelable: false });
		}
	};
	
	bar.__findAgain = bar._findAgain;
	bar._findAgain = function(aFindPrevious) {
		var suffix = (perTabFB && !viewSource && this.linkedPanel != gBrowser.mCurrentTab.linkedPanel) ? 'AnotherTab' : '';
		
		if(dispatch(this, { type: 'WillFindAgain'+suffix, detail: { aFindPrevious: aFindPrevious } })) {
			var ret = this.__findAgain(aFindPrevious);
			dispatch(this, { type: 'FoundAgain'+suffix, cancelable: false, detail: { aFindPrevious: aFindPrevious, retValue: ret } });
			return ret;
		}
		return null;
	};
};

this.highlightsDeinit = function(bar) {
	bar._setHighlightTimeout = bar.__setHighlightTimeout;
	bar.toggleHighlight = bar._toggleHighlight;
	bar._findAgain = bar.__findAgain;
	delete bar.__setHighlightTimeout;
	delete bar.__findAgain;
	delete bar._toggleHighlight;
};

this.toggleHighlightByDefault = function() {
	moduleAid.loadIf('highlightByDefault', prefAid.highlightByDefault);
};

this.toggleHideOnClose = function() {
	moduleAid.loadIf('hideOnClose', prefAid.hideWhenFinderHidden);
};

this.toggleFillSelectedText = function() {
	moduleAid.loadIf('fillSelectedText', prefAid.fillSelectedText);
};

moduleAid.LOADMODULE = function() {
	initFindBar('highlights', highlightsInit, highlightsDeinit);
	
	listenerAid.add(window, 'WillUpdateStatusFindBar', emptyNoFindUpdating);
	listenerAid.add(window, 'ClosedFindBar', highlightOnClose);
	listenerAid.add(window, 'ClosedFindBarAnotherTab', highlightOnClose);
	listenerAid.add(window, 'WillToggleHighlight', highlightsOnToggling);
	listenerAid.add(window, 'WillFindAgain', highlightsFindAgain);
	listenerAid.add(window, 'FoundAgain', highlightOnFindAgain);
	observerAid.add(reHighlightAll, 'ReHighlightAll');
	
	if(!viewSource) {
		listenerAid.add(window, 'UpdatedStatusFindBar', keepStatusUI);
		listenerAid.add(gBrowser.tabContainer, "TabSelect", highlightsTabSelected);
		listenerAid.add(gBrowser, "DOMContentLoaded", highlightsContentLoaded);
		gBrowser.addTabsProgressListener(highlightsProgressListener);
		
		if(!perTabFB) {
			listenerAid.add(gBrowser.tabContainer, "TabOpen", highlightsTabOpened);
		}
	}
	
	moduleAid.load('highlightDoc');
	
	prefAid.listen('highlightByDefault', toggleHighlightByDefault);
	prefAid.listen('hideWhenFinderHidden', toggleHideOnClose);
	prefAid.listen('fillSelectedText', toggleFillSelectedText);
	
	toggleHighlightByDefault();
	toggleHideOnClose();
	toggleFillSelectedText();
};

moduleAid.UNLOADMODULE = function() {
	moduleAid.unload('fillSelectedText');
	moduleAid.unload('toggleHideOnClose');
	moduleAid.unload('highlightByDefault');
	
	prefAid.unlisten('highlightByDefault', toggleHighlightByDefault);
	prefAid.unlisten('hideWhenFinderHidden', toggleHideOnClose);
	prefAid.unlisten('fillSelectedText', toggleFillSelectedText);
	
	moduleAid.unload('highlightDoc');
	
	if(!viewSource) {
		// Clean up everything this module may have added to tabs and panels and documents
		for(var t=0; t<gBrowser.mTabs.length; t++) {
			var panel = $(gBrowser.mTabs[t].linkedPanel);
			delete panel._findWord;
			delete panel._statusUI;
			delete panel._neverCurrent;
			
			if(panel.linkedBrowser && panel.linkedBrowser.contentDocument) {
				listenerAid.remove(panel.linkedBrowser.contentDocument, 'keyup', escHighlights);
				removeAttribute(panel.linkedBrowser.contentDocument.documentElement, 'highlighted');
				removeAttribute(panel.linkedBrowser.contentDocument.documentElement, 'reHighlight');
			}
		}
		
		listenerAid.remove(window, 'UpdatedStatusFindBar', keepStatusUI);
		listenerAid.remove(gBrowser.tabContainer, "TabSelect", highlightsTabSelected);
		listenerAid.remove(gBrowser.tabContainer, "TabOpen", highlightsTabOpened);
		listenerAid.remove(gBrowser, "DOMContentLoaded", highlightsContentLoaded);
		gBrowser.removeTabsProgressListener(highlightsProgressListener);
	}
	else {
		delete linkedPanel._findWord;
		delete linkedPanel._statusUI;
		listenerAid.remove(contentDocument, 'keyup', escHighlights);
		removeAttribute(contentDocument.documentElement, 'highlighted');
		removeAttribute(contentDocument.documentElement, 'reHighlight');
	}
	
	observerAid.remove(reHighlightAll, 'ReHighlightAll');
	listenerAid.remove(window, 'WillUpdateStatusFindBar', emptyNoFindUpdating);
	listenerAid.remove(window, 'ClosedFindBar', highlightOnClose);
	listenerAid.remove(window, 'ClosedFindBarAnotherTab', highlightOnClose);
	listenerAid.remove(window, 'WillToggleHighlight', highlightsOnToggling);
	listenerAid.remove(window, 'WillFindAgain', highlightsFindAgain);
	listenerAid.remove(window, 'FoundAgain', highlightOnFindAgain);
	
	deinitFindBar('highlights');
};
