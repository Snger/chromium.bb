/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queue for running tests asynchronously.
 */
function TestRunner() {
  this.queue = [];
  keyboard.addEventListener('stateChange', this.onStateChange.bind(this));
}

TestRunner.prototype = {

  /**
   * Queues a test to run after the keyboard has finished initializing.
   * @param {!Function} callback The deferred function call.
   */
  append: function(callback) {
    this.queue.push(callback);
  },

  /**
   * Notification of a change in the state of the keyboard. Runs all queued
   * tests if the keyboard has finished initializing.
   * @param {Object} event The state change event.
   */
  onStateChange: function(event) {
    if (event.detail.state == 'keysetLoaded') {
      for (var i = 0; i < this.queue.length; i++) {
        var callback = this.queue[i];
        try {
          callback();
        } catch(err) {
          console.error('Failure in test ' + callback.name + '\n' + err);
          console.log(err.stack);
          callback.testFailure = true;
        }
      }
      this.queue = [];
    }
  },
};

var testRunner;
var mockController;

/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  testRunner = new TestRunner();
  mockController = new MockController();
  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'insertText');

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'sendKeyEvent');

  var validateSendCall = function(index, expected, observed) {
    // Only consider the first argument (VirtualKeyEvent) for the validation of
    // sendKeyEvent calls.
    var expectedEvent = expected[0];
    var observedEvent = observed[0];
    assertEquals(expectedEvent.type,
                 observedEvent.type,
                 'Mismatched event types.');
    assertEquals(expectedEvent.charValue,
                 observedEvent.charValue,
                 'Mismatched unicode values for character.');
    assertEquals(expectedEvent.keyCode,
                 observedEvent.keyCode,
                 'Mismatched key codes.');
    assertEquals(expectedEvent.shiftKey,
                 observedEvent.shiftKey,
                 'Mismatched states for shift modifier.');
  };
  chrome.virtualKeyboardPrivate.sendKeyEvent.validateCall = validateSendCall;

  // TODO(kevers): Mock additional extension API calls as required.
}

/**
 * Verify that API calls match expectations.
 */
function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
}

/**
 * Finds the key on the keyboard with the matching label.
 * @param {string} label The label in the key.
 * @return {?kb-key} The key element with matching label or undefined if no
 *     matching key is found.
 */
function findKey(label) {
  var keys = keyboard.querySelectorAll('kb-key');
  for (var i = 0; i < keys.length; i++) {
    if (keys[i].charValue == label)
      return keys[i];
  }
}

/**
 * Triggers a callback function to run post initialization of the virtual
 * keyboard.
 * @param {Function} callback The callback function.
 */
function onKeyboardReady(callback) {
  if (keyboard.initialized)
    callback();
  else
    testRunner.append(callback);
}
