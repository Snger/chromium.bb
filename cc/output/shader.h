// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SHADER_H_
#define CC_OUTPUT_SHADER_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace gfx {
class Point;
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {

enum TexCoordPrecision {
  TEX_COORD_PRECISION_NA = 0,
  TEX_COORD_PRECISION_MEDIUM = 1,
  TEX_COORD_PRECISION_HIGH = 2,
  LAST_TEX_COORD_PRECISION = 2
};

// Texture coordinate sources for the vertex shader.
enum TexCoordSource {
  // Vertex shader does not populate a texture coordinate.
  TEX_COORD_SOURCE_NONE,
  // Texture coordinate is set to the untransformed position.
  TEX_COORD_SOURCE_POSITION,
  // Texture coordinate has its own attribute.
  TEX_COORD_SOURCE_ATTRIBUTE,
};

// Texture coordinate transformation modes for the vertex shader.
enum TexCoordTransform {
  // Texture coordinates are not transformed.
  TEX_COORD_TRANSFORM_NONE,
  // Texture coordinates are transformed by a uniform vec4, scaling by zw and
  // then translating by xy.
  TEX_COORD_TRANSFORM_VEC4,
  // Same as the above, but add vec2(0.5) to the texture coordinate first.
  TEX_COORD_TRANSFORM_TRANSLATED_VEC4,
  // Texture coordiantes are transformed by a uniform mat4.
  TEX_COORD_TRANSFORM_MATRIX,
};

// Position source for the vertex shader.
enum PositionSource {
  // The position is read directly from the position attribute.
  POSITION_SOURCE_ATTRIBUTE,
  // The position is read by attribute index into a uniform array for xy, and
  // getting zw from the attribute.
  POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM,
};

enum AAMode {
  NO_AA = 0,
  USE_AA = 1,
};

enum SwizzleMode {
  NO_SWIZZLE = 0,
  DO_SWIZZLE = 1,
};

enum PremultipliedAlphaMode {
  PREMULTIPLIED_ALPHA = 0,
  NON_PREMULTIPLIED_ALPHA = 1,
};

enum SamplerType {
  SAMPLER_TYPE_NA = 0,
  SAMPLER_TYPE_2D = 1,
  SAMPLER_TYPE_2D_RECT = 2,
  SAMPLER_TYPE_EXTERNAL_OES = 3,
  LAST_SAMPLER_TYPE = 3
};

enum BlendMode {
  BLEND_MODE_NONE,
  BLEND_MODE_NORMAL,
  BLEND_MODE_SCREEN,
  BLEND_MODE_OVERLAY,
  BLEND_MODE_DARKEN,
  BLEND_MODE_LIGHTEN,
  BLEND_MODE_COLOR_DODGE,
  BLEND_MODE_COLOR_BURN,
  BLEND_MODE_HARD_LIGHT,
  BLEND_MODE_SOFT_LIGHT,
  BLEND_MODE_DIFFERENCE,
  BLEND_MODE_EXCLUSION,
  BLEND_MODE_MULTIPLY,
  BLEND_MODE_HUE,
  BLEND_MODE_SATURATION,
  BLEND_MODE_COLOR,
  BLEND_MODE_LUMINOSITY,
  LAST_BLEND_MODE = BLEND_MODE_LUMINOSITY
};

enum InputColorSource {
  INPUT_COLOR_SOURCE_RGBA_TEXTURE,
  INPUT_COLOR_SOURCE_UNIFORM,
};

// TODO(ccameron): Merge this with BlendMode.
enum FragColorMode {
  FRAG_COLOR_MODE_DEFAULT,
  FRAG_COLOR_MODE_OPAQUE,
  FRAG_COLOR_MODE_APPLY_BLEND_MODE,
};

enum MaskMode {
  NO_MASK = 0,
  HAS_MASK = 1,
  LAST_MASK_VALUE = HAS_MASK
};

struct ShaderLocations {
  ShaderLocations();

  int sampler = -1;
  int quad = -1;
  int edge = -1;
  int viewport = -1;
  int mask_sampler = -1;
  int mask_tex_coord_scale = -1;
  int mask_tex_coord_offset = -1;
  int matrix = -1;
  int alpha = -1;
  int color_matrix = -1;
  int color_offset = -1;
  int vertex_tex_transform = -1;
  int backdrop = -1;
  int backdrop_rect = -1;
  int original_backdrop = -1;
};

// Note: The highp_threshold_cache must be provided by the caller to make
// the caching multi-thread/context safe in an easy low-overhead manner.
// The caller must make sure to clear highp_threshold_cache to 0, so it can be
// reinitialized, if a new or different context is used.
CC_EXPORT TexCoordPrecision
    TexCoordPrecisionRequired(gpu::gles2::GLES2Interface* context,
                              int* highp_threshold_cache,
                              int highp_threshold_min,
                              const gfx::Point& max_coordinate);

CC_EXPORT TexCoordPrecision TexCoordPrecisionRequired(
    gpu::gles2::GLES2Interface* context,
    int *highp_threshold_cache,
    int highp_threshold_min,
    const gfx::Size& max_size);

class VertexShaderBase {
 public:
  VertexShaderBase();
  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;
  void FillLocations(ShaderLocations* locations) const;

  int vertex_tex_transform_location() const {
    return vertex_tex_transform_location_;
  }

  int tex_matrix_location() const { return tex_matrix_location_; }

  int ya_tex_scale_location() const { return ya_tex_scale_location_; }
  int ya_tex_offset_location() const { return ya_tex_offset_location_; }
  int uv_tex_scale_location() const { return uv_tex_scale_location_; }
  int uv_tex_offset_location() const { return uv_tex_offset_location_; }

  int matrix_location() const { return matrix_location_; }

  int vertex_opacity_location() const { return vertex_opacity_location_; }

  int viewport_location() const { return viewport_location_; }
  int edge_location() const { return edge_location_; }

  int quad_location() const { return quad_location_; }

 protected:
  template <class VertexShader, class FragmentShader>
  friend class ProgramBinding;

  // Use arrays of uniforms for matrix, texTransform, and opacity.
  bool use_uniform_arrays_ = false;

  PositionSource position_source_ = POSITION_SOURCE_ATTRIBUTE;
  TexCoordSource tex_coord_source_ = TEX_COORD_SOURCE_NONE;
  TexCoordTransform tex_coord_transform_ = TEX_COORD_TRANSFORM_NONE;

  // Used only with TEX_COORD_TRANSFORM_VEC4.
  int vertex_tex_transform_location_ = -1;

  // Used only with TEX_COORD_TRANSFORM_MATRIX.
  int tex_matrix_location_ = -1;

  // Uniforms for YUV textures.
  bool is_ya_uv_ = false;
  int ya_tex_scale_location_ = -1;
  int ya_tex_offset_location_ = -1;
  int uv_tex_scale_location_ = -1;
  int uv_tex_offset_location_ = -1;

  // Matrix to transform the position.
  bool has_matrix_ = false;
  int matrix_location_ = -1;

  // Used only with POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM.
  int quad_location_ = -1;

  // Extra dummy variables to work around bugs on Android.
  // TODO(ccameron): This is likley unneeded cargo-culting.
  // http://crbug.com/240602
  bool has_dummy_variables_ = false;

  bool has_vertex_opacity_ = false;
  int vertex_opacity_location_ = -1;

  AAMode aa_mode_ = NO_AA;
  int viewport_location_ = -1;
  int edge_location_ = -1;
};

class VertexShaderPosTex : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_ATTRIBUTE);
    DCHECK_EQ(has_matrix_, true);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_NONE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class VertexShaderPosTexYUVStretchOffset : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_ATTRIBUTE);
    DCHECK_EQ(has_matrix_, true);
    DCHECK_EQ(is_ya_uv_, true);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_NONE);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class VertexShaderPos : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_matrix_, true);

    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_NONE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_NONE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(use_uniform_arrays_, false);
  }
};

