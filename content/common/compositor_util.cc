// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/compositor_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsThreadedCompositingEnabled() {
#if defined(OS_WIN) && defined(USE_AURA)
  // We always want compositing on Aura Windows.
  return true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over field trials.
  if (command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
      command_line.HasSwitch(switches::kDisableForceCompositingMode) ||
      command_line.HasSwitch(switches::kDisableThreadedCompositing))
    return false;

  if (command_line.HasSwitch(switches::kEnableThreadedCompositing))
    return true;

  base::FieldTrial* trial =
      base::FieldTrialList::Find(content::kGpuCompositingFieldTrialName);
  return trial &&
         trial->group_name() ==
             content::kGpuCompositingFieldTrialThreadEnabledName;
}

bool IsForceCompositingModeEnabled() {
#if defined(OS_WIN) && defined(USE_AURA)
  // We always want compositing on Aura Windows.
  return true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over field trials.
  if (command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
      command_line.HasSwitch(switches::kDisableForceCompositingMode))
    return false;

  if (command_line.HasSwitch(switches::kForceCompositingMode))
    return true;

  base::FieldTrial* trial =
      base::FieldTrialList::Find(content::kGpuCompositingFieldTrialName);

  // Force compositing is enabled in both the force compositing
  // and threaded compositing mode field trials.
  return trial &&
        (trial->group_name() ==
            content::kGpuCompositingFieldTrialForceCompositingEnabledName ||
         trial->group_name() ==
            content::kGpuCompositingFieldTrialThreadEnabledName);
}

}  // compositor_util
