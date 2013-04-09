# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import collections
import contextlib
import json
import logging
import os

from chromite import cros
from chromite.lib import cache
from chromite.lib import chrome_util
from chromite.lib import cros_build_lib
from chromite.lib import gclient
from chromite.lib import gs
from chromite.lib import osutils
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants


COMMAND_NAME = 'chrome-sdk'


class SDKFetcher(object):
  """Functionality for fetching an SDK environment.

  For the version of ChromeOS specified, the class downloads and caches
  SDK components.
  """
  SDK_VERSION_ENV = '%SDK_VERSION'
  SDK_BOARD_ENV = '%SDK_BOARD'

  SDKContext = collections.namedtuple(
      'SDKContext', ['version', 'metadata', 'key_map'])

  TARBALL_CACHE = 'tarballs'
  MISC_CACHE = 'misc'

  TARGET_TOOLCHAIN_KEY = 'target_toolchain'

  def __init__(self, cache_dir, board):
    """Initialize the class.

    Arguments:
      cache_dir: The toplevel cache dir to use.
      board: The board to manage the SDK for.
    """
    self.gs_ctx = gs.GSContext.Cached(cache_dir, init_boto=True)
    self.cache_base = os.path.join(cache_dir, COMMAND_NAME)
    self.tarball_cache = cache.TarballCache(
        os.path.join(self.cache_base, self.TARBALL_CACHE))
    self.misc_cache = cache.DiskCache(
        os.path.join(self.cache_base, self.MISC_CACHE))
    self.board = board
    self.gs_base = self._GetGSBaseForBoard(board)

  @staticmethod
  def _GetGSBaseForBoard(board):
    config = cbuildbot_config.FindCanonicalConfigForBoard(board)
    return '%s/%s' % (constants.DEFAULT_ARCHIVE_BUCKET, config['name'])

  def _UpdateTarball(self, url, ref):
    """Worker function to fetch tarballs"""
    with osutils.TempDir(base_dir=self.tarball_cache.staging_dir) as tempdir:
      local_path = os.path.join(tempdir, os.path.basename(url))
      self.gs_ctx.Copy(url, tempdir)
      ref.SetDefault(local_path, lock=True)

  def _GetMetadata(self, version):
    """Return metadata (in the form of a dict) for a given version."""
    raw_json = None
    version_base = os.path.join(self.gs_base, version)
    with self.misc_cache.Lookup(
        (self.board, version, constants.METADATA_JSON)) as ref:
      if ref.Exists(lock=True):
        raw_json = osutils.ReadFile(ref.path)
      else:
        raw_json = self.gs_ctx.Cat(
            os.path.join(version_base, constants.METADATA_JSON)).output
        ref.AssignText(raw_json)

    return json.loads(raw_json)

  def _GetChromeLKGM(self):
    """Get ChromeOS LKGM checked into the Chrome tree."""
    gclient_root = gclient.FindGclientCheckoutRoot(os.getcwd())
    if gclient_root is not None:
      version = osutils.ReadFile(os.path.join(
          gclient_root, constants.PATH_TO_CHROME_LKGM))
      real_path = self.gs_ctx.LS(os.path.join(
          self.gs_base, 'R??-%s' % version)).output.splitlines()[0]
      if not real_path.startswith(self.gs_base):
        raise AssertionError('%s does not start with %s'
                             % (real_path, self.gs_base))
      real_path = real_path[len(self.gs_base) + 1:]
      full_version = real_path.partition('/')[0]
      if not full_version.endswith(version):
        raise AssertionError('%s does not end with %s'
                             % (full_version, version))
      return full_version
    return None

  def _GetNewestManifestVersion(self):
    version_file = '%s/LATEST-master' % self.gs_base
    return self.gs_ctx.Cat(version_file).output

  def GetDefaultVersion(self):
    """Get the default SDK version to use.

    If we are in an existing SDK shell, use the SDK version of the shell.
    Otherwise, use what we defaulted to last time.  If there was no last time,
    use the result of UpdateDefaultVersion().
    """
    if os.environ.get(self.SDK_BOARD_ENV) == self.board:
      sdk_version = os.environ.get(self.SDK_VERSION_ENV)
      if sdk_version is not None:
        return sdk_version

    with self.misc_cache.Lookup((self.board, 'latest')) as ref:
      if ref.Exists(lock=True):
        return osutils.ReadFile(ref.path).strip()
      else:
        return self.UpdateDefaultVersion()

  def _SetDefaultVersion(self, version):
    """Set the new default version."""
    with self.misc_cache.Lookup((self.board, 'latest')) as ref:
      ref.AssignText(version)

  def UpdateDefaultVersion(self):
    """Update the version that we default to using."""
    target = self._GetChromeLKGM()
    if target is None:
      target = self._GetNewestManifestVersion()
    self._SetDefaultVersion(target)
    return target

  @contextlib.contextmanager
  def Prepare(self, components, version=None):
    """Ensures the components of an SDK exist and are read-locked.

    For a given SDK version, pulls down missing components, and provides a
    context where the components are read-locked, which prevents the cache from
    deleting them during its purge operations.

    Arguments:
      gs_ctx: GSContext object.
      components: A list of specific components(tarballs) to prepare.
      version: The version to prepare.  If not set, uses the version returned by
        GetDefaultVersion().

    Yields: An SDKFetcher.SDKContext namedtuple object.  The attributes of the
      object are:

      version: The version that was prepared.
      metadata: A dictionary containing the build metadata for the SDK version.
      key_map: Dictionary that contains CacheReference objects for the SDK
        artifacts, indexed by cache key.
    """
    if version is None:
      version = self.GetDefaultVersion()
    components = list(components)

    key_map = {}
    fetch_urls = {}
    version_base = os.path.join(self.gs_base, version)

    metadata = self._GetMetadata(version)
    # Fetch toolchains from separate location.
    if self.TARGET_TOOLCHAIN_KEY in components:
      tc_tuple = metadata['toolchain-tuple'][0]
      fetch_urls[self.TARGET_TOOLCHAIN_KEY] = os.path.join(
          'gs://', constants.SDK_GS_BUCKET,
          metadata['toolchain-url'] % {'target': tc_tuple})
      components.remove(self.TARGET_TOOLCHAIN_KEY)

    fetch_urls.update((t, os.path.join(version_base, t)) for t in components)
    try:
      for key, url in fetch_urls.iteritems():
        cache_key = (self.board, version, key)
        ref = self.tarball_cache.Lookup(cache_key)
        key_map[key] = ref
        ref.Acquire()
        if not ref.Exists(lock=True):
          # TODO(rcui): Parallelize this.  Requires acquiring locks *before*
          # generating worker processes; therefore the functionality needs to
          # be moved into the DiskCache class itself -
          # i.e.,DiskCache.ParallelSetDefault().
          self._UpdateTarball(url, ref)

      yield self.SDKContext(version, metadata, key_map)
    finally:
      # TODO(rcui): Move to using cros_build_lib.ContextManagerStack()
      cros_build_lib.SafeRun([ref.Release for ref in key_map.itervalues()])


