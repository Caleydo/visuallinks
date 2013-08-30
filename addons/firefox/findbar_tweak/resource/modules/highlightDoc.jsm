moduleAid.VERSION = '1.3.5';

this.alwaysUpdateStatusUI = function(e) {
	// toggleHighlight() doesn't update the UI in these conditions, we need it to, to update the counter (basically hide it)
	if(e.detail && !gFindBar._findField.value) {
		gFindBar._updateStatusUI(gFindBar.nsITypeAheadFind.FIND_FOUND);
	}
};

this.alwaysToggleHighlight = function() {
	// _find() doesn't toggleHighlight if checkbox is unchecked, we need it to, to update the counter
	if(!gFindBar.getElement("highlight").checked) {
              gFindBar._setHighlightTimeout();
	}
};

this.trackPDFMatches = function(e) {
	if(e && e.detail && e.detail.res == gFindBar.nsITypeAheadFind.FIND_PENDING) { return; }
	
	if(!isPDFJS) { return; }
	
	// Cancel another possible timer if it was scheduled before
	timerAid.cancel('trackPDFMatches');
	
	if(typeof(linkedPanel._matchesPDFtotal) == 'undefined') {
		linkedPanel._matchesPDFtotal = 0;
	}
	
	// We need this to access protected properties, hidden from privileged code
	var unWrap = XPCNativeWrapper.unwrap(contentWindow);
	
	// This usually means the matches are still being retrieved, however if this isn't true it still doesn't mean it's fully finished.
	// So later we set a timer to update itself after a while.
	if(!unWrap.PDFFindController.active || unWrap.PDFFindController.resumeCallback) {
		timerAid.init('trackPDFMatches', trackPDFMatches, 0);
		return;
	}
	
	// No matches
	if(!unWrap.PDFFindController.hadMatch) {
		if(linkedPanel._matchesPDFtotal > 0) {
			linkedPanel._matchesPDFtotal = 0;
			dispatch(gFindBar, { type: 'UpdatedPDFMatches', cancelable: false }); // We should still dispatch this
			return;
		}
	}
	
	var matches = unWrap.PDFFindController.pageMatches;
	var total = 0;
	for(var p=0; p<matches.length; p++) {
		total += matches[p].length;
	}
	
	if(linkedPanel._matchesPDFtotal != total) {
		linkedPanel._matchesPDFtotal = total;
		dispatch(gFindBar, { type: 'UpdatedPDFMatches', cancelable: false });
		
		// Because it might still not be finished, we should update later
		timerAid.init('trackPDFMatches', trackPDFMatches, 250);
	}
};

this.compareRanges = function(aRange, bRange) {
	if(aRange.nodeType || bRange.nodeType) { return false; } // Don't know if this could get here
	if(aRange.startContainer == bRange.startContainer
	&& aRange.endContainer == bRange.endContainer
	&& aRange.startOffset == bRange.startOffset
	&& aRange.endOffset == bRange.endOffset) {
		return true;
	}
	return false;
};

this.toggleCounter = function() {
	moduleAid.loadIf('counter', prefAid.useCounter);
};

this.toggleGrid = function() {
	moduleAid.loadIf('grid', prefAid.useGrid);
};

this.toggleSights = function() {
	moduleAid.loadIf('sights', prefAid.sightsCurrent || prefAid.sightsHighlights);
};

