# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the specific library dependencies for validating on x86-32
{
  'includes': [
    '../../../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncvalidate_x86_32"', {
        'sources': [
          'ncvalidate.c',
          'ncvalidate_verbose.c',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
      }],
    ],
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'targets': [
        # ----------------------------------------------------------------------
        {
          'target_name': 'ncvalidate_x86_32',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate_x86_32',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate'
          ],
          'hard_dependency': 1,
        },
      ],
    }],
    [ 'target_arch=="arm" or target_arch=="x64"', {
      'targets': []
    }],
  ],
}