@cros.CommandDecorator(COMMAND_NAME)
class ChromeSDKCommand(cros.CrosCommand):
  """
  Pulls down SDK components for building and testing Chrome for ChromeOS,
  sets up the environment for building Chrome, and runs a command in the
  environment, starting a bash session if no command is specified.

  The bash session environment is set up by a user-configurable rc file located
  at ~/chromite/chrome_sdk.bashrc.
  """

  EBUILD_ENV = (
      'CXX',
      'CC',
      'AR',
      'AS',
      'LD',
      'RANLIB',
      'GOLD_SET',
      'GYP_DEFINES',
  )

  @classmethod
  def AddParser(cls, parser):
    super(ChromeSDKCommand, cls).AddParser(parser)
    parser.add_argument(
        '--board', required=True, help='The board SDK to use.')
    parser.add_argument(
        '--bashrc', type=osutils.ExpandPath,
        default=constants.CHROME_SDK_BASHRC,
        help='A bashrc file used to set up the SDK shell environment. '
             'Defaults to %s.' % constants.CHROME_SDK_BASHRC)
    parser.add_argument(
        '--clang', action='store_true', default=False,
        help='Sets up the environment for building with clang.  Due to a bug '
             'with ninja, requires --make to be set.')
    parser.add_argument(
        '--internal', action='store_true', default=False,
        help='Sets up SDK for building official (internal) Chrome '
             'Chrome, rather than Chromium.')
    parser.add_argument(
        '--make', action='store_true', default=False,
        help='If set, gyp_chromium will generate Make files instead of Ninja '
             'files.  Note: Make files are spread out through the source tree, '
             'and not concentrated in the out_<board> directory, so you can '
             'only have one Make config running at a time.')
    parser.add_argument(
        '--chroot', type=osutils.ExpandPath,
        help='Path to a ChromeOS chroot to use.  If set, '
             '<chroot>/build/<board> will be used as the sysroot that Chrome '
             'is built against.  The version shown in the SDK shell prompt '
             'will then have an asterisk prepended to it.')
    parser.add_argument(
        '--update', action='store_true', default=False,
        help='Force download of latest SDK version for the board.')
    parser.add_argument(
        '--version', default=None,
        help='Specify version of SDK to use.  Defaults to determining version '
             'based on the type of checkout (Chrome or ChromeOS) you are '
             'executing from.')
    parser.add_argument(
        'cmd', nargs='*', default=None,
        help='The command to execute in the SDK environment.  Defaults to '
              'starting a bash shell.')

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.board = options.board
    # Lazy initialized.
    self.sdk = None

  @staticmethod
  def _CreatePS1(board, version, chroot=None):
    """Returns PS1 string that sets commandline and xterm window caption.

    If a chroot path is set, then indicate we are using the sysroot from there
    instead of the stock sysroot by prepending an asterisk to the version.

    Arguments:
      board, version: The SDK board and version.
      chroot: The path to the chroot, if set.
    """
    custom = '*' if chroot else ''
    sdk_version = '(sdk %s %s%s)' % (board, custom, version)
    label = '\\u@\\h: \\w'
    window_caption = "\\[\\e]0;%(sdk_version)s %(label)s \\a\\]"
    command_line = "%(sdk_version)s \\[\\e[1;33m\\]%(label)s \\$ \\[\\e[m\\]"
    ps1 = window_caption + command_line
    return (ps1 % {'sdk_version': sdk_version,
                   'label': label})

  def _FixGoldPath(self, var_contents, toolchain_path):
    """Point to the gold linker in the toolchain tarball.

    Accepts an already set environment variable in the form of '<cmd>
    -B<gold_path>', and overrides the gold_path to the correct path in the
    extracted toolchain tarball.

    Arguments:
      var_contents: The contents of the environment variable.
      toolchain_path: Path to the extracted toolchain tarball contents.

    Returns:
      Environment string that has correct gold path.
    """
    cmd, _, gold_path = var_contents.partition(' -B')
    gold_path = os.path.join(toolchain_path, gold_path.lstrip('/'))
    return '%s -B%s' % (cmd, gold_path)

  def _SetupTCEnvironment(self, sdk_ctx, options, env):
    """Sets up toolchain-related environment variables."""
    target_tc = sdk_ctx.key_map[self.sdk.TARGET_TOOLCHAIN_KEY].path
    tc_bin = os.path.join(target_tc, 'bin')
    env['PATH'] = '%s:%s' % (tc_bin, os.environ['PATH'])

    for var in ('CXX', 'CC', 'LD'):
      env[var] = self._FixGoldPath(env[var], target_tc)

    if options.clang:
      # clang++ requires C++ header paths to be explicitly specified.
      # See discussion on crbug.com/86037.
      tc_tuple = sdk_ctx.metadata['toolchain-tuple'][0]
      gcc_path = os.path.join(tc_bin, '%s-gcc' % tc_tuple)
      gcc_version = cros_build_lib.DebugRunCommand(
          [gcc_path, '-dumpversion'], redirect_stdout=True).output.strip()
      gcc_lib = 'usr/lib/gcc/%(tuple)s/%(ver)s/include/g++-v%(major_ver)s' % {
          'tuple': tc_tuple,
          'ver': gcc_version,
          'major_ver': gcc_version[0],
      }
      tc_gcc_lib = os.path.join(target_tc, gcc_lib)
      includes = []
      for p in ('',  tc_tuple, 'backward'):
        includes.append('-isystem %s' % os.path.join(tc_gcc_lib, p))
      env['CC'] = 'clang'
      env['CXX'] = 'clang++ %s' % ' '.join(includes)

    env['CXX_host'] = 'g++'
    env['CC_host'] = 'gcc'

  def _SetupEnvironment(self, board, sdk_ctx, options):
    """Sets environment variables to export to the SDK shell."""
    if options.chroot:
      sysroot = os.path.join(options.chroot, 'build', board)
      if not os.path.isdir(sysroot) and not options.cmd:
        logging.warning("Because --chroot is set, expected a sysroot to be at "
                        "%s, but couldn't find one.", sysroot)
    else:
      sysroot = sdk_ctx.key_map[constants.CHROME_SYSROOT_TAR].path

    environment = os.path.join(sdk_ctx.key_map[constants.CHROME_ENV_TAR].path,
                               'environment')
    env = osutils.SourceEnvironment(environment, self.EBUILD_ENV)
    self._SetupTCEnvironment(sdk_ctx, options, env)

    os.environ[self.sdk.SDK_VERSION_ENV] = sdk_ctx.version
    os.environ[self.sdk.SDK_BOARD_ENV] = board
    # Export the board/version info in a more accessible way, so developers can
    # reference them in their chrome_sdk.bashrc files, as well as within the
    # chrome-sdk shell.
    for var in [self.sdk.SDK_VERSION_ENV, self.sdk.SDK_BOARD_ENV]:
      os.environ[var.lstrip('%')] = os.environ[var]

    # SYSROOT is necessary for Goma and the sysroot wrapper.
    env['SYSROOT'] = sysroot
    gyp_dict = chrome_util.ProcessGypDefines(env['GYP_DEFINES'])
    gyp_dict['sysroot'] = sysroot
    gyp_dict.pop('order_text_section', None)
    if options.clang:
      gyp_dict['clang'] = 1
      gyp_dict['werror'] = ''
      gyp_dict['clang_use_chrome_plugins'] = 0
      gyp_dict['linux_use_tcmalloc'] = 0
    if options.internal:
      gyp_dict['branding'] = 'Chrome'
      gyp_dict['buildtype'] = 'Official'
    else:
      gyp_dict.pop('branding', None)
      gyp_dict.pop('buildtype', None)

    env['GYP_DEFINES'] = chrome_util.DictToGypDefines(gyp_dict)

    # PS1 sets the command line prompt and xterm window caption.
    env['PS1'] = self._CreatePS1(
        self.board, sdk_ctx.version, chroot=options.chroot)

    out_dir = 'out_%s' % self.board
    env['builddir_name'] = out_dir
    env['GYP_GENERATORS'] = 'make' if options.make else 'ninja'
    env['GYP_GENERATOR_FLAGS'] = 'output_dir=%s' % out_dir
    return env

  @staticmethod
  def _VerifyGoma(user_rc):
    """Verify that the user's goma installation is working.

    Arguments:
      user_rc: User-supplied rc file.
    """
    user_env = osutils.SourceEnvironment(user_rc, ['PATH'], env=True)
    goma_ctl = osutils.Which('goma_ctl.sh', user_env.get('PATH'))
    if goma_ctl is not None:
      manifest = os.path.join(os.path.dirname(goma_ctl), 'MANIFEST')
      platform_env = osutils.SourceEnvironment(manifest, ['PLATFORM'])
      platform = platform_env.get('PLATFORM')
      if platform is not None and platform != 'chromeos':
        cros_build_lib.Warning('Found %s version of Goma in PATH.', platform)
        cros_build_lib.Warning('Goma will not work')

  @contextlib.contextmanager
  def _GetRCFile(self, env, user_rc):
    """Returns path to dynamically created bashrc file.

    The bashrc file sets the environment variables contained in |env|, as well
    as sources the user-editable chrome_sdk.bashrc file in the user's home
    directory.  That rc file is created if it doesn't already exist.

    Arguments:
      env: A dictionary of environment variables that will be set by the rc
        file.
      user_rc: User-supplied rc file.
    """
    if not os.path.exists(user_rc):
      osutils.Touch(user_rc, makedirs=True)

    self._VerifyGoma(user_rc)

    # We need a temporary rc file to 'wrap' the user configuration file,
    # because running with '--rcfile' causes bash to ignore bash special
    # variables passed through subprocess.Popen, such as PS1.  So we set them
    # here.
    # Having a wrapper rc file will also allow us to inject bash functions into
    # the environment, not just variables.
    with osutils.TempDir() as tempdir:
      # Only source the user's ~/.bashrc if running in interactive mode.
      contents = [
          '[[ -e ~/.bashrc && $- == *i* ]] && . ~/.bashrc\n',
      ]

      for key, value in env.iteritems():
        contents.append("export %s='%s'\n" % (key, value))
      contents.append('. "%s"\n' % user_rc)

      rc_file = os.path.join(tempdir, 'rcfile')
      osutils.WriteFile(rc_file, contents)
      yield rc_file

  def Run(self):
    """Perform the command."""
    if os.environ.get(SDKFetcher.SDK_VERSION_ENV) is not None:
      cros_build_lib.Die('Already in an SDK shell.')

    if self.options.clang and not self.options.make:
      cros_build_lib.Die('--clang requires --make to be set.')

    # Lazy initialize because SDKFetcher creates a GSContext() object in its
    # constructor, which may block on user input.
    self.sdk = SDKFetcher(self.options.cache_dir, self.options.board)

    prepare_version = self.options.version
    if self.options.update:
      prepare_version = self.sdk.UpdateDefaultVersion()

    components = [self.sdk.TARGET_TOOLCHAIN_KEY, constants.CHROME_ENV_TAR]
    if not self.options.chroot:
      components.append(constants.CHROME_SYSROOT_TAR)

    with self.sdk.Prepare(components, version=prepare_version) as ctx:
      env = self._SetupEnvironment(self.options.board, ctx, self.options)
      with self._GetRCFile(env, self.options.bashrc) as rcfile:
        bash_cmd = ['/bin/bash']

        extra_env = None
        if not self.options.cmd:
          bash_cmd.extend(['--rcfile', rcfile, '-i'])
        else:
          # The '"$@"' expands out to the properly quoted positional args
          # coming after the '--'.
          bash_cmd.extend(['-c', '"$@"', '--'])
          bash_cmd.extend(self.options.cmd)
          # When run in noninteractive mode, bash sources the rc file set in
          # BASH_ENV, and ignores the --rcfile flag.
          extra_env = {'BASH_ENV': rcfile}

        cros_build_lib.RunCommand(
            bash_cmd, print_cmd=False, debug_level=logging.CRITICAL,
            error_code_ok=True, extra_env=extra_env)
