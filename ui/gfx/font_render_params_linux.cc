// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/switches.h"

#include <fontconfig/fontconfig.h>

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/gfx/linux_font_delegate.h"
#endif

namespace gfx {

namespace {

bool SubpixelPositioningRequested(bool for_web_contents) {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      for_web_contents ? switches::kEnableWebkitTextSubpixelPositioning
                       : switches::kEnableBrowserTextSubpixelPositioning);
}

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT: return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM: return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:   return FontRenderParams::HINTING_FULL;
    default:             return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:  return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:  return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB: return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR: return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:           return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Queries Fontconfig for rendering settings and updates |params_out| and
// |family_out| (if non-NULL). Returns false on failure. See
// GetCustomFontRenderParams() for descriptions of arguments.
bool QueryFontconfig(const std::vector<std::string>* family_list,
                     const int* pixel_size,
                     const int* point_size,
                     FontRenderParams* params_out,
                     std::string* family_out) {
  FcPattern* pattern = FcPatternCreate();
  CHECK(pattern);

  if (family_list) {
    for (std::vector<std::string>::const_iterator it = family_list->begin();
         it != family_list->end(); ++it) {
      FcPatternAddString(
          pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(it->c_str()));
    }
  }
  if (pixel_size)
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, *pixel_size);
  if (point_size)
    FcPatternAddInteger(pattern, FC_SIZE, *point_size);

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(NULL, pattern, &result);
  FcPatternDestroy(pattern);
  if (!match)
    return false;

  if (family_out) {
    FcChar8* family = NULL;
    FcPatternGetString(match, FC_FAMILY, 0, &family);
    if (family)
      family_out->assign(reinterpret_cast<const char*>(family));
  }

  if (params_out) {
    FcBool fc_antialias = 0;
    FcPatternGetBool(match, FC_ANTIALIAS, 0, &fc_antialias);
    params_out->antialiasing = fc_antialias;

    FcBool fc_autohint = 0;
    FcPatternGetBool(match, FC_AUTOHINT, 0, &fc_autohint);
    params_out->autohinter = fc_autohint;

    FcBool fc_hinting = 0;
    int fc_hint_style = FC_HINT_NONE;
    FcPatternGetBool(match, FC_HINTING, 0, &fc_hinting);
    if (fc_hinting)
      FcPatternGetInteger(match, FC_HINT_STYLE, 0, &fc_hint_style);
    params_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);

    int fc_rgba = FC_RGBA_NONE;
    FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba);
    params_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
  }

  FcPatternDestroy(match);
  return true;
}

// Initializes |params| with the system's default settings.
void LoadDefaults(FontRenderParams* params, bool for_web_contents) {
  *params = GetCustomFontRenderParams(for_web_contents, NULL, NULL, NULL, NULL);
}

}  // namespace

const FontRenderParams& GetDefaultFontRenderParams() {
  static bool loaded_defaults = false;
  static FontRenderParams default_params;
  if (!loaded_defaults)
    LoadDefaults(&default_params, /* for_web_contents */ false);
  loaded_defaults = true;
  return default_params;
}

const FontRenderParams& GetDefaultWebKitFontRenderParams() {
  static bool loaded_defaults = false;
  static FontRenderParams default_params;
  if (!loaded_defaults)
    LoadDefaults(&default_params, /* for_web_contents */ true);
  loaded_defaults = true;
  return default_params;
}

FontRenderParams GetCustomFontRenderParams(
    bool for_web_contents,
    const std::vector<std::string>* family_list,
    const int* pixel_size,
    const int* point_size,
    std::string* family_out) {
  FontRenderParams params;
  if (family_out)
    family_out->clear();

#if defined(OS_CHROMEOS)
  // Use reasonable defaults.
  params.antialiasing = true;
  params.autohinter = true;
  params.use_bitmaps = true;
  params.hinting = FontRenderParams::HINTING_SLIGHT;

  // Query Fontconfig to get the family name and subpixel rendering setting.
  // In general, we try to limit Chrome OS's dependency on Fontconfig, but it's
  // used to configure fonts for different scripts and to disable subpixel
  // rendering on systems that use external displays.
  // TODO(derat): Decide if we should just use Fontconfig wholeheartedly on
  // Chrome OS; Blink is using it, after all.
  FontRenderParams fc_params;
  QueryFontconfig(family_list, pixel_size, point_size, &fc_params, family_out);
  params.subpixel_rendering = fc_params.subpixel_rendering;
#else
  // Start with the delegate's settings, but let Fontconfig have the final say.
  // TODO(derat): Figure out if we need to query the delegate at all. Does
  // GtkSettings always get overridden by Fontconfig in GTK apps?
  const LinuxFontDelegate* delegate = LinuxFontDelegate::instance();
  if (delegate) {
    params.antialiasing = delegate->UseAntialiasing();
    params.hinting = delegate->GetHintingStyle();
    params.subpixel_rendering = delegate->GetSubpixelRenderingStyle();
  }
  QueryFontconfig(family_list, pixel_size, point_size, &params, family_out);
#endif

  params.subpixel_positioning = SubpixelPositioningRequested(for_web_contents);

  // To enable subpixel positioning, we need to disable hinting.
  if (params.subpixel_positioning)
    params.hinting = FontRenderParams::HINTING_NONE;

  // Use the first family from the list if Fontconfig didn't suggest a family.
  if (family_out && family_out->empty() &&
      family_list && !family_list->empty())
    *family_out = (*family_list)[0];

  return params;
}

}  // namespace gfx
