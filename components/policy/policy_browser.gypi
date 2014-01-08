# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../base/base.gyp:base_prefs',
    '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
    '../net/net.gyp:net',
    '../ui/ui.gyp:ui',
    '../url/url.gyp:url_lib',
    'component_strings.gyp:component_strings',
    'user_prefs',
  ],
  'defines': [
    'POLICY_COMPONENT_IMPLEMENTATION',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    # Note that these sources are always included, even for builds that
    # disable policy. Most source files should go in the conditional
    # sources list below.
    # url_blacklist_manager.h is used by managed mode.
    'core/browser/url_blacklist_manager.cc',
    'core/browser/url_blacklist_manager.h',
  ],
  'conditions': [
    ['configuration_policy==1', {
      'dependencies': [
        'cloud_policy_proto',
        'url_matcher',
      ],
      'sources': [
        'core/browser/cloud/message_util.cc',
        'core/browser/cloud/message_util.h',
        'core/browser/configuration_policy_handler.cc',
        'core/browser/configuration_policy_handler.h',
        'core/browser/configuration_policy_handler_list.cc',
        'core/browser/configuration_policy_handler_list.h',
        'core/browser/configuration_policy_pref_store.cc',
        'core/browser/configuration_policy_pref_store.h',
        'core/browser/policy_error_map.cc',
        'core/browser/policy_error_map.h',
      ],
    }],
  ],
}
