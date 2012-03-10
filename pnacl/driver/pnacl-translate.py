#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import driver_tools
import pathtools
import shutil
from driver_env import env
from driver_log import Log, TempFiles
import re

EXTRA_ENV = {
  'PIC'           : '0',

  # Use the IRT shim by default on x86-64. This can be disabled with an
  # explicit flag (--noirtshim) or via -nostdlib.
  'USE_IRT_SHIM'  : '${ARCH==X8664 && !SHARED ? 1 : 0}',

  # Flags for pnacl-nativeld
  'LD_FLAGS': '${STATIC ? -static} ${SHARED ? -shared}',

  'STATIC'         : '0',
  'SHARED'         : '0',
  'STDLIB'         : '1',
  'USE_DEFAULTLIBS': '1',

  'INPUTS'        : '',
  'OUTPUT'        : '',
  'OUTPUT_TYPE'   : '',

  # Library Strings
  'LD_ARGS' : '${STDLIB ? ${LD_ARGS_normal} : ${LD_ARGS_nostdlib}}',

  'LD_ARGS_IRT_SHIM':
    '--entry=_pnacl_wrapper_start -l:libpnacl_irt_shim.a',

  'CRTBEGIN' : '${SHARED ? -l:crtbeginS.o : -l:crtbegin.o}',
  'CRTEND'   : '${SHARED ? -l:crtendS.o : -l:crtend.o}',
  'LIBGCC_EH': '${STATIC ? -l:libgcc_eh.a : -l:libgcc_s.so.1}',

  'LD_ARGS_nostdlib': '-nostdlib ${ld_inputs}',

  # These are just the dependencies in the native link.
  'LD_ARGS_normal':
    '${USE_IRT_SHIM ? ${LD_ARGS_IRT_SHIM}} ' +
    '${CRTBEGIN} ${ld_inputs} ' +
    '${STATIC ? --start-group} ' +
    '${USE_DEFAULTLIBS ? ${DEFAULTLIBS}} ' +
    '${STATIC ? --end-group} ' +
    '${CRTEND}',

  # TODO(pdox): To simplify translation, reduce from 3 to 2 cases.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2423
  'DEFAULTLIBS':
    '${LINKER_HACK} ${LIBGCC_EH} -l:libgcc.a ${MISC_LIBS}',

  'MISC_LIBS':
    # TODO(pdox):
    # Move libcrt_platform into the __pnacl namespace,
    # with stubs to access it from newlib.
    '${LIBMODE_NEWLIB ? -l:libcrt_platform.a} ' +
    # This is needed because the ld.so sonames don't match
    # between X86-32 and X86-64.
    # TODO(pdox): Unify the names.
    '${LIBMODE_GLIBC && !STATIC ? -l:ld-2.9.so}',

  # Because our bitcode linker doesn't record symbol resolution information,
  # some libraries still need to be included directly in the native link.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=577
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2089
  'LINKER_HACK': '', # Populated in function ApplyBitcodeConfig().

  # Intermediate variable LLCVAR is used for delaying evaluation.
  'LLCVAR'        : '${SANDBOXED ? LLC_SB : LLVM_LLC}',
  'LLC'           : '${%LLCVAR%}',

  'TRIPLE'      : '${TRIPLE_%ARCH%}',
  'TRIPLE_ARM'  : 'armv7a-none-nacl-gnueabi',
  'TRIPLE_X8632': 'i686-none-nacl-gnu',
  'TRIPLE_X8664': 'x86_64-none-nacl-gnu',

  'LLC_FLAGS_COMMON': '-asm-verbose=false -tail-merge-threshold=50 ' +
                      '${PIC ? -relocation-model=pic}',

  'LLC_FLAGS_ARM'    :
    # The following options might come in handy and are left here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float -aeabi-calls -sfi-zero-mask',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    ('-arm-reserve-r9 -sfi-disable-cp -arm_static_tls ' +
     '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables'),

  'LLC_FLAGS_X8632' : '',
  'LLC_FLAGS_X8664' : '',

  # LLC flags which set the target and output type.
  # These are handled separately by libLTO.
  'LLC_FLAGS_TARGET' : '-march=${LLC_MARCH} -mcpu=${LLC_MCPU_%ARCH%} ' +
                       '-mtriple=${TRIPLE} -filetype=${filetype}',
  # Additional non-default flags go here.
  'LLC_FLAGS_EXTRA' : '',
  'LLC_FLAGS_BASE': '${LLC_FLAGS_COMMON} ${LLC_FLAGS_%ARCH%} '
                    '${LLC_FLAGS_EXTRA}',
  'LLC_FLAGS'     : '${LLC_FLAGS_TARGET} ${LLC_FLAGS_BASE}',

  'LLC_MARCH'       : '${LLC_MARCH_%ARCH%}',
  'LLC_MARCH_ARM'   : 'arm',
  'LLC_MARCH_X8632' : 'x86',
  'LLC_MARCH_X8664' : 'x86-64',

  'LLC_MCPU'        : '${LLC_MCPU_%ARCH%}',
  'LLC_MCPU_ARM'    : 'cortex-a8',
  'LLC_MCPU_X8632'  : 'pentium4',
  'LLC_MCPU_X8664'  : 'core2',

  'RUN_LLC'       : '${LLC} ${LLC_FLAGS} ${input} -o ${output} ' +
                    '-metadata-out ${output}.meta',
  'STREAM_BITCODE' : '0',
}

TranslatorPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-S',              "env.set('OUTPUT_TYPE', 's')"), # Stop at .s
  ( '-c',              "env.set('OUTPUT_TYPE', 'o')"), # Stop at .o

  # Expose a very limited set of llc flags. Used primarily for
  # the shared lib ad-hoc tests, c.f. tests/pnacl_ld_example
  ( '(-sfi-.+)',        "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '(-mtls-use-call)', "env.append('LLC_FLAGS_EXTRA', $0)"),
  ( '-translate-fast',  "env.append('LLC_FLAGS_EXTRA', '-O0')"),

  # If translating a .pexe which was linked statically against
  # glibc, then you must do pnacl-translate -static. This will
  # be removed once GLibC is actually statically linked.
  ( '-static',         "env.set('STATIC', '1')"),
  ( '-shared',         "env.set('SHARED', '1')"),
  ( '-nostdlib',       "env.set('STDLIB', '0')"),

  # Disables the default libraries.
  # This flag is needed for building libgcc_s.so.
  ( '-nodefaultlibs',  "env.set('USE_DEFAULTLIBS', '0')"),

  ( '--noirtshim',      "env.set('USE_IRT_SHIM', '0')"),

  ( '-rpath-link=(.+)', "env.append('LD_FLAGS', '-L'+$0)"),

  ( '-fPIC',           "env.set('PIC', '1')"),

  ( '-Wl,(.*)',        "env.append('LD_FLAGS', *($0).split(','))"),
  ( '-bitcode-stream-rate=([0-9]+)', "env.set('STREAM_BITCODE', $0)"),

  ( '(-.*)',            driver_tools.UnrecognizedOption),

  ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]

def main(argv):
  env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, TranslatorPatterns)

  if env.getbool('SHARED') and env.getbool('STATIC'):
    Log.Fatal('Cannot mix -static and -shared')

  driver_tools.GetArch(required = True)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')

  if len(inputs) == 0:
    Log.Fatal("No input files")

  if output == '':
    Log.Fatal("Please specify output file with -o")

  # Find the bitcode file on the command line.
  bcfiles = filter(driver_tools.IsBitcode, inputs)
  if len(bcfiles) > 1:
    Log.Fatal('Expecting at most 1 bitcode file')
  elif len(bcfiles) == 1:
    bcfile = bcfiles[0]
  else:
    bcfile = None

  # If there's a bitcode file, translate it now.
  tng = driver_tools.TempNameGen(inputs + bcfiles, output)
  output_type = env.getone('OUTPUT_TYPE')
  if bcfile:
    sfile = None
    if output_type == 's':
      sfile = output
    elif env.getbool('FORCE_INTERMEDIATE_S'):
      sfile = tng.TempNameForInput(bcfile, 's')

    ofile = None
    if output_type == 'o':
      ofile = output
    elif output_type != 's':
      ofile = tng.TempNameForInput(bcfile, 'o')

    if sfile:
      RunLLC(bcfile, sfile, filetype='asm')
      if ofile:
        RunAS(sfile, ofile)
    else:
      RunLLC(bcfile, ofile, filetype='obj')
  else:
    ofile = None

  # If we've been told to stop after translation, stop now.
  if output_type in ('o','s'):
    return 0

  # Replace the bitcode file with __BITCODE__ in the input list
  if bcfile:
    inputs = ListReplace(inputs, bcfile, '__BITCODE__')
    env.set('INPUTS', *inputs)

  # Get bitcode type and metadata
  if bcfile:
    bctype = driver_tools.FileType(bcfile)
    metadata = driver_tools.GetBitcodeMetadata(bcfile)

  # Determine the output type, in this order of precedence:
  # 1) Output type can be specified on command-line (-S, -c, -shared, -static)
  # 2) If bitcode file given, use it's output type. (pso->so, pexe->nexe, po->o)
  # 3) Otherwise, assume nexe output.
  if env.getbool('SHARED'):
    output_type = 'so'
  elif env.getbool('STATIC'):
    output_type = 'nexe'
  elif bcfile:
    DefaultOutputTypes = {
      'pso' : 'so',
      'pexe': 'nexe',
      'po'  : 'o',
    }
    output_type = DefaultOutputTypes[bctype]
  else:
    output_type = 'nexe'

  # If the bitcode is of type "object", no linking is required.
  if output_type == 'o':
    # Copy ofile to output
    Log.Info('Copying %s to %s' % (ofile, output))
    shutil.copy(pathtools.tosys(ofile), pathtools.tosys(output))
    return 0

  if bcfile:
    ApplyBitcodeConfig(metadata, bctype)

  # Default to -static for newlib.
  # TODO(pdox): This shouldn't be necessary.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2423
  if env.getbool('LIBMODE_NEWLIB'):
    if env.getbool('SHARED'):
      Log.Fatal('Cannot handle -shared with newlib toolchain')
    env.set('STATIC', '1')

  assert output_type in ('so','nexe')
  RunLD(ofile, output)
  return 0