moduleAid.LOADMODULE = function() {
	// Add found words to counter and grid arrays if needed,
	// Modified to more accurately handle frames
	initFindBar('highlightDoc',
		function(bar) {
			bar.__highlightDoc = bar._highlightDoc;
			bar._highlightDoc = function _highlightDoc(aHighlight, aWord, aWindow, aLevel, aSights, innerRanges) {
				if(!aWindow) {
					// Using the counter?
					linkedPanel._counterHighlights = null;
					if(prefAid.useCounter) {
						var counterLevels = [];
						aLevel = counterLevels;
					}
					
					// Using the grid?
					var fillGrid = false;
					innerRanges = null;
					if(prefAid.useGrid) {
						var toAddtoGrid = [];
						fillGrid = resetHighlightGrid();
					}
					
					// Using the sights?
					if(prefAid.sightsHighlights) {
						aSights = [];
					}
				}
				
				// Prepare counter levels for every frame, later we add them in order (frames last)
				if(aLevel) {
					aLevel.push({ highlights: [], levels: [] });
					var thisLevel = aLevel.length -1;
				}
				
				var win = aWindow || this.browser.contentWindow;
				var textFound = false;
				for(var i = 0; win.frames && i < win.frames.length; i++) {
					var nextLevel = null;
					if(aLevel) { nextLevel = aLevel[thisLevel].levels; }
					if(!aWindow && fillGrid) { innerRanges = new Array(); }
					
					if(this._highlightDoc(aHighlight, aWord, win.frames[i], nextLevel, aSights, innerRanges)) {
						textFound = true;
						
						// Frames get a pattern in the grid, with the whole extesion of the frame instead of just the highlight,
						// frames can be scrolled so the highlights position may not reflect their actual page position, they may not even be visible at the moment.
						if(aHighlight && !aWindow && fillGrid) {
							// Arrays are always live objects
							var frameRanges = new Array();
							for(var r=0; r<innerRanges.length; r++) {
								frameRanges.push(innerRanges[r]);
							}
							fillGrid = (toAddtoGrid.push({ node: win.frames[i].frameElement, pattern: true, ranges: frameRanges }) <= prefAid.gridLimit);
						}
					}
					
					if(aLevel) { aLevel[thisLevel].levels = nextLevel; }
				}
				
				var controller = this._getSelectionController(win);
				if(!controller) {
					// Without the selection controller,
					// we are unable to (un)highlight any matches
					return textFound;
				}
				
				var doc = win.document;
				if(!doc || !(doc instanceof window.HTMLDocument) || !doc.body) {
					return textFound;
				}
				
				// Bugfix: when using neither the highlights nor the counter, toggling the highlights off would trigger the "Phrase not found" status
				// because textFound would never have had the chance to be verified. This doesn't need to happen if a frame already triggered the found status.
				if(aHighlight || aLevel || !textFound) {
					var searchRange = doc.createRange();
					searchRange.selectNodeContents(doc.body);
					
					var startPt = searchRange.cloneRange();
					startPt.collapse(true);
					
					var endPt = searchRange.cloneRange();
					endPt.collapse(false);
					
					var retRange = null;
					var finder = Components.classes['@mozilla.org/embedcomp/rangefind;1'].createInstance().QueryInterface(Components.interfaces.nsIFind);
					finder.caseSensitive = this._shouldBeCaseSensitive(aWord);
					
					while((retRange = finder.Find(aWord, searchRange, startPt, endPt))) {
						textFound = true;
						
						// We can stop now if all we're looking for is the found status
						if(!aHighlight && !aLevel) { break; }
						
						startPt = retRange.cloneRange();
						startPt.collapse(false);
						
						if(aHighlight) {
							this._highlight(retRange, controller);
						
							if(!aWindow && fillGrid) {
								var editableNode = this._getEditableNode(retRange.startContainer);
								if(editableNode) {
									fillGrid = (toAddtoGrid.push({ node: editableNode, pattern: true, ranges: new Array(retRange) }) <= prefAid.gridLimit);
								} else {
									fillGrid = (toAddtoGrid.push({ node: retRange, pattern: false }) <= prefAid.gridLimit);
								}
							}
							else if(innerRanges) {
								innerRanges.push(retRange);
							}
							
							if(aSights) {
								aSights.push({ node: retRange, sights: false });
							}
						}
						
						// Always add to counter, whether we are highlighting or not
						if(aLevel) {
							aLevel[thisLevel].highlights.push(retRange);
						}
					}
				}
				
				if(!aWindow) {
					if(aLevel && aLevel == counterLevels) {
						var counterHighlights = [];
						moveHighlightsArray(counterLevels, counterHighlights);
						linkedPanel._counterHighlights = counterHighlights;
					}
					
					if(fillGrid) {
						fillHighlightGrid(toAddtoGrid);
					}
					
					if(aSights) {
						sightsOnVisibleHighlights(aSights);
					}
					
					if(textFound) {
						// Never take attention from current hit
						var curSel = controller.getSelection(gFindBar.nsISelectionController.SELECTION_NORMAL);
						if(curSel.rangeCount == 1) {
							controller.setDisplaySelection(controller.SELECTION_ATTENTION);
						}
					}
				}
				
				if(!aHighlight) {
					// First, attempt to remove highlighting from main document
					var sel = controller.getSelection(this._findSelection);
					sel.removeAllRanges();
					
					// Next, check our editor cache, for editors belonging to this
					// document
					if(this._editors) {
						for(var x = this._editors.length - 1; x >= 0; --x) {
							if(this._editors[x].document == doc) {
								sel = this._editors[x].selectionController.getSelection(this._findSelection);
								sel.removeAllRanges();
								// We don't need to listen to this editor any more
								this._unhookListenersAtIndex(x);
							}
						}
					}
				}
				
				return textFound;
			};
		},
		function(bar) {
			bar._highlightDoc = bar.__highlightDoc;
			delete bar.__highlightDoc;
		}
	);

	
	listenerAid.add(window, 'ToggledHighlight', alwaysUpdateStatusUI);
	listenerAid.add(window, 'FoundFindBar', alwaysToggleHighlight);
	listenerAid.add(window, 'UpdatedStatusFindBar', trackPDFMatches);
	
	prefAid.listen('useCounter', toggleCounter);
	prefAid.listen('useGrid', toggleGrid);
	prefAid.listen('sightsCurrent', toggleSights);
	prefAid.listen('sightsHighlights', toggleSights);
	
	toggleCounter();
	toggleGrid();
	toggleSights();
};

moduleAid.UNLOADMODULE = function() {
	prefAid.unlisten('useCounter', toggleCounter);
	prefAid.unlisten('useGrid', toggleGrid);
	prefAid.unlisten('sightsCurrent', toggleSights);
	prefAid.unlisten('sightsHighlights', toggleSights);
	
	moduleAid.unload('sights');
	moduleAid.unload('grid');
	moduleAid.unload('counter');
	
	listenerAid.remove(window, 'ToggledHighlight', alwaysUpdateStatusUI);
	listenerAid.remove(window, 'FoundFindBar', alwaysToggleHighlight);
	listenerAid.remove(window, 'UpdatedStatusFindBar', trackPDFMatches);
	
	if(!viewSource) {
		// Clean up everything this module may have added to tabs and panels and documents
		for(var t=0; t<gBrowser.mTabs.length; t++) {
			var panel = $(gBrowser.mTabs[t].linkedPanel);
			delete panel._counterHighlights;
			delete panel._currentHighlight;
			delete panel._matchesPDFtotal;
		}
	} else {
		delete linkedPanel._counterHighlights;
		delete linkedPanel._currentHighlight;
	}
	
	deinitFindBar('highlightDoc');
};