class VertexShaderPosTexTransform : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_VEC4);
    DCHECK_EQ(has_matrix_, true);
    DCHECK_EQ(has_vertex_opacity_, true);
    DCHECK_EQ(use_uniform_arrays_, true);

    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class VertexShaderQuad : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM);
    DCHECK_EQ(has_matrix_, true);
#if defined(OS_ANDROID)
    DCHECK_EQ(has_dummy_variables_, true);
#endif

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_NONE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_NONE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class VertexShaderQuadAA : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM);
    DCHECK_EQ(has_matrix_, true);
    DCHECK_EQ(aa_mode_, USE_AA);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_NONE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_NONE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
  }
};

class VertexShaderQuadTexTransformAA : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_POSITION);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_TRANSLATED_VEC4);
    DCHECK_EQ(has_matrix_, true);
    DCHECK_EQ(aa_mode_, USE_AA);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
  }
};

class VertexShaderTile : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_VEC4);
    DCHECK_EQ(has_matrix_, true);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class VertexShaderTileAA : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM);
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_POSITION);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_VEC4);
    DCHECK_EQ(has_matrix_, true);
    DCHECK_EQ(aa_mode_, USE_AA);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
  }
};

class VertexShaderVideoTransform : public VertexShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(tex_coord_source_, TEX_COORD_SOURCE_ATTRIBUTE);
    DCHECK_EQ(tex_coord_transform_, TEX_COORD_TRANSFORM_MATRIX);
    DCHECK_EQ(has_matrix_, true);

    DCHECK_EQ(use_uniform_arrays_, false);
    DCHECK_EQ(position_source_, POSITION_SOURCE_ATTRIBUTE);
    DCHECK_EQ(is_ya_uv_, false);
    DCHECK_EQ(has_dummy_variables_, false);
    DCHECK_EQ(has_vertex_opacity_, false);
    DCHECK_EQ(aa_mode_, NO_AA);
  }
};

