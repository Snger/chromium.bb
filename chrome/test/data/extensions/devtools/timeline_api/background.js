// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var receivedEvents = [];
var devtoolsTabEvents = undefined;

function pageEventListener() {
  receivedEvents.push("onPageEvent");
}

function tabCloseListener() {
  receivedEvents.push("onTabClose");
}

function setListenersOnTab(tabId) {
  try {
    devtoolsTabEvents = chrome.devtools.getTabEvents(tabId);
    devtoolsTabEvents.onPageEvent.addListener(pageEventListener);
    devtoolsTabEvents.onTabClose.addListener(tabCloseListener);
    window.domAutomationController.send(true);
  } catch(e) {
    window.domAutomationController.send(false);
  }
}

function testReceivePageEvent() {
  var sawPage = false;
  if (receivedEvents.length == 1) {
    var eventName = receivedEvents.pop();
    if (eventName === "onPageEvent")
      sawPage = true;
    else
      console.warn('received ' + eventName + '; expecting onPageEvent');
  } else {
    console.warn('received ' + receivedEvents.toString() +
                 '; expecting onPageEvent');
    receivedEvents = [];
  }
  window.domAutomationController.send(sawPage);
}

function testReceiveTabCloseEvent() {
  var sawTabClose = false;
  for(var i = 0; i < receivedEvents.length; i++) {
    if (receivedEvents[i] === 'onTabClose') {
      sawTabClose = true;
      break;
    }
  }
  receivedEvents = [];
  window.domAutomationController.send(sawTabClose);
}

function unregisterListeners() {
  devtoolsTabEvents.onPageEvent.removeListener(pageEventListener);
  devtoolsTabEvents.onTabClose.removeListener(tabCloseListener);
  window.domAutomationController.send(true);
}
