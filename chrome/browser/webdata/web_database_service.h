// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/api/webdata/web_data_service_base.h"
#include "chrome/browser/webdata/web_database.h"

namespace content {
class BrowserContext;
}

namespace tracked_objects {
class Location;
}

class WDTypedResult;
class WebDataServiceConsumer;


////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseService defines the interface to a generic data repository
// responsible for controlling access to the web database (metadata associated
// with web pages).
//
////////////////////////////////////////////////////////////////////////////////

class WebDatabaseService {
 public:
  typedef base::Callback<scoped_ptr<WDTypedResult>(WebDatabase*)> ReadTask;
  typedef base::Callback<WebDatabase::State(WebDatabase*)> WriteTask;
  typedef base::Callback<void(sql::InitStatus)> InitCallback;

  // Retrieve a WebDatabaseService for the given context.
  //
  // Can return NULL in some contexts.
  static WebDatabaseService* FromBrowserContext(
      content::BrowserContext* context);

  virtual ~WebDatabaseService() {}

  // Initializes the web database service. Takes a callback which will return
  // the status of the DB after the init.
  virtual void LoadDatabase(const InitCallback& callback) = 0;

  // Unloads the database without actually shutting down the service.  This can
  // be used to temporarily reduce the browser process' memory footprint.
  virtual void UnloadDatabase() = 0;

  // Gets a ptr to the WebDatabase (owned by WebDatabaseService).
  // TODO(caitkp): remove this method once SyncServices no longer depend on it.
  virtual WebDatabase* GetDatabase() const = 0;

  // Schedule an update/write task on the DB thread.
  virtual void ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const WriteTask& task) = 0;

  // Schedule a read task on the DB thread.
  virtual WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ReadTask& task,
      WebDataServiceConsumer* consumer) = 0;

  // Cancel an existing request for a task on the DB thread.
  // TODO(caitkp): Think about moving the definition of the Handle type to
  // somewhere else.
  virtual void CancelRequest(WebDataServiceBase::Handle h) = 0;
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_
