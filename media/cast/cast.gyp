# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 0,
  },
  'targets': [
    {
      'target_name': 'cast_config',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cast_config.h',
        'cast_config.cc',
      ], # source
    },
    {
      'target_name': 'cast_sender',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_sender.gyp:cast_sender_impl',
      ],
    },
    {
      'target_name': 'cast_receiver',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_receiver.gyp:cast_receiver_impl',
      ],
    },
  ],  # targets,
}