class FragmentShaderBase {
 public:
  virtual void Init(gpu::gles2::GLES2Interface* context,
                    unsigned program,
                    int* base_uniform_index);
  std::string GetShaderString() const;
  void FillLocations(ShaderLocations* locations) const;

  int sampler_location() const { return sampler_location_; }
  int alpha_location() const { return alpha_location_; }
  int color_location() const { return color_location_; }
  int background_color_location() const { return background_color_location_; }
  int fragment_tex_transform_location() const {
    return fragment_tex_transform_location_;
  }

 protected:
  FragmentShaderBase();
  virtual std::string GetShaderSource() const;
  bool has_blend_mode() const { return blend_mode_ != BLEND_MODE_NONE; }

  std::string SetBlendModeFunctions(const std::string& shader_string) const;

  // Settings that are modified by sub-classes.
  AAMode aa_mode_ = NO_AA;
  bool has_varying_alpha_ = false;
  SwizzleMode swizzle_mode_ = NO_SWIZZLE;
  PremultipliedAlphaMode premultiply_alpha_mode_ = PREMULTIPLIED_ALPHA;
  FragColorMode frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  InputColorSource input_color_type_ = INPUT_COLOR_SOURCE_RGBA_TEXTURE;

  // Used only if |blend_mode_| is not BLEND_MODE_NONE.
  int backdrop_location_ = -1;
  int original_backdrop_location_ = -1;
  int backdrop_rect_location_ = -1;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_RGBA_TEXTURE.
  bool has_rgba_fragment_tex_transform_ = false;
  int sampler_location_ = -1;
  int fragment_tex_transform_location_ = -1;

  // Always use sampler2D and texture2D for the RGBA texture, regardless of the
  // specified SamplerType.
  // TODO(ccameron): Change GLRenderer to always specify the correct
  // SamplerType.
  bool ignore_sampler_type_ = false;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_UNIFORM.
  int color_location_ = -1;

  MaskMode mask_mode_ = NO_MASK;
  int mask_sampler_location_ = -1;
  int mask_tex_coord_scale_location_ = -1;
  int mask_tex_coord_offset_location_ = -1;

  bool has_color_matrix_ = false;
  int color_matrix_location_ = -1;
  int color_offset_location_ = -1;

  bool has_uniform_alpha_ = false;
  int alpha_location_ = -1;

  bool has_background_color_ = false;
  int background_color_location_ = -1;

  TexCoordPrecision tex_coord_precision_ = TEX_COORD_PRECISION_NA;
  SamplerType sampler_type_ = SAMPLER_TYPE_NA;

  BlendMode blend_mode_ = BLEND_MODE_NONE;
  bool mask_for_background_ = false;

 private:
  template <class VertexShader, class FragmentShader>
  friend class ProgramBinding;

  std::string GetHelperFunctions() const;
  std::string GetBlendFunction() const;
  std::string GetBlendFunctionBodyForRGB() const;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderBase);
};

class FragmentShaderRGBATexVaryingAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_varying_alpha_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexPremultiplyAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_varying_alpha_, true);
    DCHECK_EQ(premultiply_alpha_mode_, NON_PREMULTIPLIED_ALPHA);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderTexBackgroundVaryingAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_background_color_, true);
    DCHECK_EQ(has_varying_alpha_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
  }
};

class FragmentShaderTexBackgroundPremultiplyAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_background_color_, true);
    DCHECK_EQ(has_varying_alpha_, true);
    DCHECK_EQ(premultiply_alpha_mode_, NON_PREMULTIPLIED_ALPHA);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
  }
};

class FragmentShaderRGBATexAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexColorMatrixAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(has_color_matrix_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexOpaque : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_OPAQUE);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATex : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);
    DCHECK_EQ(sampler_type_, SAMPLER_TYPE_EXTERNAL_OES);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

// Swizzles the red and blue component of sampled texel with alpha.
class FragmentShaderRGBATexSwizzleAlpha : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(swizzle_mode_, DO_SWIZZLE);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

// Swizzles the red and blue component of sampled texel without alpha.
class FragmentShaderRGBATexSwizzleOpaque : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(swizzle_mode_, DO_SWIZZLE);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_OPAQUE);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexClampAlphaAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, true);
    DCHECK_EQ(swizzle_mode_, DO_SWIZZLE);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaMask : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(mask_mode_, HAS_MASK);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);
    DCHECK_EQ(ignore_sampler_type_, true);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaMaskAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(mask_mode_, HAS_MASK);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);
    DCHECK_EQ(ignore_sampler_type_, true);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaMaskColorMatrixAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(mask_mode_, HAS_MASK);
    DCHECK_EQ(has_color_matrix_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);
    DCHECK_EQ(ignore_sampler_type_, true);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaColorMatrixAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(has_color_matrix_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderRGBATexAlphaMaskColorMatrix : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(has_uniform_alpha_, true);
    DCHECK_EQ(mask_mode_, HAS_MASK);
    DCHECK_EQ(has_color_matrix_, true);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_APPLY_BLEND_MODE);
    DCHECK_EQ(ignore_sampler_type_, true);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_RGBA_TEXTURE);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderYUVVideo : public FragmentShaderBase {
 public:
  FragmentShaderYUVVideo();

  void CheckSubclassProperties() {}

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index) override;
  int y_texture_location() const { return y_texture_location_; }
  int u_texture_location() const { return u_texture_location_; }
  int v_texture_location() const { return v_texture_location_; }
  int uv_texture_location() const { return uv_texture_location_; }
  int a_texture_location() const { return a_texture_location_; }
  int lut_texture_location() const { return lut_texture_location_; }
  int alpha_location() const { return alpha_location_; }
  int yuv_matrix_location() const { return yuv_matrix_location_; }
  int yuv_adj_location() const { return yuv_adj_location_; }
  int ya_clamp_rect_location() const { return ya_clamp_rect_location_; }
  int uv_clamp_rect_location() const { return uv_clamp_rect_location_; }
  int resource_multiplier_location() const {
    return resource_multiplier_location_;
  }
  int resource_offset_location() const { return resource_offset_location_; }

 private:
  template <class VertexShader, class FragmentShader>
  friend class ProgramBinding;

  std::string GetShaderSource() const override;

  bool use_alpha_texture_ = false;
  bool use_nv12_ = false;
  bool use_color_lut_ = false;

  int y_texture_location_ = -1;
  int u_texture_location_ = -1;
  int v_texture_location_ = -1;
  int uv_texture_location_ = -1;
  int a_texture_location_ = -1;
  int lut_texture_location_ = -1;
  int alpha_location_ = -1;
  int yuv_matrix_location_ = -1;
  int yuv_adj_location_ = -1;
  int ya_clamp_rect_location_ = -1;
  int uv_clamp_rect_location_ = -1;
  int resource_multiplier_location_ = -1;
  int resource_offset_location_ = -1;
};

class FragmentShaderColor : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_UNIFORM);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(aa_mode_, NO_AA);
    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

class FragmentShaderColorAA : public FragmentShaderBase {
 public:
  void CheckSubclassProperties() {
    DCHECK_EQ(input_color_type_, INPUT_COLOR_SOURCE_UNIFORM);
    DCHECK_EQ(aa_mode_, USE_AA);
    DCHECK_EQ(frag_color_mode_, FRAG_COLOR_MODE_DEFAULT);

    DCHECK_EQ(has_varying_alpha_, false);
    DCHECK_EQ(swizzle_mode_, NO_SWIZZLE);
    DCHECK_EQ(premultiply_alpha_mode_, PREMULTIPLIED_ALPHA);
    DCHECK_EQ(has_rgba_fragment_tex_transform_, false);
    DCHECK_EQ(ignore_sampler_type_, false);
    DCHECK_EQ(mask_mode_, NO_MASK);
    DCHECK_EQ(has_color_matrix_, false);
    DCHECK_EQ(has_uniform_alpha_, false);
    DCHECK_EQ(has_background_color_, false);
  }
};

}  // namespace cc

#endif  // CC_OUTPUT_SHADER_H_
