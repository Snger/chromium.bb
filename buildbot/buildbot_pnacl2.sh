#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

######################################################################
# SCRIPT CONFIG
######################################################################

CLOBBER=${CLOBBER:-yes}
SCONS_COMMON="./scons --mode=opt-host bitcode=1 -j8"
SPEC_HARNESS=${SPEC_HARNESS:-${HOME}/cpu2000-redhat64-ia32}/

# Rough test running time classification for ARM which is our bottleneck
FAST_ARM="176.gcc 181.mcf 197.parser 254.gap"
MEDIUM_ARM="164.gzip 175.vpr 179.art 186.crafty 252.eon \
            256.bzip2 255.vortex 300.twolf"
SLOW_ARM="177.mesa 183.equake 188.ammp 253.perlbmk"

SPEC_BASE="tests/spec2k"

TestsToBuild() {
  local setup=$1
  case ${setup} in
    SetupPnaclArmOpt)
      # we expect arm to diverge
      echo ${FAST_ARM} 252.eon 179.art
      ;;
    SetupPnaclTranslator*)
      echo 176.gcc
      ;;
    *)
      echo ${FAST_ARM} 252.eon 179.art
      ;;
  esac
}

TestsToRun() {
  local setup=$1
  case ${setup} in
    SetupPnaclArmOpt)
      # we expect arm to diverge
      echo ${FAST_ARM} 252.eon 179.art
      ;;
    SetupPnaclTranslator*)
      echo 176.gcc
      ;;
    *)
      echo ${FAST_ARM} 252.eon 179.art
      ;;
  esac
}

######################################################################
# SCRIPT ACTION
######################################################################

clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out toolchain

  echo "@@@BUILD_STEP gclient_runhooks@@@"
  gclient runhooks --force
}

basic-setup() {
  local platforms=$1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons sel_ldr [${platform}]@@@"
    ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal
  done
}

build-and-run-some() {
  local harness=$1
  local setups=$2

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train-some]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness ${harness}
    MAKEOPTS=-j8 \
      ./run_all.sh BuildBenchmarks 0 ${setup} $(TestsToBuild ${setup})

    echo "@@@BUILD_STEP spec2k run [${setup}] [train-some]@@@"
    ./run_all.sh RunBenchmarks ${setup} train $(TestsToRun ${setup}) || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}

build-and-run-all-timed() {
  local harness=$1
  local setups=$2

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness ${harness}
    MAKEOPTS=-j8 \
      ./run_all.sh BuildBenchmarks 1 ${setup} train

    echo @@@BUILD_STEP spec2k run [${setup}] [train]@@@
    # NOTE: we intentionally do not parallelize the build because
    # we are measuring build times
    ./run_all.sh RunTimedBenchmarks ${setup} train || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}


######################################################################
# NOTE: trybots only runs a subset of the the spec2k tests
# TODO: elminate this long running bot in favor per arch sharded bots
mode-spec-pnacl-trybot() {
  clobber
  basic-setup "arm x86-64 x86-32"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclArmOpt \
                                      SetupPnaclX8632Opt \
                                      SetupPnaclX8664Opt \
                                      SetupPnaclTranslatorX8632Opt \
                                      SetupPnaclTranslatorX8664Opt"
}

mode-spec-pnacl-trybot-arm() {
  clobber
  basic-setup "arm"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclArmOpt"
}

mode-spec-pnacl-trybot-x8632() {
  clobber
  basic-setup "x86-32"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclX8632Opt \
                                      SetupPnaclTranslatorX8632Opt"
}

mode-spec-pnacl-trybot-x8664() {
  clobber
  basic-setup "x86-64"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclX8664Opt \
                                      SetupPnaclTranslatorX8664Opt"
}


mode-spec-pnacl-arm() {
  clobber
  basic-setup "arm"
  # arm takes a long time and we do not have sandboxed tests working
  build-and-run-all-timed ${SPEC_HARNESS} "SetupPnaclArmOpt"
}

mode-spec-pnacl-x8664() {
  clobber
  basic-setup "x86-64"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupPnaclX8664 \
                           SetupPnaclX8664Opt \
                           SetupPnaclTranslatorX8664 \
                           SetupPnaclTranslatorX8664Opt"
}

mode-spec-pnacl-x8632() {
  clobber
  basic-setup "x86-32"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupPnaclX8632 \
                           SetupPnaclX8632Opt \
                           SetupPnaclTranslatorX8632 \
                           SetupPnaclTranslatorX8632Opt"
}

mode-spec-nacl() {
  clobber
  basic-setup "x86-32 x86-64"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupNaclX8664 \
                           SetupNaclX8664Opt \
                           SetupNaclX8632 \
                           SetupNaclX8632Opt"
}


######################################################################
# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi


if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  exit 1
fi

if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"