def ApplyBitcodeConfig(metadata, bctype):
  if bctype == 'pso':
    env.set('SHARED', '1')

  if env.getbool('SHARED'):
    env.set('PIC', '1')

  # Normally, only pso files need to be translated with PIC, but since we
  # are linking executables with unresolved symbols, dynamic nexe's
  # also need to be PIC to be able to generate the correct relocations.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2351
  if bctype == 'pexe' and env.getbool('LIBMODE_GLIBC'):
    env.set('PIC', '1')
    env.append('LD_FLAGS', '--unresolved-symbols=ignore-all')

  # Read the bitcode metadata to extract library
  # dependencies and SOName.
  # For now, we use LD_FLAGS to convey the information.
  # However, if the metadata becomes richer we will need another mechanism.
  # TODO(jvoung): at least grep out the SRPC output from LLC and transmit
  # that directly to LD to avoid issues with mismatching delimiters.
  for needed in metadata['NeedsLibrary']:
    env.append('LD_FLAGS', '--add-extra-dt-needed=' + needed)
  if bctype == 'pso':
    soname = metadata['SOName']
    if soname:
      env.append('LD_FLAGS', '-soname=' + soname)

  # Certain libraries still need to be linked directly.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2451
  direct_libs = ['libsrpc',
                 'libppapi_cpp',
                 'libstdc++',
                 'libm',
                 'libc',
                 'libpthread']
  for name in direct_libs:
    for needed in metadata['NeedsLibrary']:
      if needed.startswith(name):
        env.append('LINKER_HACK', '-l:'+needed)
        if name == 'libc' or name == 'libpthread':
          env.append('LINKER_HACK', '-l:%s_nonshared.a' % name)
        break

def RunAS(infile, outfile):
  driver_tools.RunDriver('as', [infile, '-o', outfile])

def ListReplace(items, old, new):
  ret = []
  for k in items:
    if k == old:
      ret.append(new)
    else:
      ret.append(k)
  return ret

def RequiresNonStandardLDCommandline(inputs, infile):
  ''' Determine when we must force USE_DEFAULT_CMD_LINE off for running
  the sandboxed LD (if link line is completely non-standard).
  '''
  if len(inputs) > 1:
    # There must have been some native objects on the link line.
    # In that case, if we are using the sandboxed translator, we cannot
    # currently allow that with the default commandline (only one input).
    return ('Native link with more than one native object: %s' % str(inputs),
            True)
  if not infile:
    return ('No bitcode input: %s' % str(infile), True)
  if not env.getbool('STDLIB'):
    return ('NOSTDLIB', True)
  if (driver_tools.GetArch(required=True) == 'X8664' and
      not env.getbool('SHARED') and
      not env.getbool('USE_IRT_SHIM')):
    return ('USE_IRT_SHIM false when normally true', True)
  return (None, False)

def ToggleDefaultCommandlineLD(inputs, infile):
  if env.getbool('USE_DEFAULT_CMD_LINE'):
    reason, non_standard = RequiresNonStandardLDCommandline(inputs, infile)
    if non_standard:
      Log.Info(reason + ' -- not using default SRPC commandline for LD!')
      inputs.append('--pnacl-driver-set-USE_DEFAULT_CMD_LINE=0')

def RequiresNonStandardLLCCommandline():
  extra_flags = env.get('LLC_FLAGS_EXTRA')
  if extra_flags != []:
    reason = 'Has additional llc flags: %s' % extra_flags
    return (reason, True)
  else:
    return (None, False)

def UseDefaultCommandlineLLC():
  if not env.getbool('USE_DEFAULT_CMD_LINE'):
    return False
  else:
    reason, non_standard = RequiresNonStandardLLCCommandline()
    if non_standard:
      Log.Info(reason + ' -- not using default SRPC commandline for LLC!')
      return False
    return True

