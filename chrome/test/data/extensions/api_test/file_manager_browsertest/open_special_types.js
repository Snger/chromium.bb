// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if the gallery shows up for the selected image and that the image
 * gets displayed.
 *
 * @param {string} path Directory path to be tested.
 */
function galleryOpen(path) {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Resize the window to desired dimensions to avoid flakyness.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('resizeWindow',
                         appId,
                         [320, 320],
                         this.next);
    },
    // Select the image.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('openFile',
                         appId,
                         ['My Desktop Background.png'],
                         this.next);
    },
    // Wait for the image in the gallery's screen image.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.image',
                          'iframe.overlay-pane'],
                         this.next);
    },
    // Verify the gallery's screen image.
    function(element) {
      chrome.test.assertEq('320', element.attributes.width);
      chrome.test.assertEq('240', element.attributes.height);
      // Get the full-resolution image.
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.fullres',
                          'iframe.overlay-pane'],
                         this.next);
    },
    // Verify the gallery's full resolution image.
    function(element) {
      chrome.test.assertEq('800', element.attributes.width);
      chrome.test.assertEq('600', element.attributes.height);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player shows up for the selected image and that the audio
 * is loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
function audioOpen(path) {
  var appId;
  var audioAppId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the audio player.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['audio_player.html'],
                         this.next);
    },
    // Wait for the audio tag and verify the source.
    function(inAppId) {
      audioAppId = inAppId;
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['audio-player[playing]'],
                         this.next);
    },
    // Get the title tag.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
          element.attributes.currenttrackurl);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the video player shows up for the selected movie and that it is
 * loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
function videoOpen(path) {
  var appId;
  var videoAppId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'openFile', appId, ['world.ogv'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the video player.
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['video_player.html'],
                         this.next);
    },
    function(inAppId) {
      videoAppId = inAppId;
      // Wait for the video tag and verify the source.
      callRemoteTestUtil('waitForElement',
                         videoAppId,
                         ['video[src]'],
                         this.next);
    },
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/world.ogv',
          element.attributes.src);
      // Wait for the window's inner dimensions. Should be changed to the video
      // size once the metadata is loaded.
      callRemoteTestUtil('waitForWindowGeometry',
                         videoAppId,
                         [320, 192],
                         this.next);
    },
    function(element) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.galleryOpenDownloads = function() {
  galleryOpen(RootPath.DOWNLOADS);
};

testcase.audioOpenDownloads = function() {
  audioOpen(RootPath.DOWNLOADS);
};

testcase.videoOpenDownloads = function() {
  videoOpen(RootPath.DOWNLOADS);
};

testcase.galleryOpenDrive = function() {
  galleryOpen(RootPath.DRIVE);
};

testcase.audioOpenDrive = function() {
  audioOpen(RootPath.DRIVE);
};

testcase.videoOpenDrive = function() {
  videoOpen(RootPath.DRIVE);
};
