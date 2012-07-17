// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_

namespace performance_monitor {

// TODO(chebert): i18n
extern const char kMetricNotFoundError[];

// Any metric that is not associated with a specific activity will use this as
// its activity.
extern const char kProcessChromeAggregate[];

// When you add a metric type, make sure to edit Database::InitMetricDetails
// and add an entry to the MericType enum.
// TODO(mwrosen): When you make real metrics, delete the sample metric.
extern const char kSampleMetricDescription[];
extern const char kSampleMetricName[];
extern const char kStateChromeVersion[];
extern const char kStateProfilePrefix[];

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
