# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'libwebview',
      'type': 'shared_library',
      'dependencies': [
        '<(DEPTH)/chrome/chrome.gyp:browser',
        '<(DEPTH)/chrome/chrome.gyp:renderer',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/android_webview/native/webview_native.gyp:webview_native',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android',
        '<(DEPTH)/chrome/browser/component/components.gyp:browser_component_jni_headers',
      ],
      'include_dirs': [
        '../..',
        '../../skia/config',
      ],
      'sources': [
        'aw_browser_dependency_factory_impl.cc',
        'aw_browser_dependency_factory_impl.h',
        'aw_content_browser_client.cc',
        'aw_content_browser_client.h',
        'main/webview_entry_point.cc',
        'main/webview_main_delegate.cc',
        'main/webview_main_delegate.h',
        'main/webview_stubs.cc',
      ],
      'includes': [
        '../aw_browser.gypi',
      ],
    },
    {
      'target_name': 'android_webview',
      'type' : 'none',
      'dependencies': [
        'libwebview',
      ],
      'variables': {
        'install_binary_script': '../build/install_binary',
      },
      'actions': [
        {
          'action_name': 'libwebview_strip_and_install_in_android',
          'inputs': [
            '<(SHARED_LIB_DIR)/libwebview.so',
          ],
          'outputs': [
            '<(android_product_out)/obj/lib/libwebview.so',
            '<(android_product_out)/system/lib/libwebview.so',
            '<(android_product_out)/symbols/system/lib/libwebview.so',
          ],
          'action': [
            '<(install_binary_script)',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'android_webview_java',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/content/content.gyp:content_java',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android_java',
      ],
      'variables': {
        'package_name': 'android_webview_java',
        'java_in_dir': '<(DEPTH)/android_webview/java',
      },
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': 'android_webview_javatests',
      'type': 'none',
      'dependencies': [
        'android_webview_java',
        '<(DEPTH)/base/base.gyp:base_java_test_support',
        '<(DEPTH)/content/content.gyp:content_java',
        '<(DEPTH)/content/content.gyp:content_javatests',
        '<(DEPTH)/chrome/browser/component/components.gyp:web_contents_delegate_android_java',
      ],
      'variables': {
        'package_name': 'android_webview_javatests',
        'java_in_dir': '<(DEPTH)/android_webview/javatests',
      },
      'includes': [ '../../build/java.gypi' ],
    },

    {
      'target_name': 'android_webview_apk',
      'type': 'none',
      'actions': [
      {
        'action_name': 'copy_base_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_base.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_net_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_net.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_net.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_media_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_media.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_media.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_content_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_content.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_content.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_web_contents_delegate_android_java',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_web_contents_delegate_android.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_web_contents_delegate_android.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_android_webview_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_android_webview_java.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_android_webview_java.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_android_webview_test_jar',
        'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_android_webview_javatests.jar'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/java/libs/chromium_android_webview_javatests.jar'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_chrome_pak',
        'inputs': ['<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/assets/chrome.pak'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_chrome_100_percent_pak',
        'inputs': ['<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/assets/chrome_100_percent.pak'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_resources_pak',
        'inputs': ['<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/assets/resources.pak'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_en_pak',
        'inputs': ['<(SHARED_INTERMEDIATE_DIR)/repack/en-US.pak'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/assets/en.pak'],
        'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
      },
      {
        'action_name': 'copy_and_strip_so',
        'inputs': ['<(SHARED_LIB_DIR)/libwebview.so'],
        'outputs': ['<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so'],
        'action': [
            '<!(/bin/echo -n $STRIP)',
            '--strip-unneeded',  # All symbols not needed for relocation.
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
        ],
      },
      {
        'action_name': 'android_webview_apk',
        'inputs': [
          '<(DEPTH)/android_webview/java/android_webview_apk.xml',
          '<(DEPTH)/android_webview/java/AndroidManifest.xml',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_android_webview_java.jar',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_android_webview_javatests.jar',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_base.jar',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_net.jar',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_media.jar',
          '<(PRODUCT_DIR)/android_webview/java/libs/chromium_content.jar',
          '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/repack/en-US.pak',
          '<(PRODUCT_DIR)/android_webview/libs/<(android_app_abi)/libwebview.so',
        ],
        'outputs': [
          '<(PRODUCT_DIR)/android_webview/AndroidWebView-debug.apk',
        ],
        'action': [
          'ant',
          '-DPRODUCT_DIR=<(ant_build_out)',
          '-DAPP_ABI=<(android_app_abi)',
          '-DANDROID_SDK=<(android_sdk)',
          '-DANDROID_SDK_ROOT=<(android_sdk_root)',
          '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
          '-DANDROID_SDK_VERSION=<(android_sdk_version)',
          '-DANDROID_TOOLCHAIN=<(android_toolchain)',
          '-buildfile',
          '<(DEPTH)/android_webview/java/android_webview_apk.xml',
        ],
        'dependencies': [
          'libwebview',
          'android_webview_java',
          'android_webview_javatests',
          '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
          '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
        ],
      }
      ],
    },
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_apk',
        '<(DEPTH)/content/content.gyp:content_javatests',
      ],
      'actions': [
        {
          'action_name': 'copy_base_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_base.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_base_javatests_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base_javatests.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_base_javatests.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_net_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_net.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_net.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_media_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_media.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_media.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_content_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_content.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_content.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_web_contents_delegate_android_java',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_web_contents_delegate_android.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_web_contents_delegate_android.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_content_javatests_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_content_javatests.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_content_javatests.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'copy_android_webview_jar',
          'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_android_webview_java.jar'],
          'outputs': ['<(PRODUCT_DIR)/android_webview_test/java/libs/chromium_android_webview_java.jar'],
          'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'android_webview_test_generate_apk',
          'inputs': [
            '<(DEPTH)/android_webview/javatests/android_webview_test_apk.xml',
            '<(DEPTH)/android_webview/javatests/AndroidManifest.xml',
            '<!@(find <(DEPTH)/android_webview/javatests/ -name "*.java")'
          ],
          'outputs': [
            '<(PRODUCT_DIR)/android_webview_test/ContentShellTest-debug.apk',
          ],
          'action': [
            'ant',
            '-DPRODUCT_DIR=<(ant_build_out)',
            '-DAPP_ABI=<(android_app_abi)',
            '-DANDROID_SDK=<(android_sdk)',
            '-DANDROID_SDK_ROOT=<(android_sdk_root)',
            '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
            '-DANDROID_SDK_VERSION=<(android_sdk_version)',
            '-DANDROID_TOOLCHAIN=<(android_toolchain)',
            '-buildfile',
            '<(DEPTH)/android_webview/javatests/android_webview_test_apk.xml',
          ]
        }
      ],
    },
  ],
}
