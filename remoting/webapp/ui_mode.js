// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to controlling the modal UI state of the app. UI states
 * are expressed as HTML attributes with a dotted hierarchy. For example, the
 * string 'host.shared' will match any elements with an associated attribute
 * of 'host' or 'host.shared', showing those elements and hiding all others.
 * Elements with no associated attribute are ignored.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @enum {string} */
remoting.AppMode = {
  HOME: 'home',
  UNAUTHENTICATED: 'auth',
  CLIENT: 'client',
    CLIENT_UNCONNECTED: 'client.unconnected',
    CLIENT_PIN_PROMPT: 'client.pin-prompt',
    CLIENT_CONNECTING: 'client.connecting',
    CLIENT_CONNECT_FAILED_IT2ME: 'client.connect-failed.it2me',
    CLIENT_CONNECT_FAILED_ME2ME: 'client.connect-failed.me2me',
    CLIENT_SESSION_FINISHED_IT2ME: 'client.session-finished.it2me',
    CLIENT_SESSION_FINISHED_ME2ME: 'client.session-finished.me2me',
  HOST: 'host',
    HOST_WAITING_FOR_CODE: 'host.waiting-for-code',
    HOST_WAITING_FOR_CONNECTION: 'host.waiting-for-connection',
    HOST_SHARED: 'host.shared',
    HOST_SHARE_FAILED: 'host.share-failed',
    HOST_SHARE_FINISHED: 'host.share-finished',
  IN_SESSION: 'in-session'
};

/**
 * Update the DOM by showing or hiding elements based on whether or not they
 * have an attribute matching the specified name.
 * @param {string} mode The value against which to match the attribute.
 * @param {string} attr The attribute name to match.
 * @return {void} Nothing.
 */
remoting.updateModalUi = function(mode, attr) {
  var modes = mode.split('.');
  for (var i = 1; i < modes.length; ++i)
    modes[i] = modes[i - 1] + '.' + modes[i];
  var elements = document.querySelectorAll('[' + attr + ']');
  for (var i = 0; i < elements.length; ++i) {
    var element = /** @type {Element} */ elements[i];
    var hidden = true;
    for (var m = 0; m < modes.length; ++m) {
      if (hasClass(element.getAttribute(attr), modes[m])) {
        hidden = false;
        break;
      }
    }
    element.hidden = hidden;
  }
}

/**
 * @type {remoting.AppMode} The current app mode
 */
remoting.currentMode = remoting.AppMode.HOME;

/**
 * Change the app's modal state to |mode|, determined by the data-ui-mode
 * attribute.
 *
 * @param {remoting.AppMode} mode The new modal state.
 */
remoting.setMode = function(mode) {
  remoting.updateModalUi(mode, 'data-ui-mode');
  remoting.debug.log('App mode: ' + mode);
  remoting.currentMode = mode;
  if (mode == remoting.AppMode.IN_SESSION) {
    document.removeEventListener('keydown', remoting.DebugLog.onKeydown, false);
  } else {
    document.addEventListener('keydown', remoting.DebugLog.onKeydown, false);
  }
  if (mode == remoting.AppMode.HOME) {
    var display = function() { remoting.hostList.display(); };
    remoting.hostList.refresh(display);
  }
};

/**
 * Get the major mode that the app is running in.
 * @return {string} The app's current major mode.
 */
remoting.getMajorMode = function() {
  return remoting.currentMode.split('.')[0];
};
