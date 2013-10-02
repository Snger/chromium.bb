/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Mocks using the longpress candidate window to enter an alternate character.
 * @param {string} label The label on the key.
 * @param {string} candidateLabel The alternate character being typed.
 * @param {number} keyCode The keyCode for the alternate character, which may
 *     be zero for characters not found on a QWERTY keyboard.
 * @param {boolean} shiftModifier Indicates the state of the shift key when
 *     entering the character.
 * @param {Object.<string, boolean>=} opt_variant Optional test variant.
 */
function mockLongPressType(label,
                           candidateLabel,
                           keyCode,
                           shiftModifier,
                           opt_variant) {
  // Verify that candidates popup window is initally hidden.
  var keyset = keyboard.querySelector('#' + keyboard.activeKeysetId);
  assertTrue(!!keyset, 'Unable to find active keyset.');
  var candidatesPopup = keyset.querySelector('kb-altkey-container');
  assertTrue(!!candidatesPopup, 'Unable to find altkey container.');
  assertTrue(candidatesPopup.hidden,
             'Candidate popup should be hidden initially.');

  // Show candidate window of alternate keys on a long press.
  var key = findKey(label);
  var altKey = null;
  assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
  key.down();
  mockTimer.tick(1000);
  if (opt_variant && opt_variant.noCandidates) {
    assertTrue(candidatesPopup.hidden, 'Candidate popup should remain hidden.');
  } else {
    assertFalse(candidatesPopup.hidden, 'Candidate popup should be visible.');

    // Verify that the popup window contains the candidate key.
    var candidates = candidatesPopup.querySelectorAll('kb-altkey');
    for (var i = 0; i < candidates.length; i++) {
       if (candidates[i].innerText == candidateLabel) {
         altKey = candidates[i];
         break;
       }
    }
    assertTrue(!!altKey, 'Unable to find key in candidate list.');
  }

  var abortSelection = opt_variant && opt_variant.abortSelection;
  if (!abortSelection) {
    // Verify that the candidate key is typed on release of the longpress.
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    var unicodeValue = candidateLabel.charCodeAt(0);
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: keyCode,
      shiftKey: shiftModifier
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: keyCode,
      shiftKey: shiftModifier
    });
  }
  if (altKey) {
    altKey.over({relatedTarget: altKey});
    if (abortSelection)
      altKey.out({relatedTarget: altKey});
    else
      altKey.up();

    // Verify that the candidate list is hidden on a pointer up event.
    candidatesPopup.up();
    assertTrue(candidatesPopup.hidden,
               'Candidate popup should be hidden after inserting a character.');
  } else {
    key.up();
  }
}

/**
 * Tests that typing characters on the default lowercase keyboard triggers the
 * correct sequence of events. The test is run asynchronously since the
 * keyboard loads keysets dynamically.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  var runTest = function() {
    // Keyboard defaults to lowercase.
    mockTypeCharacter('a', 0x41, false);
    mockTypeCharacter('s', 0x53, false);
    mockTypeCharacter('.', 0xBE, false);
    mockTypeCharacter('\b', 0x08, false, 0x08);
    mockTypeCharacter('\t', 0x09, false, 0x09);
    mockTypeCharacter('\n', 0x0D, false, 0x0A);
    mockTypeCharacter(' ', 0x20, false);
  };
  onKeyboardReady('testLowercaseKeysetAsync', runTest, testDoneCallback);
}

/**
 * Tests long press on a key that has alternate sugestions. For example,
 * longpressing the 'a' key displays 'a acute' 'a grave', etc. Longpressing
 * characters on the top row of the keyboard displays numbers as alternatives.
 */
function testLongPressTypeAccentedCharacterAsync(testDoneCallback) {
  var runTest = function() {
    // Test popup for letters with candidate lists that are derived from a
    // single source (hintText or accents).
    // Type lowercase A grave
    mockLongPressType('a', '\u00E0', 0, false);
    // Type the digit '1' (hintText on 'q' key).
    mockLongPressType('q', '1', 0x31, false);

    // Test popup for letter that has a candidate list combining hintText and
    // accented letters.
    // Type lowercase E acute.
    mockLongPressType('e', '\u00E9', 0, false);
    // Type the digit '3' (hintText on the 'e' key).
    mockLongPressType('e', '3', 0x33, false);

    // Mock longpress typing a character that does not have alternate
    // candidates.
    mockLongPressType('z', 'z', 0x5A, false, {noCandidates: true});

    // Mock aborting a longpress selection.
    mockLongPressType('e', '3', 0x33, false, {abortSelection: true});
  };
  onKeyboardReady('testLongPressTypeAccentedCharacterAsync',
                  runTest,
                  testDoneCallback);
}
