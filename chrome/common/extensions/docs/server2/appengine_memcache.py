# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

MEMCACHE_FILE_SYSTEM_READ = 'MemcacheFileSystem.Get'
MEMCACHE_FILE_SYSTEM_STAT = 'MemcacheFileSystem.Stat'
MEMCACHE_BRANCH_UTILITY = 'BranchUtility'

class AppEngineMemcache(object):
  """A wrapper around the standard App Engine memcache that enforces namespace
  use. Uses a branch to make sure there are no key collisions if separate
  branches cache the same file.
  """
  def __init__(self, branch, memcache):
    self._branch = branch
    self._memcache = memcache

  def Set(self, key, value, namespace, time=60):
    return self._memcache.set(key,
                              value,
                              namespace=self._branch + '.' + namespace,
                              time=time)

  def Get(self, key, namespace):
    return self._memcache.get(key, namespace=self._branch + '.' + namespace)

  def Delete(self, key, namespace):
    return self._memcache.delete(key, namespace=self._branch + '.' + namespace)
