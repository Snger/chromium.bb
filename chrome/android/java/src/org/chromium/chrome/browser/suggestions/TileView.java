// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * The view for a site suggestion tile. Displays the title of the site beneath a large icon. If a
 * large icon isn't available, displays a rounded rectangle with a single letter in its place.
 */
public class TileView extends FrameLayout {
    private String mUrl;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the title text.
     */
    public void setTitle(String title) {
        ((TextView) findViewById(R.id.tile_view_title)).setText(title);
    }

    /**
     * Sets the icon, or null to clear it.
     */
    public void setIcon(@Nullable Drawable icon) {
        ((ImageView) findViewById(R.id.tile_view_icon)).setImageDrawable(icon);
    }

    /**
     * Sets whether the page is available offline.
     */
    public void setOfflineAvailable(boolean offlineAvailable) {
        findViewById(R.id.offline_badge)
                .setVisibility(offlineAvailable ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Sets the site URL. This is used to identify the view.
     */
    public void setUrl(String url) {
        mUrl = url;
    }

    /**
     * Gets the site URL. This is used to identify the view.
     */
    public String getUrl() {
        return mUrl;
    }
}
