// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.TestWebServer;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content.browser.NavigationEntry;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.Callable;

public class NavigationHistoryTest extends AndroidWebViewTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    private NavigationHistory getNavigationHistory(final AwContents awContents)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<NavigationHistory>() {
            @Override
            public NavigationHistory call() {
                return awContents.getContentViewCore().getNavigationHistory();
            }
        });
    }

    private void checkHistoryItem(NavigationEntry item, String url, String originalUrl,
            String title, boolean faviconNull) {
        assertEquals(url, item.getUrl());
        assertEquals(originalUrl, item.getOriginalUrl());
        assertEquals(title, item.getTitle());
        if (faviconNull) {
            assertNull(item.getFavicon());
        } else {
            assertNotNull(item.getFavicon());
        }
    }

    /*
     * @SmallTest
     * Bug 6776361
     */
    @FlakyTest
    public void testNavigateOneUrl() throws Throwable {
        NavigationHistory history = getNavigationHistory(mAwContents);
        assertEquals(0, history.getEntryCount());

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                "chrome://newtab/");
        history = getNavigationHistory(mAwContents);
        checkHistoryItem(history.getEntryAtIndex(0),
                "chrome://newtab/#bookmarks",
                "chrome://newtab/",
                "New tab",
                true);

        assertEquals(0, history.getCurrentEntryIndex());
    }

    @SmallTest
    public void testNavigateTwoUrls() throws Throwable {
        NavigationHistory list = getNavigationHistory(mAwContents);
        assertEquals(0, list.getEntryCount());

        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        loadUrlSync(mAwContents, onPageFinishedHelper, "chrome://newtab/");
        loadUrlSync(mAwContents, onPageFinishedHelper, "chrome://version");

        list = getNavigationHistory(mAwContents);

        // Make sure there is a new entry entry the list
        assertEquals(2, list.getEntryCount());

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0),
                "chrome://newtab/#bookmarks",
                "chrome://newtab/",
                "New tab",
                true);

        // Make sure the second entry was added properly
        checkHistoryItem(list.getEntryAtIndex(1),
                "chrome://version/",
                "chrome://version/",
                "About Version",
                true);

        assertEquals(1, list.getCurrentEntryIndex());

    }

    @SmallTest
    public void testNavigateTwoUrlsAndBack() throws Throwable {
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        NavigationHistory list = getNavigationHistory(mAwContents);
        assertEquals(0, list.getEntryCount());

        loadUrlSync(mAwContents, onPageFinishedHelper, "chrome://newtab/");
        loadUrlSync(mAwContents, onPageFinishedHelper, "chrome://version");
        HistoryUtils.goBackSync(getInstrumentation(), mAwContents.getContentViewCore(),
                onPageFinishedHelper);
        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0),
                "chrome://newtab/#bookmarks",
                "chrome://newtab/",
                "New tab",
                true);

        // Make sure the second entry is still okay
        checkHistoryItem(list.getEntryAtIndex(1),
                "chrome://version/",
                "chrome://version/",
                "About Version",
                true);

        // Make sure the current index is back to 0
        assertEquals(0, list.getCurrentEntryIndex());
    }

    /**
     * Disabled until favicons are getting fetched when using ContentView.
     *
     * @SmallTest
     * @throws Throwable
     */
    @DisabledTest
    public void testFavicon() throws Throwable {
        NavigationHistory list = getNavigationHistory(mAwContents);
        String url;

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            webServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(false));
            url = webServer.setResponse("/favicon.html", CommonResources.FAVICON_STATIC_HTML, null);

            assertEquals(0, list.getEntryCount());
            getContentSettingsOnUiThread(mAwContents).setImagesEnabled(true);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        } finally {
            if (webServer != null) webServer.shutdown();
        }

        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay.
        checkHistoryItem(list.getEntryAtIndex(0), url, url, "", false);
    }
}
