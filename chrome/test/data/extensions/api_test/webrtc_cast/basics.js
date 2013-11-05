// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function udpTransport() {
    // TODO(hclam): Remove this line when the bug with build bot is
    // fixed. See crbug.com/315169 and crbug.com/314971.
    chrome.test.succeed();
    return;

    chrome.webrtc.castUdpTransport.create(function(info) {
      chrome.webrtc.castUdpTransport.start(
          info.transportId,
          {address: "127.0.0.1", port: 60613});
      chrome.webrtc.castUdpTransport.stop(info.transportId);
      chrome.webrtc.castUdpTransport.destroy(info.transportId);
      chrome.test.succeed();
    });
  }
]);
