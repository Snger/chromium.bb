// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string}
 * @const
 */
var FEEDBACK_LANDING_PAGE =
    'https://www.google.com/support/chrome/go/feedback_confirmation';
/** @type {number}
 * @const
 */
var MAX_ATTACH_FILE_SIZE = 3 * 1024 * 1024;

var attachedFileBlob = null;
var lastReader = null;

var feedbackInfo = null;
var systemInfo = null;

/**
 * Reads the selected file when the user selects a file.
 * @param {Event} fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent) {
  $('attach-error').hidden = true;
  var file = fileSelectedEvent.target.files[0];
  if (!file) {
    // User canceled file selection.
    attachedFileBlob = null;
    return;
  }

  if (file.size > MAX_ATTACH_FILE_SIZE) {
    $('attach-error').hidden = false;

    // Clear our selected file.
    $('attach-file').value = '';
    attachedFileBlob = null;
    return;
  }

  attachedFileBlob = file.slice();
}

/**
 * Opens a new tab with chrome://system, showing the current system info.
 */
function openSystemTab() {
  window.open('chrome://system', '_blank');
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return {boolean} True if the report was sent.
 */
function sendReport() {
  if ($('description-text').value.length == 0) {
    $('description-text').placeholder =
        loadTimeData.getString('no-description');
    return false;
  }

  console.log('Feedback: Sending report');
  if (attachedFileBlob) {
    feedbackInfo.attachedFile = { name: $('attach-file').value,
                                  data: attachedFileBlob };
  }

  feedbackInfo.description = $('description-text').value;
  feedbackInfo.pageUrl = $('page-url-text').value;
  feedbackInfo.email = $('user-email-text').value;

  if ($('sys-info-checkbox') != null &&
      $('sys-info-checkbox').checked &&
      systemInfo != null) {
    if (feedbackInfo.systemInformation != null) {
      // Concatenate sysinfo if we had any initial system information
      // sent with the feedback request event.
      feedbackInfo.systemInformation =
          feedbackInfo.systemInformation.concat(systemInfo);
    } else {
      feedbackInfo.systemInformation = systemInfo;
    }
  }

  chrome.feedbackPrivate.sendFeedback(feedbackInfo, function(result) {
    window.open(FEEDBACK_LANDING_PAGE, '_blank');
    window.close();
  });

  return true;
}

/**
 * Click listener for the cancel button.
 * @param {Event} e The click event being handled.
 */
function cancel(e) {
  e.preventDefault();
  window.close();
}

/**
 * Converts a blob data URL to a blob object.
 * @param {string} url The data URL to convert.
 * @return {Blob} Blob object containing the data.
 */
function dataUrlToBlob(url) {
  var mimeString = url.split(',')[0].split(':')[1].split(';')[0];
  var data = atob(url.split(',')[1]);
  var dataArray = [];
  for (var i = 0; i < data.length; ++i)
    dataArray.push(data.charCodeAt(i));

  return new Blob([new Uint8Array(dataArray)], {type: mimeString});
}

/**
 * Initializes our page.
 * Flow:
 * .) DOMContent Loaded        -> . Request feedbackInfo object
 *                                . Setup page event handlers
 * .) Feedback Object Received -> . take screenshot
 *                                . request email
 *                                . request System info
 *                                . request i18n strings
 * .) Screenshot taken         -> . Show Feedback window.
 */
function initialize() {
  // Add listener to receive the feedback info object.
  chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
    if (request.sentFromEventPage) {
      feedbackInfo = request.data;
      $('description-text').textContent = feedbackInfo.description;
      $('page-url-text').value = feedbackInfo.pageUrl;

      takeScreenshot(function(screenshotDataUrl) {
        $('screenshot-image').src = screenshotDataUrl;
        feedbackInfo.screenshot = dataUrlToBlob(screenshotDataUrl);
        chrome.app.window.current().show();
      });

      chrome.feedbackPrivate.getUserEmail(function(email) {
        $('user-email-text').value = email;
      });

      chrome.feedbackPrivate.getSystemInformation(function(sysInfo) {
        systemInfo = sysInfo;
      });

      chrome.feedbackPrivate.getStrings(function(strings) {
        loadTimeData.data = strings;
        i18nTemplate.process(document, loadTimeData);
      });
    }
  });

  window.addEventListener('DOMContentLoaded', function() {
    // Ready to receive the feedback object.
    chrome.runtime.sendMessage({ready: true});

    // Setup our event handlers.
    $('attach-file').addEventListener('change', onFileSelected);
    $('send-report-button').onclick = sendReport;
    $('cancel-button').onclick = cancel;
    if ($('sysinfo-url')) {
      $('sysinfo-url').onclick = openSystemTab;
    }
  });
}

initialize();
