// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crx_downloader.h"
#include "chrome/browser/component_updater/url_fetcher_downloader.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/browser/component_updater/background_downloader_win.h"
#endif

using content::BrowserThread;

namespace component_updater {

// On Windows, the first downloader in the chain is a background downloader,
// which uses the BITS service.
CrxDownloader* CrxDownloader::Create(
    bool is_background_download,
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const DownloadCallback& download_callback) {
  scoped_ptr<CrxDownloader> url_fetcher_downloader(
      new UrlFetcherDownloader(scoped_ptr<CrxDownloader>().Pass(),
                               context_getter,
                               task_runner,
                               download_callback));
#if defined (OS_WIN)
  if (is_background_download) {
    return new BackgroundDownloader(url_fetcher_downloader.Pass(),
                                    context_getter,
                                    task_runner,
                                    download_callback);
  }
#endif

  return url_fetcher_downloader.release();
}

CrxDownloader::CrxDownloader(
    scoped_ptr<CrxDownloader> successor,
    const DownloadCallback& download_callback)
    : successor_(successor.Pass()),
      download_callback_(download_callback) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CrxDownloader::~CrxDownloader() {
}

bool CrxDownloader::StartDownloadFromUrl(const GURL& url) {
  std::vector<GURL> urls;
  urls.push_back(url);
  return StartDownload(urls);
}

bool CrxDownloader::StartDownload(const std::vector<GURL>& urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (urls.empty())
    return false;

  // If the urls are mutated while this downloader is active, then the
  // behavior is undefined in the sense that the outcome of the download could
  // be inconsistent for the list of urls. At any rate, the |current_url_| is
  // reset at this point, and the iterator will be valid in all conditions.
  urls_ = urls;
  current_url_ = urls_.begin();

  DoStartDownload(*current_url_);
  return true;
}

void CrxDownloader::OnDownloadComplete(bool is_handled, const Result& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (result.error) {
    // If an error has occured, in general try the next url if there is any,
    // then move on to the successor in the chain if there is any successor.
    // If this downloader has received a 5xx error for the current url,
    // as indicated by the |is_handled| flag, remove that url from the list of
    // urls so the url is never retried. In both cases, move on to the
    // next url.
    if (!is_handled) {
      ++current_url_;
    } else {
      current_url_ = urls_.erase(current_url_);
    }

    // Try downloading from another url from the list.
    if (current_url_ != urls_.end()) {
      DoStartDownload(*current_url_);
      return;
    }

    // If there is another downloader that can accept this request, then hand
    // the request over to it so that the successor can try the pruned list
    // of urls. Otherwise, the request ends here since the current downloader
    // has tried all urls and it can't fall back on any other downloader.
    if (successor_ && successor_->StartDownload(urls_))
      return;
  }

  download_callback_.Run(result);
}

}  // namespace component_updater

