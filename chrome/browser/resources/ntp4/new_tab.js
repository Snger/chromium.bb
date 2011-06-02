// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview New tab page
 * This is the main code for the new tab page used by touch-enabled Chrome
 * browsers.  For now this is still a prototype.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('ntp4', function() {
  'use strict';

  /**
   * The CardSlider object to use for changing app pages.
   * @type {CardSlider|undefined}
   */
  var cardSlider;

  /**
   * The 'page-list' element.
   * @type {!Element|undefined}
   */
  var pageList;

  /**
   * A list of all 'tile-page' elements.
   * @type {!NodeList|undefined}
   */
  var tilePages;

  /**
   * The Most Visited page.
   * @type {!Element|undefined}
   */
  var mostVisitedPage;

  /**
   * A list of all 'apps-page' elements.
   * @type {!NodeList|undefined}
   */
  var appsPages;

  /**
   * The 'dots-list' element.
   * @type {!Element|undefined}
   */
  var dotList;

  /**
   * A list of all 'dots' elements.
   * @type {!NodeList|undefined}
   */
  var dots;

  /**
   * The 'trash' element.  Note that technically this is unnecessary,
   * JavaScript creates the object for us based on the id.  But I don't want
   * to rely on the ID being the same, and JSCompiler doesn't know about it.
   * @type {!Element|undefined}
   */
  var trash;

  /**
   * The time in milliseconds for most transitions.  This should match what's
   * in new_tab.css.  Unfortunately there's no better way to try to time
   * something to occur until after a transition has completed.
   * @type {number}
   * @const
   */
  var DEFAULT_TRANSITION_TIME = 500;

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function initialize() {
    // Load the current theme colors.
    themeChanged();

    dotList = getRequiredElement('dot-list');
    pageList = getRequiredElement('page-list');
    trash = getRequiredElement('trash');
    trash.hidden = true;

    // Request data on the apps so we can fill them in.
    // Note that this is kicked off asynchronously.  'getAppsCallback' will be
    // invoked at some point after this function returns.
    chrome.send('getApps');

    // Prevent touch events from triggering any sort of native scrolling
    document.addEventListener('touchmove', function(e) {
      e.preventDefault();
    }, true);

    dots = dotList.getElementsByClassName('dot');
    tilePages = pageList.getElementsByClassName('tile-page');
    appsPages = pageList.getElementsByClassName('apps-page');

    // Initialize the cardSlider without any cards at the moment
    var sliderFrame = getRequiredElement('card-slider-frame');
    cardSlider = new CardSlider(sliderFrame, pageList, [], 0,
                                sliderFrame.offsetWidth);
    cardSlider.initialize();

    // Ensure the slider is resized appropriately with the window
    window.addEventListener('resize', function() {
      cardSlider.resize(sliderFrame.offsetWidth);
    });

    // Handle the page being changed
    pageList.addEventListener(
        CardSlider.EventType.CARD_CHANGED,
        function(e) {
          // Update the active dot
          var curDot = dotList.getElementsByClassName('selected')[0];
          if (curDot)
            curDot.classList.remove('selected');
          var newPageIndex = e.cardSlider.currentCard;
          dots[newPageIndex].classList.add('selected');
        });

    cr.ui.decorate($('recently-closed-menu-button'), ntp4.RecentMenuButton);
    chrome.send('getRecentlyClosedTabs');

    mostVisitedPage = new ntp4.MostVisitedPage('Most Visited');
    appendTilePage(mostVisitedPage);
    chrome.send('getMostVisited');
  }

  /**
   * Simple common assertion API
   * @param {*} condition The condition to test.  Note that this may be used to
   *     test whether a value is defined or not, and we don't want to force a
   *     cast to Boolean.
   * @param {string=} opt_message A message to use in any error.
   */
  function assert(condition, opt_message) {
    'use strict';
    if (!condition) {
      var msg = 'Assertion failed';
      if (opt_message)
        msg = msg + ': ' + opt_message;
      throw new Error(msg);
    }
  }

  /**
   * Get an element that's known to exist by its ID. We use this instead of just
   * calling getElementById and not checking the result because this lets us
   * satisfy the JSCompiler type system.
   * @param {string} id The identifier name.
   * @return {!Element} the Element.
   */
  function getRequiredElement(id) {
    var element = document.getElementById(id);
    assert(element, 'Missing required element: ' + id);
    return element;
  }

  /**
   * Callback invoked by chrome with the apps available.
   *
   * Note that calls to this function can occur at any time, not just in
   * response to a getApps request. For example, when a user installs/uninstalls
   * an app on another synchronized devices.
   * @param {Object} data An object with all the data on available
   *        applications.
   */
  function getAppsCallback(data) {
    // Clear any existing apps pages and dots.
    // TODO(rbyers): It might be nice to preserve animation of dots after an
    // uninstall. Could we re-use the existing page and dot elements?  It seems
    // unfortunate to have Chrome send us the entire apps list after an
    // uninstall.
    while (appsPages.length > 0) {
      var page = appsPages[0];
      var dot = page.navigationDot;

      page.tearDown();
      page.parentNode.removeChild(page);
      dot.parentNode.removeChild(dot);
    }

    // Get the array of apps and add any special synthesized entries
    var apps = data.apps;

    // Sort by launch index
    apps.sort(function(a, b) {
      return a.app_launch_index - b.app_launch_index;
    });

    // Add the apps, creating pages as necessary
    for (var i = 0; i < apps.length; i++) {
      var app = apps[i];
      var pageIndex = (app.page_index || 0);
      while (pageIndex >= appsPages.length) {
        var origPageCount = appsPages.length;
        appendTilePage(new ntp4.AppsPage('Apps'));
        // Confirm that appsPages is a live object, updated when a new page is
        // added (otherwise we'd have an infinite loop)
        assert(appsPages.length == origPageCount + 1, 'expected new page');
      }

      appsPages[pageIndex].appendApp(app);
    }

    // Tell the slider about the pages
    updateSliderCards();

    // Mark the current page
    dots[cardSlider.currentCard].classList.add('selected');
  }

  /**
   * Make a synthesized app object representing the chrome web store.  It seems
   * like this could just as easily come from the back-end, and then would
   * support being rearranged, etc.
   * @return {Object} The app object as would be sent from the webui back-end.
   */
  function makeWebstoreApp() {
    return {
      id: '',   // Empty ID signifies this is a special synthesized app
      page_index: 0,
      app_launch_index: -1,   // always first
      name: templateData.web_store_title,
      launch_url: templateData.web_store_url,
      icon_big: getThemeUrl('IDR_WEBSTORE_ICON')
    };
  }

  /**
   * Given a theme resource name, construct a URL for it.
   * @param {string} resourceName The name of the resource.
   * @return {string} A url which can be used to load the resource.
   */
  function getThemeUrl(resourceName) {
    return 'chrome://theme/' + resourceName;
  }

  /**
   * Callback invoked by chrome whenever an app preference changes.
   * The normal NTP uses this to keep track of the current launch-type of an
   * app, updating the choices in the context menu.  We don't have such a menu
   * so don't use this at all (but it still needs to be here for chrome to
   * call).
   * @param {Object} data An object with all the data on available
   *        applications.
   */
  function appsPrefChangeCallback(data) {
  }

  function getCardSlider() {
    return cardSlider;
  }

  /**
   * Invoked whenever the pages in apps-page-list have changed so that
   * the Slider knows about the new elements.
   */
  function updateSliderCards() {
    var pageNo = cardSlider.currentCard;
    if (pageNo >= tilePages.length)
      pageNo = tilePages.length - 1;
    var pageArray = [];
    for (var i = 0; i < tilePages.length; i++)
      pageArray[i] = tilePages[i];
    cardSlider.setCards(pageArray, pageNo);
  }

  /**
   * Appends a tile page (for apps or most visited).
   *
   * @param {TilePage} page The page element.
   * @param {boolean=} opt_animate If true, add the class 'new' to the created
   *        dot.
   */
  function appendTilePage(page) {
    pageList.appendChild(page);

    // Make a deep copy of the dot template to add a new one.
    var newDot = new ntp4.NavDot(page);
    if (page.classList.contains('temporary'))
      newDot.classList.add('temporary');

    dotList.appendChild(newDot);
    page.navigationDot = newDot;
  }

  /**
   * Search an elements ancestor chain for the nearest element that is a member
   * of the specified class.
   * @param {!Element} element The element to start searching from.
   * @param {string} className The name of the class to locate.
   * @return {Element} The first ancestor of the specified class or null.
   */
  function getParentByClassName(element, className) {
    for (var e = element; e; e = e.parentElement) {
      if (e.classList.contains(className))
        return e;
    }
    return null;
  }

  /**
   * Invoked whenever some app is grabbed
   * @param {Grabber.Event} e The Grabber Grab event.
   */
  function enterRearrangeMode(e) {
    var tempPage = new ntp4.AppsPage('');
    tempPage.classList.add('temporary');
    appendTilePage(tempPage);
    updateSliderCards();
  }

  /**
   * Invoked whenever some app is released
   * @param {Grabber.Event} e The Grabber RELEASE event.
   */
  function leaveRearrangeMode(e) {
    var tempPage = document.querySelector('.tile-page.temporary');
    var dot = tempPage.navigationDot;
    if (!tempPage.tileCount) {
      dot.parentNode.removeChild(dot);
      tempPage.parentNode.removeChild(tempPage);
    } else {
      dot.classList.remove('temporary');
      tempPage.classList.remove('temporary');
    }
  }

  /**
   * Returns the index of the given page.
   * @param {AppsPage} page The AppsPage for we wish to find.
   * @return {number} The index of |page|, or -1 if it is not here.
   */
  function getAppsPageIndex(page) {
    return Array.prototype.indexOf.call(appsPages, page);
  }

  // TODO(estade): rename newtab.css to new_tab_theme.css
  function themeChanged(hasAttribution) {
    $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
    if (typeof hasAttribution != 'undefined')
      document.documentElement.setAttribute('hasattribution', hasAttribution);
    updateLogo();
    updateAttribution();
  }

  /**
   * Sets the proper image for the logo at the bottom left.
   */
  function updateLogo() {
    var imageId = 'IDR_PRODUCT_LOGO';
    if (document.documentElement.getAttribute('customlogo') == 'true')
      imageId = 'IDR_CUSTOM_PRODUCT_LOGO';

    $('logo-img').src = 'chrome://theme/' + imageId + '?' + Date.now();
  }

  /**
   * Attributes the attribution image at the bottom left.
   */
  function updateAttribution() {
    var attribution = $('attribution');
    if (document.documentElement.getAttribute('hasattribution') == 'true') {
      $('attribution-img').src =
          'chrome://theme/IDR_THEME_NTP_ATTRIBUTION?' + Date.now();
      attribution.hidden = false;
    } else {
      attribution.hidden = true;
    }

  }

  function setRecentlyClosedTabs(dataItems) {
    $('recently-closed-menu-button').dataItems = dataItems;
  }

  function setMostVisitedPages(data, firstRun, hasBlacklistedUrls) {
    mostVisitedPage.data = data;
  }

  // Return an object with all the exports
  return {
    assert: assert,
    appsPrefChangeCallback: appsPrefChangeCallback,
    enterRearrangeMode: enterRearrangeMode,
    getAppsCallback: getAppsCallback,
    getCardSlider: getCardSlider,
    getAppsPageIndex: getAppsPageIndex,
    initialize: initialize,
    leaveRearrangeMode: leaveRearrangeMode,
    themeChanged: themeChanged,
    setRecentlyClosedTabs: setRecentlyClosedTabs,
    setMostVisitedPages: setMostVisitedPages,
  };
});

// publish ntp globals
// TODO(estade): update the content handlers to use ntp namespace instead of
// making these global.
var assert = ntp4.assert;
var getAppsCallback = ntp4.getAppsCallback;
var appsPrefChangeCallback = ntp4.appsPrefChangeCallback;
var themeChanged = ntp4.themeChanged;
var recentlyClosedTabs = ntp4.setRecentlyClosedTabs;
var setMostVisitedPages = ntp4.setMostVisitedPages;

document.addEventListener('DOMContentLoaded', ntp4.initialize);
