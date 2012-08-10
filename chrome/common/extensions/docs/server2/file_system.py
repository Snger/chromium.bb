# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

class FileNotFoundError(Exception):
  def __init__(self, filename):
    Exception.__init__(self, filename)

def _ProcessFileData(data, path):
  if os.path.splitext(path)[-1] not in ['.js', '.html', '.json']:
    return data
  try:
    return unicode(data, 'utf-8')
  except:
    return unicode(data, 'latin-1')

class FileSystem(object):
  """A FileSystem interface that can read files and directories.
  """
  class StatInfo(object):
    """The result of calling Stat on a FileSystem.
    """
    def __init__(self, version):
      self.version = version

  def Read(self, paths, binary=False):
    """Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.

    If binary=False, the contents of each file will be unicode parsed as utf-8,
    and failing that as latin-1 (some extension docs use latin-1). If
    binary=True then the contents will be a str.
    """
    raise NotImplementedError()

  def ReadSingle(self, path):
    """Reads a single file from the FileSystem.
    """
    return self.Read([path]).Get()[path]

  def Stat(self, path):
    """Gets the version number of |path| if it is a directory, or the parent
    directory if it is a file.
    """
    raise NotImplementedError()