def RunLD(infile, outfile):
  inputs = env.get('INPUTS')
  if infile:
    inputs = ListReplace(inputs, '__BITCODE__', '--shm=' + infile)
  ToggleDefaultCommandlineLD(inputs, infile)
  env.set('ld_inputs', *inputs)
  args = env.get('LD_ARGS') + ['-o', outfile]
  args += env.get('LD_FLAGS')
  # If there is bitcode, there is also a metadata file.
  if infile:
    args += ['--metadata', '%s.meta' % infile]
  driver_tools.RunDriver('nativeld', args)

def RunLLC(infile, outfile, filetype):
  UseSRPC = env.getbool('SANDBOXED') and env.getbool('SRPC')
  # For now, sel_universal doesn't properly support dynamic sb translator
  if env.getbool('SANDBOXED') and env.getbool('SB_DYNAMIC'):
    driver_tools.CheckTranslatorPrerequisites()
    UseSRPC = False
  env.push()
  env.setmany(input=infile, output=outfile, filetype=filetype)
  if UseSRPC:
    is_shared, soname, needed = RunLLCSRPC()
    env.pop()
    # soname and dt_needed libs are returned from LLC and passed to LD
    driver_tools.SetBitcodeMetadata(infile, is_shared, soname, needed)
  else:
    driver_tools.RunWithLog("${RUN_LLC}")
    # As a side effect, this creates a temporary file
    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(outfile + '.meta')
    env.pop()
  return 0

def RunLLCSRPC():
  driver_tools.CheckTranslatorPrerequisites()
  infile = env.getone('input')
  outfile = env.getone('output')
  flags = env.get('LLC_FLAGS')
  script = MakeSelUniversalScriptForLLC(infile, outfile, flags)
  retcode, stdout, stderr = driver_tools.RunWithLog('${SEL_UNIVERSAL_PREFIX} ' +
                 '${SEL_UNIVERSAL} ${SEL_UNIVERSAL_FLAGS} -- ${LLC_SRPC}',
                  stdin=script, echo_stdout=False, echo_stderr=False,
                  return_stdout=True)
  # Get the values returned from the llc RPC to use in input to ld
  is_shared = re.search(r'output\s+0:\s+i\(([0|1])\)', stdout).group(1)
  is_shared = (is_shared == '1')
  if is_shared:
    assert env.getbool('SHARED')
  soname = re.search(r'output\s+1:\s+s\("(.*)"\)', stdout).group(1)
  needed_str = re.search(r'output\s+2:\s+s\("(.*)"\)', stdout).group(1)
  # If the delimiter changes, this line needs to change
  needed_libs = [ lib for lib in needed_str.split(r'\n') if lib]
  return is_shared, soname, needed_libs

def MakeSelUniversalScriptForLLC(infile, outfile, flags):
  script = []
  script.append('readwrite_file objfile %s' % outfile)
  stream_bitcode = int(env.getraw('STREAM_BITCODE'))
  if stream_bitcode == 0:
    script.append('readonly_file myfile %s' % infile)
    if UseDefaultCommandlineLLC():
      script.append('rpc RunWithDefaultCommandLine  h(myfile) h(objfile) *'
                    ' i() s() s()');
    else:
      # command_line is a NUL (\x00) terminated sequence.
      kTerminator = '\0'
      command_line = kTerminator.join(['llc'] + flags) + kTerminator
      command_line_escaped = command_line.replace(kTerminator, '\\x00')
      script.append('rpc Run h(myfile) h(objfile) C(%d,%s) * i() s() s()' %
                    (len(command_line), command_line_escaped))
  else:
    script.append('rpc StreamInit h(objfile) * s()')
    # specify filename, chunk size and rate in bits/s
    script.append('stream_file %s %s %s' % (infile, 64 * 1024, stream_bitcode))
    script.append('rpc StreamEnd * i() s() s() s()')
  script.append('echo "llc complete"')
  script.append('')
  return '\n'.join(script)

def get_help(argv):
  return """
PNaCl bitcode to native code translator.

Usage: pnacl-translate [options] -arch <arch> <input> -o <output>

  <input>            Input file (bitcode).
  -arch <arch>       Translate to <arch> (i686, x86_64, armv7)
  -o <output>        Output file.

  The output file type depends on the input file type:
     Portable object (.po)         -> NaCl ELF object (.o)
     Portable shared object (.pso) -> NaCl ELF shared object (.so)
     Portable executable (.pexe)   -> NaCl ELF executable (.nexe)

ADVANCED OPTIONS:
  -S                 Generate native assembly only.
  -c                 Generate native object file only.
  --pnacl-sb         Use the translator which runs inside the NaCl sandbox.
"""
