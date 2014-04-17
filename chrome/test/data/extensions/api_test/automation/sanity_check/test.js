// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

// Do not test orientation or hover attributes (similar to exclusions on native
// accessibility), since they can be inconsistent depending on the environment.
var RemoveUntestedStates = function(state) {
  delete state['horizontal'];
  delete state['hovered'];
  delete state['vertical'];
};

var allTests = [
  function testSimplePage() {
    function makeAssertions(tree) {
      var title = tree.root.attributes['ax_attr_doc_title'];
      assertEq('Automation Tests', title);
      RemoveUntestedStates(tree.root.state);
      assertEq(
          {enabled: true, focusable: true, read_only: true},
          tree.root.state);
      var children = tree.root.children();
      assertEq(1, children.length);

      var body = children[0];
      assertEq('body', body.attributes['ax_attr_html_tag']);

      RemoveUntestedStates(body.state);
      assertEq({enabled: true, read_only: true},
               body.state);

      var contentChildren = body.children();
      assertEq(3, contentChildren.length);
      var okButton = contentChildren[0];
      assertEq('Ok', okButton.attributes['ax_attr_name']);
      RemoveUntestedStates(okButton.state);
      assertEq({enabled: true, focusable: true, read_only: true},
               okButton.state);
      var userNameInput = contentChildren[1];
      assertEq('Username',
               userNameInput.attributes['ax_attr_description']);
      RemoveUntestedStates(userNameInput.state);
      assertEq({enabled: true, focusable: true},
               userNameInput.state);
      var cancelButton = contentChildren[2];
      assertEq('Cancel',
               cancelButton.attributes['ax_attr_name']);
      RemoveUntestedStates(cancelButton.state);
      assertEq({enabled: true, focusable: true, read_only: true},
               cancelButton.state);

      // Traversal.
      assertEq(undefined, tree.root.parent());
      assertEq(tree.root, body.parent());

      assertEq(body, tree.root.firstChild());
      assertEq(body, tree.root.lastChild());

      assertEq(okButton, body.firstChild());
      assertEq(cancelButton, body.lastChild());

      assertEq(body, okButton.parent());
      assertEq(body, userNameInput.parent());
      assertEq(body, cancelButton.parent());

      assertEq(undefined, okButton.previousSibling());
      assertEq(undefined, okButton.firstChild());
      assertEq(userNameInput, okButton.nextSibling());
      assertEq(undefined, okButton.lastChild());

      assertEq(okButton, userNameInput.previousSibling());
      assertEq(cancelButton, userNameInput.nextSibling());

      assertEq(userNameInput, cancelButton.previousSibling());
      assertEq(undefined, cancelButton.nextSibling());

      chrome.test.succeed();
    };

    chrome.tabs.query({active: true}, function(tabs) {
      assertEq(1, tabs.length);
      chrome.tabs.update(tabs[0].id, {url: 'test.html'}, function(tab) {
        chrome.runtime.onMessage.addListener(
                               function listener(message, sender) {
          if (!sender.tab)
            return;
          assertEq(tab.id, sender.tab.id);
          assertTrue(message['loaded']);
          chrome.automation.getTree(makeAssertions);
          chrome.runtime.onMessage.removeListener(listener);
        });
      });
    });
  },
  function testEventListenerTarget() {
    chrome.automation.getTree(function(tree) {
      var cancelButton = tree.root.firstChild().children()[2];
      assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
      var eventListenerCalledAlready = false;
      cancelButton.addEventListener('focus', function onFocus(event) {
        if (eventListenerCalledAlready)
          chrome.test.fail('Event listener called twice');
        eventListenerCalledAlready = true;
        window.setTimeout(function() {
          chrome.test.succeed();
          cancelButton.removeEventListener('focus', onFocus);
        }, 0);
      });
      cancelButton.focus();
    });
  },
  function testEventListenerBubble() {
    chrome.automation.getTree(function(tree) {
      var cancelButton = tree.root.firstChild().children()[2];
      assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
      var cancelButtonGotEvent = false;
      cancelButton.addEventListener('focus', function onFocus(event) {
        cancelButtonGotEvent = true;
        cancelButton.removeEventListener('focus', onFocus);
      });
      tree.root.addEventListener('focus', function onFocusRoot(event) {
        assertEq('focus', event.type);
        assertEq(cancelButton, event.target);
        assertTrue(cancelButtonGotEvent);
        tree.root.removeEventListener('focus', onFocusRoot);
        chrome.test.succeed();
      });
      cancelButton.focus();
    });
  },
  function testStopPropagation() {
     chrome.automation.getTree(function(tree) {
      var cancelButton = tree.root.firstChild().children()[2];
      assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
      cancelButton.addEventListener('focus', function onFocus(event) {
        cancelButton.removeEventListener('focus', onFocus);
        event.stopPropagation();
        window.setTimeout(chrome.test.succeed.bind(this), 0);
      });
      tree.root.addEventListener('focus', function onFocusRoot(event) {
        tree.root.removeEventListener('focus', onFocusRoot);
        chrome.test.fail("Focus event was propagated to root");
      });
      cancelButton.focus();
    });
  },
  function testEventListenerCapture() {
    chrome.automation.getTree(function(tree) {
      var cancelButton = tree.root.firstChild().children()[2];
      assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
      var cancelButtonGotEvent = false;
      cancelButton.addEventListener('focus', function onFocus(event) {
        cancelButtonGotEvent = true;
        cancelButton.removeEventListener('focus', onFocus);
        chrome.test.fail("Focus event was not captured by root");
      });
      tree.root.addEventListener('focus', function onFocusRoot(event) {
        assertEq('focus', event.type);
        assertEq(cancelButton, event.target);
        assertFalse(cancelButtonGotEvent);
        event.stopPropagation();
        tree.root.removeEventListener('focus', onFocusRoot);
        window.setTimeout(chrome.test.succeed.bind(this), 0);
      }, true);
      cancelButton.focus();
    });
  },
  function testSimpleAction() {
    function makeAssertions(tree) {
      var okButton = tree.root.firstChild().firstChild();
      okButton.addEventListener('focus', function() {
        chrome.test.succeed();
      }, true);
      okButton.focus();
    }
    chrome.tabs.create({url: 'test.html'});
    chrome.automation.getTree(function(tree) {
      tree.root.addEventListener('load_complete',
          function() {
            makeAssertions(tree);
          }, true);
    });
  },
  function cantChangeEventReadonlyParams() {
    chrome.automation.getTree(function(tree) {
      var cancelButton = tree.root.firstChild().children()[2];
      assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
      cancelButton.addEventListener('focus', function onFocus(event) {
        assertEq('focus', event.type);
        assertEq(cancelButton, event.target);
        assertEq(Event.AT_TARGET, event.eventPhase);
        event.type = 'bad_type';
        assertEq('focus', event.type);
        event.target = tree.root;
        assertEq(cancelButton, event.target);
        event.eventPhase = Event.NONE;
        assertEq(Event.AT_TARGET, event.eventPhase);
        chrome.test.succeed();
      });
      cancelButton.focus();
    });
  },
];

chrome.test.runTests(allTests);
