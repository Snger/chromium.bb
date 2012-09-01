# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'webkit_compositor_bindings_tests_sources': [
      'LayerChromiumTest.cpp',
      'TextureCopierTest.cpp',
      'TextureLayerChromiumTest.cpp',
      'ThrottledTextureUploaderTest.cpp',
      'TiledLayerChromiumTest.cpp',
      'TreeSynchronizerTest.cpp',
      'WebAnimationTest.cpp',
      'WebFloatAnimationCurveTest.cpp',
      'WebFloatAnimationCurveTest.cpp',
      'WebLayerTest.cpp',
      'WebLayerTreeViewTest.cpp',
      'WebTransformAnimationCurveTest.cpp',
      'WebTransformAnimationCurveTest.cpp',
      'WebTransformOperationsTest.cpp',
      'WebTransformationMatrixTest.cpp',
      'test/FakeWebScrollbarThemeGeometry.h',
      'test/WebLayerTreeViewTestCommon.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_bindings_unittests',
      'type' : 'executable',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['use_libcc_for_compositor==1 and component!="shared_library"', {
          'defines': [
            'USE_LIBCC_FOR_COMPOSITOR',
          ],
          'dependencies': [
            '../../cc/cc.gyp:cc',
            '../../cc/cc_tests.gyp:cc_test_support',
            '../../skia/skia.gyp:skia',
            '../../third_party/WebKit/Source/Platform/Platform.gyp/Platform.gyp:webkit_platform',
            '../../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '../../webkit/support/webkit_support.gyp:webkit_support',
            'compositor_bindings.gyp:webkit_compositor_bindings',
          ],
          'include_dirs': [
            '.',
            'test',
            '<(DEPTH)/cc',
            '<(DEPTH)/cc/stubs',
            '<(DEPTH)/cc/test',
          ],
          'sources': [
            '<@(webkit_compositor_bindings_tests_sources)',
          ]
        }],
      ],
    },
  ],
}
