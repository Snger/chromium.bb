#!/usr/bin/env python

# Copyright (C) 2017 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script is based on deport_tools/gn.py, and contains code copied from it.
# That file carries the following license:
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is a wrapper around gn for generating blpwtk2 build tree
#
# Usage:
# blpwtk2.py                    -- Generate out/shared/Debug
#                                           out/shared/Release
#                                           out/static/Debug
#                                           out/static/Release
#                                           out/static/BuildMaster
#
# blpwtk2.py shared             -- Generate out/shared/Debug
#                                           out/shared/Release
#
# blpwtk2.py static             -- Generate out/static/Debug
#                                           out/static/Release
#                                           out/static/BuildMaster
#
# blpwtk2.py Debug              -- Generate out/shared/Debug
#                                           out/static/Debug
#
# blpwtk2.py Release            -- Generate out/shared/Release
#                                           out/static/Release
#
# blpwtk2.py BuildMaster        -- Generate out/static/BuildMaster
#
# blpwtk2.py shared Debug       -- Generate out/shared/Debug
# blpwtk2.py shared Release     -- Generate out/shared/Release
# blpwtk2.py static Debug       -- Generate out/static/Debug
# blpwtk2.py static Release     -- Generate out/static/Release
# blpwtk2.py static BuildMaster -- Generate out/static/BuildMaster

import os
import sys
import subprocess

# Update the version of content shell here
content_version = '56.0.2924.122'

if sys.platform == 'win32':
  try:
    sys.path.insert(0, os.path.join(chrome_src, 'third_party', 'psyco_win32'))
    import psyco
  except:
    psyco = None
else:
  psyco = None

def verifyGN():
  try:
    subprocess.check_output(["which", "gn"], stderr=subprocess.STDOUT)
    return 0
  except subprocess.CalledProcessError:
    return 1

def applyVariableToEnvironment(env, var, val):
  if env in os.environ:
    envItems = os.environ[env].split(" ")
  else:
    envItems = []
  for i in range(0, len(envItems)):
    iSplit = envItems[i].split("=")
    if iSplit[0] == var:
      # Found it, remove current, replace with new value
      del envItems[i]
      break
  envItems.append(var + "=" + val)
  os.environ[env] = " ".join(envItems)


def createBuildCmd(gn_cmds, gn_mode, gn_type, bb_version):
  gn_cmd = ''
  version = ''
  if bb_version:
    version = ' bb_version=\\\"' + bb_version + '\\\"'

  if gn_mode == 'shared' and gn_type == 'Debug':
      gn_cmd = ' gen out/' + gn_mode + '/Debug' + ' ' \
               + '--args="' + os.environ['GN_DEFINES'] + version + '"'
  elif gn_mode == 'shared' and gn_type == 'Release':
      gn_cmd = ' gen out/' + gn_mode + '/Release' + ' ' \
               + '--args="' + os.environ['GN_DEFINES'] + version + '"'
  elif gn_mode == 'static' and gn_type == 'Debug':
      gn_cmd = ' gen out/' + gn_mode + '/Debug' + ' ' \
               + '--args="' + os.environ['GN_DEFINES'] + version + '"'
  elif gn_mode == 'static' and gn_type == 'Release':
      gn_cmd = ' gen out/' + gn_mode + '/Release' + ' ' \
               + '--args="' + os.environ['GN_DEFINES'] + version + '"'
  elif gn_mode == 'static' and gn_type == 'BuildMaster':
      gn_cmd = ' gen out/' + gn_mode + '/BuildMaster' + ' ' \
               + '--args="' + os.environ['GN_DEFINES'] + version + '"'
  gn_cmds.append(gn_cmd)

