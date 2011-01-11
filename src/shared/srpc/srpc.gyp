# Copyright (c) 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="srpc"', {
        'sources': [
          'invoke.c',
          'module_init_fini.c',
          'nacl_srpc.c',
          'nacl_srpc.h',
          'nacl_srpc_internal.h',
          'nacl_srpc_message.c',
          'rpc_serialize.c',
          'rpc_service.c',
          'rpc_server_loop.c',
          'utility.c',
        ],
        'cflags': [
          '-fno-strict-aliasing',
          '-Wno-missing-field-initializers'
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'nonnacl_srpc',
      'type': 'static_library',
      'variables': {
        'target_base': 'srpc',
      },
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'nonnacl_srpc64',
          'type': 'static_library',
          'variables': {
            'target_base': 'srpc',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        }
      ],
    }],
  ]
}
