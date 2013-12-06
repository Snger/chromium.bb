// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function testAlwaysOnTop(testId, value) {
  var options = {
    id: testId,
    alwaysOnTop: value
  };

  chrome.app.window.create('index.html',
                           options,
                           callbackPass(function(win) {
    // Check that isAlwaysOnTop() returns false because the feature is not
    // permitted in Stable.
    chrome.test.assertEq(false, win.isAlwaysOnTop());

    // Attempt to use setAlwaysOnTop() to change the property. But this should
    // also fail.
    // TODO(tmdiep): The new value of isAlwaysOnTop() should be checked
    // asynchronously, but we need a non-flaky way to do this.
    win.setAlwaysOnTop(value);

    win.contentWindow.close();
  }));
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Try to enable alwaysOnTop on window creation and after window creation.
    function testAlwaysOnTopEnable() {
      testAlwaysOnTop('testAlwaysOnTopEnable', true);
    },

    // Try to disable alwaysOnTop on window creation and after window creation.
    function testAlwaysOnTopDisable() {
      testAlwaysOnTop('testAlwaysOnTopDisable', false);
    }

  ]);
});
