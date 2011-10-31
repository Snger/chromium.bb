# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_chrome',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/browser_resources.pak',
      '<(grit_out_dir)/common_resources.pak',
      '<(grit_out_dir)/default_plugin_resources/default_plugin_resources.pak',
      '<(grit_out_dir)/renderer_resources.pak',
      '<(grit_out_dir)/theme_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
    ],
    'conditions': [
      ['OS != "mac"', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
        ]
      }],
      ['touchui==0 or OS == "mac"', {
        'pak_inputs': [
          '<(grit_out_dir)/theme_resources_standard.pak',
          '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
        ],
      }, {  # else: touchui!=0
        'pak_inputs': [
          '<(grit_out_dir)/theme_resources_large.pak',
          '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_large/ui_resources_large.pak',
        ],
      }],
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(INTERMEDIATE_DIR)/repack/chrome.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
