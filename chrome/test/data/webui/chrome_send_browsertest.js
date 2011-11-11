// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that chrome.send mocking works as expected.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see test_api.js
 */

GEN('#include "chrome/test/data/webui/chrome_send_browsertest-inl.h"');

// Code that must be in a C++ file to pass clang rules.
GEN('ChromeSendWebUITest::ChromeSendWebUITest() {}');
GEN('ChromeSendWebUITest::~ChromeSendWebUITest() {}');
GEN('ChromeSendWebUITest::ChromeSendWebUIMessageHandler::');
GEN('    ChromeSendWebUIMessageHandler() {}');
GEN('ChromeSendWebUITest::ChromeSendWebUIMessageHandler::');
GEN('    ~ChromeSendWebUIMessageHandler() {}');
GEN('ChromeSendPassthroughWebUITest::ChromeSendPassthroughWebUITest() {}');
GEN('ChromeSendPassthroughWebUITest::~ChromeSendPassthroughWebUITest() {}');

/**
 * Test fixture for chrome send WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function ChromeSendWebUITest() {}

ChromeSendWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Generate a real C++ class; don't typedef.
   * @type {?string}
   * @override
   */
  typedefCppFixture: null,

  /** @inheritDoc */
  browsePreload: DUMMY_URL,

  /** @inheritDoc */
  setUp: function() {
    // Set up a mock handler class to catch the 'checkSend' message.
    function MockHandler() {}
    MockHandler.prototype = {
      checkSend: function() {},
    };
    this.mockHandler = mock(MockHandler);
    registerMockMessageCallbacks(this.mockHandler, MockHandler);
  }
};

// Test that chrome.send can be mocked outside the preLoad method.
TEST_F('ChromeSendWebUITest', 'NotInPreload', function() {
  this.mockHandler.expects(once()).checkSend();
  chrome.send('checkSend');
});

/**
 * Test fixture for chrome send WebUI testing with passthrough.
 * @constructor
 * @extends {ChromeSendWebUITest}
 */
function ChromeSendPassthroughWebUITest() {}

ChromeSendPassthroughWebUITest.prototype = {
  __proto__: ChromeSendWebUITest.prototype,
};

// Test that the mocked chrome.send can call the original.
TEST_F('ChromeSendPassthroughWebUITest', 'CanCallOriginal', function() {
  chrome.send('expectCheckSend');
  this.mockHandler.expects(once()).checkSend().
      will(callFunction(function() {
                          chrome.originalSend('checkSend');
                        }));
  chrome.send('checkSend');
});
