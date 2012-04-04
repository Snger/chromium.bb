#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This is a thin wrapper for native LD. This is not meant to be
# used by the user, but is called from pnacl-translate.
# This implements the native linking part of translation.
#
# All inputs must be native objects or linker scripts.
#
# --pnacl-sb will cause the sandboxed LD to be used.
# The bulk of this file is logic to invoke the sandboxed translator
# (SRPC and non-SRPC).

from driver_tools import *
from driver_env import env
from driver_log import Log

EXTRA_ENV = {
  'INPUTS'   : '',
  'OUTPUT'   : '',
  'SHM_FILE' : '',

  # Capture some metadata passed from pnacl-translate over to here.
  'SONAME' : '',

  'STDLIB': '1',
  'SHARED': '0',
  'STATIC': '0',
  'RELOCATABLE': '0',
  'USE_GOLD' : '0',

  'BASE_TEXT': '0x20000',
  'BASE_RODATA': '0x10020000',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

   # Intermediate variable LDMODE is used for delaying evaluation.
   # LD_SB is the non-srpc sandboxed tool.
  'LDMODE'      : '${SANDBOXED ? SB : ${USE_GOLD ? ALT : BFD}}',
  'LD'          : '${LD_%LDMODE%}',
  # --eh-frame-hdr asks the linker to generate an .eh_frame_hdr section,
  # which is a presorted list of registered frames. This section is
  # used by libgcc_eh/libgcc_s to avoid doing the sort during runtime.
  # http://www.airs.com/blog/archives/462
  #
  # -Tdata we use this flag to influence the placement of the ro segment
  # gold has been adjusted accordingly
  'LD_GOLD_FLAGS': '--rosegment --native-client ' +
                   '-Tdata=${BASE_RODATA} -Ttext=${BASE_TEXT} ',
  'LD_FLAGS'    : '${USE_GOLD ? ${LD_GOLD_FLAGS}} ' +
                  '-nostdlib -m ${LD_EMUL} --eh-frame-hdr ' +
                  '${#LD_SCRIPT ? -T ${LD_SCRIPT}} ' +
                  '${#SONAME ? -soname=${SONAME}} ' +
                  '${STATIC ? -static} ${SHARED ? -shared} ${RELOCATABLE ? -r}',

  # This may contain the metadata file, which is passed to LD with --metadata.
  # It must be passed at the end of the link line.
  'METADATA_FILE': '',

  'EMITMODE'         : '${RELOCATABLE ? relocatable : ' +
                       '${STATIC ? static : ' +
                       '${SHARED ? shared : dynamic}}}',

  'LD_SCRIPT': '${STDLIB ? ' +
               '${LD_SCRIPT_%LIBMODE%_%EMITMODE%} : ' +
               '${LD_SCRIPT_nostdlib}}',

  'LD_SCRIPT_nostdlib' : '',

  # For newlib, omit the -T flag (the builtin linker script works fine).
  'LD_SCRIPT_newlib_static': '',

  # For glibc, the linker script is always explicitly specified.
  'LD_SCRIPT_glibc_static' : '${LD_EMUL}.x.static',
  'LD_SCRIPT_glibc_shared' : '${LD_EMUL}.xs',
  'LD_SCRIPT_glibc_dynamic': '${LD_EMUL}.x',

  'LD_SCRIPT_newlib_relocatable': '',
  'LD_SCRIPT_glibc_relocatable' : '',

  'LD_EMUL'        : '${LD_EMUL_%ARCH%}',
  'LD_EMUL_ARM'    : 'armelf_nacl',
  'LD_EMUL_X8632'  : 'elf_nacl',
  'LD_EMUL_X8664'  : 'elf64_nacl',

  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',
  'SEARCH_DIRS_BUILTIN': '${STDLIB ? ${LIBS_ARCH}/}',

  'LIBS_ARCH'        : '${LIBS_%ARCH%}',
  'LIBS_ARM'         : '${BASE_LIB_NATIVE}arm',
  'LIBS_X8632'       : '${BASE_LIB_NATIVE}x86-32',
  'LIBS_X8664'       : '${BASE_LIB_NATIVE}x86-64',

  'RUN_LD' : '${LD} ${LD_FLAGS} ${inputs} -o "${output}" ' +
             '${#METADATA_FILE ? --metadata ${METADATA_FILE}}',
}

def PassThrough(*args):
  env.append('LD_FLAGS', *args)

def PassThroughSegment(*args):
  if not env.getbool('USE_GOLD'):
      PassThrough(*args)
      return
  # Note: we conflate '-Ttext' and '--section-start .text=' here
  # This is not quite right but it is the intention of the tests
  # using the flags which want to control the placement of the
  # "rx" and "r" segments
  if args[0] == '--section-start' and args[1].startswith('.rodata='):
    env.set('BASE_RODATA', args[1][len('.rodata='):])
  elif args[0] == '--section-start' and args[1].startswith('.text='):
    env.set('BASE_TEXT', args[1][len('.text='):])
  elif args[0].startswith('-Ttext='):
    env.set('BASE_TEXT', args[0][len('-Ttext='):])
  elif args[0] == '-Ttext':
    env.set('BASE_TEXT', args[1])
  else:
    Log.Fatal("Bad segment directive '%s'", repr(args))

LDPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( ('(--add-extra-dt-needed=.*)'), PassThrough),
  ( ('--metadata', '(.+)'),         "env.set('METADATA_FILE', $0)"),

  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),
  ( '-r',              "env.set('RELOCATABLE', '1')"),
  ( '-relocatable',    "env.set('RELOCATABLE', '1')"),

  ( '-L(.+)',          "env.append('SEARCH_DIRS_USER', $0)"),
  ( ('-L', '(.*)'),    "env.append('SEARCH_DIRS_USER', $0)"),

  ( ('(-Ttext)','(.*)'), PassThroughSegment),
  ( ('(-Ttext=.*)'),     PassThroughSegment),

  # This overrides the builtin linker script.
  ( ('-T', '(.*)'),      "env.set('LD_SCRIPT', $0)"),

  ( ('(-e)','(.*)'),              PassThrough),
  ( '(--entry=.*)',               PassThrough),
  ( ('(--section-start)','(.*)'), PassThroughSegment),
  ( '-?-soname=(.*)',             "env.set('SONAME', $0)"),
  ( ('(-?-soname)', '(.*)'),      "env.set('SONAME', $1)"),
  ( '(-M)',                       PassThrough),
  ( '(-t)',                       PassThrough),
  ( ('-y','(.*)'),                PassThrough),
  ( ('(-defsym)','(.*)'),         PassThrough),

  ( '(--print-gc-sections)',      PassThrough),
  ( '(-gc-sections)',             PassThrough),
  ( '(--unresolved-symbols=.*)',  PassThrough),

  ( '-melf_nacl',          "env.set('ARCH', 'X8632')"),
  ( ('-m','elf_nacl'),     "env.set('ARCH', 'X8632')"),
  ( '-melf64_nacl',        "env.set('ARCH', 'X8664')"),
  ( ('-m','elf64_nacl'),   "env.set('ARCH', 'X8664')"),
  ( '-marmelf_nacl',       "env.set('ARCH', 'ARM')"),
  ( ('-m','armelf_nacl'),  "env.set('ARCH', 'ARM')"),

  # Inputs and options that need to be kept in order
  ( '(--no-as-needed)',    "env.append('INPUTS', $0)"),
  ( '(--as-needed)',       "env.append('INPUTS', $0)"),
  ( '(--start-group)',     "env.append('INPUTS', $0)"),
  ( '(--end-group)',       "env.append('INPUTS', $0)"),
  ( '(-Bstatic)',          "env.append('INPUTS', $0)"),
  ( '(-Bdynamic)',         "env.append('INPUTS', $0)"),
  # This is the file passed using shmem
  ( ('--shm=(.*)'),        "env.append('INPUTS', $0)\n"
                           "env.set('SHM_FILE', $0)"),
  ( '(--(no-)?whole-archive)', "env.append('INPUTS', $0)"),

  ( '(-l.*)',              "env.append('INPUTS', $0)"),

  ( '(-.*)',               UnrecognizedOption),
  ( '(.*)',                "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  env.update(EXTRA_ENV)
  ParseArgs(argv, LDPatterns)

  GetArch(required=True)
  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if output == '':
    output = pathtools.normalize('a.out')

  # Default to -static for newlib
  if env.getbool('LIBMODE_NEWLIB'):
    env.set('STATIC', '1')

  # Expand all parameters
  # This resolves -lfoo into actual filenames,
  # and expands linker scripts into command-line arguments.
  inputs = ldtools.ExpandInputs(inputs,
                                env.get('SEARCH_DIRS'),
                                env.getbool('STATIC'))

  # If there's a linker script which needs to be searched for, find it.
  LocateLinkerScript()

  env.push()
  env.set('inputs', *inputs)
  env.set('output', output)
  if env.getbool('SANDBOXED') and env.getbool('SRPC'):
    RunLDSRPC()
  else:
    RunWithLog('${RUN_LD}')
  env.pop()
  return 0

def IsFlag(arg):
  return arg.startswith('-')

def RunLDSRPC():
  CheckTranslatorPrerequisites()
  # The "main" input file is the application's combined object file.
  all_inputs = env.get('inputs')

  main_input = env.getone('SHM_FILE')
  if not main_input:
    Log.Fatal("Sandboxed LD requires one shm input file")

  outfile = env.getone('output')

  files = LinkerFiles(all_inputs)
  ld_flags = env.get('LD_FLAGS')

  script = MakeSelUniversalScriptForLD(ld_flags,
                                       main_input,
                                       files,
                                       outfile)

  retcode, stdout, stderr = RunWithLog(
      '${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ' +
      '${SEL_UNIVERSAL_FLAGS} -- ${LD_SRPC}',
      stdin=script, echo_stdout=False, echo_stderr=False,
      return_stdout=True, return_stderr=True, errexit=False)
  if retcode:
    Log.FatalWithResult(retcode, 'ERROR: Sandboxed LD Failed. stdout:\n' +
    stdout + '\nstderr:\n' + stderr)
    driver_tools.DriverExit(retcode)

def MakeSelUniversalScriptForLD(ld_flags,
                                main_input,
                                files,
                                outfile):
  """ Return sel_universal script text for invoking LD.nexe with the
      given ld_flags, main_input (which is treated specially), and
      other input files. The output will be written to outfile.  """
  script = []

  # Open the output file.
  script.append('readwrite_file nexefile %s' % outfile)

  # Need to include the linker script file as a linker resource for glibc.
  files_to_map = list(files)
  if env.getbool('LIBMODE_GLIBC') and env.getbool('STDLIB'):
    ld_script = env.getone('LD_SCRIPT')
    assert(ld_script != '')
    files_to_map.append(ld_script)
  else:
    # Otherwise, use the built-in linker script.
    assert(env.getone('LD_SCRIPT') == '')

  # Create a mapping for each input file and add it to the command line.
  for f in files_to_map:
    basename = pathtools.basename(f)
    script.append('reverse_service_add_manifest_mapping files/%s %s' %
                  (basename, f))

  use_default = env.getbool('USE_DEFAULT_CMD_LINE')
  if use_default:
    # We don't have the bitcode file here anymore, so we assume that
    # pnacl-translate passed along the relevant information via commandline.
    soname = env.getone('SONAME')
    needed = GetNeededLibrariesString()
    emit_mode = env.getone('EMITMODE')
    if emit_mode == 'shared':
      is_shared_library = 1
    else:
      is_shared_library = 0

    script.append('readonly_file objfile %s' % main_input)
    script.append(('rpc RunWithDefaultCommandLine ' +
                   'h(objfile) h(nexefile) i(%d) s("%s") s("%s") *'
                   % (is_shared_library, soname, needed)))
  else:
    # Join all the arguments.
    # Based on the format of RUN_LD, the order of arguments is:
    # ld_flags, then input files (which are more sensitive to order).

    # For the sandboxed build, we don't want "real" filesystem paths,
    # because we use the reverse-service to do a lookup -- make sure
    # everything is a basename first.
    ld_flags = [ pathtools.basename(flag) for flag in ld_flags ]
    kTerminator = '\0'
    command_line = kTerminator.join(['ld'] + ld_flags) + kTerminator
    for f in files:
      basename = pathtools.basename(f)
      command_line = command_line + basename + kTerminator

    # TODO(pdox): Enable this.
    # Add the metadata file
    #metadata_file = env.getone('METADATA_FILE')
    #if metadata_file:
    #  command_line = command_line + '--metadata' + kTerminator
    #  command_line = command_line + metadata_file + kTerminator

    command_line_escaped = command_line.replace(kTerminator, '\\x00')
    # Assume that the commandline captures all necessary metadata for now.
    script.append('rpc Run h(nexefile) C(%d,%s) *' %
                  (len(command_line), command_line_escaped))
  script.append('echo "ld complete"')
  script.append('')
  return '\n'.join(script)

# Given linker arguments (including -L, -l, and filenames),
# returns the list of files which are pulled by the linker,
# with real path names set set up the real -> flat name mapping.
def LinkerFiles(args):
  ret = []
  for f in args:
    if IsFlag(f):
      continue
    else:
      if not pathtools.exists(f):
        Log.Fatal("Unable to open '%s'", pathtools.touser(f))
      ret.append(f)
  return ret

# Pull metadata passed into pnacl-nativeld from pnacl-translate via
#  commandline options. This can be removed if we have a more direct
# mechanism for transferring metadata from LLC -> LD.
def GetNeededLibrariesString():
  libs = []
  prefix = '--add-extra-dt-needed='
  for flag in env.get('LD_FLAGS'):
    if flag.startswith(prefix):
      libs.append(flag[len(prefix):])
  # This delimiter must be the same between LLC, this and LD.
  delim = '\\n'
  return delim.join(libs)

def LocateLinkerScript():
  ld_script = env.getone('LD_SCRIPT')
  if not ld_script:
    # No linker script specified
    return

  # See if it's an absolute or relative path
  path = pathtools.normalize(ld_script)
  if pathtools.exists(path):
    env.set('LD_SCRIPT', path)
    return

  # Search for the script
  search_dirs = [pathtools.normalize('.')] + env.get('SEARCH_DIRS')
  path = ldtools.FindFile([ld_script], search_dirs)
  if not path:
    Log.Fatal("Unable to find linker script '%s'", ld_script)

  env.set('LD_SCRIPT', path)
