# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
# win xp vista win7
# mac leopard snowleopard lion mountainlion
# linux chromeos android
# nvidia amd intel
# Specific gpu's can be listed as a tuple with vendor name and device ID.
# Example: ('nvidia', 0x1234)
# Device ID's must be paired with a gpu vendor.

class WebGLConformanceExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('gl-enable-vertex-attrib.html',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # Windows/Intel failures
    self.Fail('conformance/textures/texture-size.html',
        ['win', 'intel'], bug=121139)

    # Windows 7/Intel failures
    self.Fail('conformance/context/context-lost-restored.html',
        ['win7', 'intel'])
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/extensions/oes-texture-float-with-image-data.html',
        ['win7', 'intel'])
    self.Fail('conformance/extensions/oes-texture-float.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-attribs.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-textures.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-uniforms.html',
        ['win7', 'intel'])
    self.Fail('conformance/rendering/gl-clear.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/gl-teximage.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-array-buffer-view.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgb565.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba4444.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba5551.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-with-format-and-type.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texparameter-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-active-bind-2.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-active-bind.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-complete.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-formats-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-mips.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-npot.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-size-cube-maps.html',
        ['win7', 'intel'])
