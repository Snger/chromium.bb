// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.content_public.common.MediaMetadata.MediaImage;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/**
 * Robolectric tests for MediaImageManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaImageManagerTest {
    private static final int TINY_IMAGE_SIZE_PX = 50;
    private static final int MIN_IMAGE_SIZE_PX = 100;
    private static final int IDEAL_IMAGE_SIZE_PX = 200;
    private static final int REQUEST_ID_1 = 1;
    private static final int REQUEST_ID_2 = 2;
    private static final String IMAGE_URL = "http://example.com/foo.png";

    @Mock
    private WebContents mWebContents;
    @Mock
    private MediaImageCallback mCallback;

    private MediaImageManager mMediaImageManager;

    // Prepared data for feeding.
    private ArrayList<MediaImage> mImages;
    private ArrayList<Bitmap> mBitmaps;
    private ArrayList<Rect> mOriginalImageSizes;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        doReturn(REQUEST_ID_1)
                .when(mWebContents)
                .downloadImage(anyString(), anyBoolean(), anyInt(), anyBoolean(),
                        any(MediaImageManager.class));
        mMediaImageManager = new MediaImageManager(MIN_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX);
        mMediaImageManager.setWebContents(mWebContents);

        mImages = new ArrayList<MediaImage>();
        mImages.add(new MediaImage(IMAGE_URL, "", new ArrayList<Rect>()));

        mBitmaps = new ArrayList<Bitmap>();
        mBitmaps.add(Bitmap.createBitmap(
                IDEAL_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888));

        mOriginalImageSizes = new ArrayList<Rect>();
        mOriginalImageSizes.add(new Rect(0, 0, IDEAL_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX));
    }

    @Test
    public void testDownloadImage() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mWebContents)
                .downloadImage(eq(IMAGE_URL), eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD), eq(false),
                        eq(mMediaImageManager));
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mCallback).onImageDownloaded(isNotNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNull(Bitmap.class));
    }

    @Test
    public void testDownloadImageTwice() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        // Second download.
        doReturn(REQUEST_ID_2)
                .when(mWebContents)
                .downloadImage(anyString(), anyBoolean(), anyInt(), anyBoolean(),
                        any(MediaImageManager.class));
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mWebContents, times(2))
                .downloadImage(eq(IMAGE_URL), eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD), eq(false),
                        eq(mMediaImageManager));
        verify(mCallback, times(2)).onImageDownloaded(isNotNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNull(Bitmap.class));
    }

    @Test
    public void testDuplicateResponce() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mCallback, times(1)).onImageDownloaded(isNotNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNull(Bitmap.class));
    }

    @Test
    public void testWrongResponceId() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNull(Bitmap.class));
    }

    @Test
    public void testTinyImagesRemovedBeforeDownloading() {
        mImages.clear();
        ArrayList<Rect> sizes = new ArrayList<Rect>();
        sizes.add(new Rect(0, 0, TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX));
        mImages.add(new MediaImage(IMAGE_URL, "", sizes));
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(anyString(), anyBoolean(), anyInt(), anyBoolean(),
                        any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded(isNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
    }

    @Test
    public void testTinyImagesRemovedAfterDownloading() {
        mMediaImageManager.downloadImage(mImages, mCallback);

        // Reset the data for feeding.
        mBitmaps.clear();
        mBitmaps.add(Bitmap.createBitmap(
                TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888));
        mOriginalImageSizes.clear();
        mOriginalImageSizes.add(new Rect(0, 0, TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX));

        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mCallback).onImageDownloaded(isNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
    }

    @Test
    public void testDownloadImageFails() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 404, IMAGE_URL, mBitmaps, mOriginalImageSizes);

        verify(mCallback).onImageDownloaded(isNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
    }

    @Test
    public void testEmptyImageList() {
        mImages.clear();
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(anyString(), anyBoolean(), anyInt(), anyBoolean(),
                        any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded(isNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
    }

    @Test
    public void testNullImageList() {
        mMediaImageManager.downloadImage(null, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(anyString(), anyBoolean(), anyInt(), anyBoolean(),
                        any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded(isNull(Bitmap.class));
        verify(mCallback, times(0)).onImageDownloaded(isNotNull(Bitmap.class));
    }
}