def parseArgs(argv):
  gn_shared = []
  gn_static = []
  gn_mode = None
  gn_type = None
  bb_version = None

  if argv:
    for i in xrange(0, len(argv)):
      arg = argv[i]
      if arg == 'shared' or arg == 'static':
        gn_mode = arg
      elif arg == 'Debug' or arg == 'Release' or arg == 'BuildMaster':
        gn_type = arg
      elif arg == '--bb_version':
        with open('../devkit_version.txt', 'r') as f:
          bb_version = f.readline()
  
  # Generate 32-bit binaries and libraries
  applyVariableToEnvironment('GN_DEFINES', 'target_cpu', '\\\"x86\\\"')

  # Disable NativeClient, print preview, browser extensiion support
  applyVariableToEnvironment('GN_DEFINES', 'enable_nacl', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_print_preview', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_extensions', 'false')

  # Disable safe browsing mode
  applyVariableToEnvironment('GN_DEFINES', 'safe_browsing_mode', '0')

  # Enable proprietary codecs
  applyVariableToEnvironment('GN_DEFINES', 'proprietary_codecs', 'true')
  applyVariableToEnvironment('GN_DEFINES', 'ffmpeg_branding', '\\\"Chrome\\\"')

  # Apply the content shell version
  applyVariableToEnvironment('GN_DEFINES', 'content_shell_version', '\\\"' + content_version + '\\\"')

  if gn_mode == 'shared':
    applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
    if not gn_type:
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
      createBuildCmd(gn_shared, gn_mode, 'Debug', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
      createBuildCmd(gn_shared, gn_mode, 'Release', bb_version)
    else:
      if gn_type == 'Debug':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
      else:
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
      createBuildCmd(gn_shared, gn_mode, gn_type, bb_version)
  elif gn_mode == 'static':
    applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
    if not gn_type:
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
      createBuildCmd(gn_static, gn_mode, 'Debug', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
      createBuildCmd(gn_static, gn_mode, 'Release', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'true')
      createBuildCmd(gn_static, gn_mode, 'BuildMaster', bb_version)
    else:
      if gn_type == 'Debug':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
        createBuildCmd(gn_static, gn_mode, gn_type, bb_version)
      elif gn_type == 'Release':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
        createBuildCmd(gn_static, gn_mode, gn_type, bb_version)
      elif gn_type == 'BuildMaster':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
        applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'true')
        createBuildCmd(gn_static, gn_mode, gn_type, bb_version)
  else:
    if not gn_type:
      # Make GN shared Debug/Release tree
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
      createBuildCmd(gn_shared, 'shared', 'Debug', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
      createBuildCmd(gn_shared, 'shared', 'Release', bb_version)

      # Make GN static Debug/Release/BuildMaster tree
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
      createBuildCmd(gn_static, 'static', 'Debug', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
      createBuildCmd(gn_static, 'static', 'Release', bb_version)
      applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'true')
      createBuildCmd(gn_static, 'static', 'BuildMaster', bb_version)
    else:
      if gn_type == 'Debug':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')

        applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
        createBuildCmd(gn_shared, 'shared', gn_type, bb_version)

        applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
        createBuildCmd(gn_static, 'static', gn_type, bb_version)

      elif gn_type == 'Release':
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')

        applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
        createBuildCmd(gn_shared, 'shared', gn_type, bb_version)

        applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
        createBuildCmd(gn_static, 'static', gn_type, bb_version)

      elif gn_type == 'BuildMaster':
        applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'true')
        applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')
        applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
        createBuildCmd(gn_static, 'static', gn_type, bb_version)

  return gn_shared, gn_static, gn_type

def generateBuildTree(cmds):
  for cmd in cmds:
    os.system("gn" + cmd)

def main(argv):
  if verifyGN():
    print "Please install depot_tools."
    return 1

  gn_shared, gn_static, gn_type = parseArgs(argv)

  if len(gn_shared)  > 0:
    if gn_type:
      print "Generating GN shared %s build tree" % gn_type
    else:
      print "Generating GN shared Debug and Release build trees"
  sys.stdout.flush()

  generateBuildTree(gn_shared)

  if len(gn_static)  > 0:
    if gn_type:
      print "Generating GN static %s build tree" % gn_type
    else:
      print "Generating GN static Debug, Release and BuildMaster build trees"
  sys.stdout.flush()
  generateBuildTree(gn_static)

  return 0

if __name__ == '__main__':
  # Use the Psyco JIT if available.
  if psyco:
    psyco.profile()
    print "Enabled Psyco JIT."

  sys.exit(main(sys.argv[1:]))

