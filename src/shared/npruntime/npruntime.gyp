# Copyright (c) 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'SRPCGEN': '<(DEPTH)/native_client/tools/srpcgen.py',
    'NPRUNTIME_DIR':
    '<(INTERMEDIATE_DIR)/gen/native_client/src/shared/npruntime',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
      'npmodule_specs': [
        'audio.srpc',
        'device2d.srpc',
        'device3d.srpc',
        'npmodule.srpc',
        'npobjectstub.srpc',
      ],
      'npnavigator_specs': [
        'npnavigator.srpc',
        'npobjectstub.srpc',
      ],
    },
    'target_conditions': [
      ['target_base=="npruntime"', {
        'sources': [
        'nacl_npapi.h',
        'naclnp_util.cc',
        'npbridge.cc',
        'npbridge.h',
        'npcapability.h',
        'npmodule.cc',
        'npmodule.h',
        'npobject_proxy.cc',
        'npobject_proxy.h',
        'npobject_stub.cc',
        'npobject_stub.h',
        # TODO env_no_strict_aliasing.ComponentObject('nprpc.cc')
        'nprpc.cc',
        'nprpc.h',
        'npmodule_rpc_impl.cc',
        'npstub_rpc_impl.cc',
        'pointer_translations.cc',
        'pointer_translations.h',
        ],
      }],
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'XP_MACOSX',
          'XP_UNIX',
          'TARGET_API_MAC_CARBON=1',
          'NO_X11',
          'USE_SYSTEM_CONSOLE',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '2',  # /EHsc
          },
        },
      }],
    ],
    'actions': [
      {
        'action_name': 'npmodule_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(npmodule_specs)',
        ],
        'action':
          # TODO(gregoryd): find out how to generate a file
          # in such a location that can be found in both
          # NaCl and Chrome builds.
          ['<@(python_exe)', '<(SRPCGEN)',
           '-s',
           'NPModuleRpcs',
           'GEN_NPRUNTIME_NPMODULE_RPC_H_',
           '<@(_outputs)',
           '<@(npmodule_specs)'],

        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<(NPRUNTIME_DIR)/npmodule_rpc.h',
          '<(NPRUNTIME_DIR)/npmodule_rpc_server.cc',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Creating npmodule_rpc.h and npmodule_rpc_server.cc',
      },
      {
        'action_name': 'npnavigator_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(npnavigator_specs)',
        ],
        'action':
          # TODO(gregoryd): find out how to generate a file
          # in such a location that can be found in both
          # NaCl and Chrome builds.
          ['<@(python_exe)', '<(SRPCGEN)',
           '-c',
           'NPNavigatorRpcs',
           'GEN_NPRUNTIME_NPNAVIGATOR_RPC_H_',
           '<@(_outputs)',
           '<@(npnavigator_specs)'],

        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<(NPRUNTIME_DIR)/npnavigator_rpc.h',
          '<(NPRUNTIME_DIR)/npnavigator_rpc_client.cc',
        ],
        'process_outputs_as_sources': 1,
        'message': 'Creating npnavigator_rpc.h and npnavigator_rpc_client.cc',
      },
    ],
  },
  'targets': [
    {
      'target_name': 'google_nacl_npruntime',
      'type': 'static_library',
      'variables': {
        'target_base': 'npruntime',
      },
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ]
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'google_nacl_npruntime64',
          'type': 'static_library',
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'variables': {
            'target_base': 'npruntime',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        }
      ],
    }],
  ],
}

#env_no_strict_aliasing = env.Clone()
#if env.Bit('linux'):
#   env_no_strict_aliasing.Append(CCFLAGS = ['-fno-strict-aliasing'])
#
