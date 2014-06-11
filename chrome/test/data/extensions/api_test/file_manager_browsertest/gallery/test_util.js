// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<string>} names File name list.
 * @return {Promise} Promise to be fulflled with Array.<FileEntry>.
 */
function getFilesUnderVolume(volumeType, names) {
  var displayRootPromise = backgroundComponentsPromise.then(
      function(backgroundComponent) {
        var volumeManager = backgroundComponent.volumeManager;
        var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
        return new Promise(function(fulfill, reject) {
          volumeInfo.resolveDisplayRoot(fulfill, reject);
        });
      });
  return displayRootPromise.then(function(displayRoot) {
    var filesPromise = names.map(function(name) {
      return new Promise(
          displayRoot.getFile.bind(displayRoot, name, {}));
    });
    return Promise.all(filesPromise);
  });
}

/**
 * Waits until an element appears and returns it.
 *
 * @param {AppWindow} appWindow Application window.
 * @param {string} query Query for the element.
 * @return {Promise} Promise to be fulfilled with the element.
 */
function waitForElement(appWindow, query) {
  return repeatUntil(function() {
    var element = appWindow.contentWindow.document.querySelector(query);
    if (element)
      return element;
    else
      return pending('The element %s is not found.', query);
  });
}

/**
 * Launches the Gallery app with the test entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array.<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 */
function launchWithTestEntries(
    testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    return getFilesUnderVolume(
        volumeType,
        selectedEntries.map(function(entry) { return entry.nameText; }));
  });
  return launch(entriesPromise).then(function() {
    return appWindowPromise.then(function(appWindow) {
      return {appWindow: appWindow, entries: entries};
    });
  });
}
