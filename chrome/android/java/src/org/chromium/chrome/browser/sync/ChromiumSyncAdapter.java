// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Application;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.AndroidBrowserProcess;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.common.ProcessInitException;

import java.util.concurrent.Semaphore;

/**
 * A sync adapter for Chromium.
 */
public abstract class ChromiumSyncAdapter extends AbstractThreadedSyncAdapter {
    private static final String TAG = "ChromiumSyncAdapter";

    // TODO(nyquist) Make these fields package protected once downstream sync adapter tests are
    // removed.
    @VisibleForTesting
    public static final String INVALIDATION_OBJECT_ID_KEY = "objectId";
    @VisibleForTesting
    public static final String INVALIDATION_VERSION_KEY = "version";
    @VisibleForTesting
    public static final String INVALIDATION_PAYLOAD_KEY = "payload";

    private final Application mApplication;
    private final boolean mAsyncStartup;

    public ChromiumSyncAdapter(Context context, Application application) {
        super(context, false);
        mApplication = application;
        mAsyncStartup = useAsyncStartup();
    }

    protected abstract boolean useAsyncStartup();

    protected abstract void initCommandLine();

    @Override
    public void onPerformSync(Account account, Bundle extras, String authority,
                              ContentProviderClient provider, SyncResult syncResult) {
        if (!DelayedSyncController.getInstance().shouldPerformSync(getContext(), extras, account)) {
            return;
        }

        // Browser startup is asynchronous, so we will need to wait for startup to finish.
        Semaphore semaphore = new Semaphore(0);

        // Configure the callback with all the data it needs.
        BrowserStartupController.StartupCallback callback =
                getStartupCallback(mApplication, account, extras, syncResult, semaphore);
        startBrowserProcess(callback, syncResult, semaphore);

        try {
            // Wait for startup to complete.
            semaphore.acquire();
        } catch (InterruptedException e) {
            Log.w(TAG, "Got InterruptedException when trying to request a sync.", e);
            // Using numIoExceptions so Android will treat this as a soft error.
            syncResult.stats.numIoExceptions++;
        }
    }

    private void startBrowserProcess(
            final BrowserStartupController.StartupCallback callback,
            final SyncResult syncResult, Semaphore semaphore) {
        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    initCommandLine();
                    if (mAsyncStartup) {
                        BrowserStartupController.get(mApplication)
                                .startBrowserProcessesAsync(callback);
                    } else {
                        startBrowserProcessesSync(callback);
                    }
                }
            });
        } catch (RuntimeException e) {
            // It is still unknown why we ever experience this. See http://crbug.com/180044.
            Log.w(TAG, "Got exception when trying to request a sync. Informing Android system.", e);
            // Using numIoExceptions so Android will treat this as a soft error.
            syncResult.stats.numIoExceptions++;
            semaphore.release();
        }
    }

    private void startBrowserProcessesSync(
            final BrowserStartupController.StartupCallback callback) {
        try {
            AndroidBrowserProcess.init(
                    mApplication, AndroidBrowserProcess.MAX_RENDERERS_LIMIT);
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    callback.onSuccess(false);
                }
            });
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to start browser process.", e);
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    callback.onFailure();
                }
            });
        }
    }

    private BrowserStartupController.StartupCallback getStartupCallback(
            final Context context, final Account acct, Bundle extras,
            final SyncResult syncResult, final Semaphore semaphore) {
        final boolean syncAllTypes = extras.getString(INVALIDATION_OBJECT_ID_KEY) == null;
        final String objectId = syncAllTypes ? "" : extras.getString(INVALIDATION_OBJECT_ID_KEY);
        final long version = syncAllTypes ? 0 : extras.getLong(INVALIDATION_VERSION_KEY);
        final String payload = syncAllTypes ? "" : extras.getString(INVALIDATION_PAYLOAD_KEY);

        return new BrowserStartupController.StartupCallback() {
            @Override
            public void onSuccess(boolean alreadyStarted) {
                // Startup succeeded, so we can tickle the sync engine.
                if (syncAllTypes) {
                    Log.v(TAG, "Received sync tickle for all types.");
                    requestSyncForAllTypes();
                } else {
                    Log.v(TAG, "Received sync tickle for " + objectId + ".");
                    requestSync(objectId, version, payload);
                }
                semaphore.release();
            }

            @Override
            public void onFailure() {
                // The startup failed, so we reset the delayed sync state.
                DelayedSyncController.getInstance().setDelayedSync(context, acct.name);
                // Using numIoExceptions so Android will treat this as a soft error.
                syncResult.stats.numIoExceptions++;
                semaphore.release();
            }
        };
    }

    @VisibleForTesting
    public void requestSync(String objectId, long version, String payload) {
        ProfileSyncService.get(mApplication)
                .requestSyncFromNativeChrome(objectId, version, payload);
    }

    @VisibleForTesting
    public void requestSyncForAllTypes() {
        ProfileSyncService.get(mApplication).requestSyncFromNativeChromeForAllTypes();
    }
}
