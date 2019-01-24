#!/usr/bin/env python2

import sys
import re

# Purpose: The purpose of this tool is to generate a MSVS style map file from
# a combination of dumpbin-style map and a llvm-style map.  Some in-house
# tools that parse these map files require them to be MSVS formatted.  These
# parsers are very sensitive to lexical tokens so even the slightest format
# changes can break the parser.  These mynute requirements will be noted with
# comments.

# The following two functions parse a dumpbin-style map file and extract only
# the necessary information needed during the construction of the MSVS style
# map file.
def parse_dumpbin_line(dumpbin_map, pattern, line):
  match = pattern.match(line)
  if match:
    dumpbin_map.append({
      'addr':   match.group(1),
      'rva':    match.group(2).lower(),
      'symbol': match.group(3),
      'object': '<unknown>'
    })

def read_dumpbin_map(dumpbin_map, filename):
  pattern = re.compile(
    '([0-9a-f]{4}:[0-9a-f]{8})' + # address
    ' +' +                        # [whitespace]
    '([0-9a-f]{8,16})' +          # RVA
    ' +' +                        # [whitespace]
    '[0-9a-f]{8,16}' +            # (unknown)
    ' +' +                        # [whitespace]
    '[0-9]+'                      # size
    ' +' +                        # [whitespace]
    '([^ ]{2,})' +                # symbol
    '\\Z',                        # [end of string]
    re.IGNORECASE)

  with open(filename, 'r') as fp:
    line = fp.readline()
    while line:
      parse_dumpbin_line(dumpbin_map, pattern, line.strip())
      line = fp.readline()

# The following two functions parse a llvm-style map file and extract only
# necessary information needed during the construction of the MSVS style
# map file.
def parse_llvm_line(llvm_map, pattern, line):
  match = pattern.match(line)
  if match:
    rva = match.group(1).lower()
    object_name = match.group(2)
    llvm_map[rva] = object_name

def read_llvm_map(llvm_map, filename):
  pattern = re.compile(
    '([0-9a-f]{8,16})' +    # RVA
    ' +' +                  # [whitespace]
    '[0-9a-f]{8,16}' +      # size
    ' +' +                  # [whitespace]
    '[0-9]+'                # alignment
    ' +' +                  # [whitespace]
    '([^ ]{2,}[.]obj):.*' + # object name
    '\\Z',                  # [end of string]
    re.IGNORECASE)

  with open(filename, 'r') as fp:
    line = fp.readline()
    while line:
      parse_llvm_line(llvm_map, pattern, line.strip())
      line = fp.readline()

# The following two functions consume the data collected from the
# dumpbin-style map file and the llvm-style map file to generate a dataset
# that can be used to write a MSVS style map.
def formatted_object_name(llvm_map, pattern, rva, symbol):
  object_name = ''

  if rva not in llvm_map:
    if '@' in symbol:
      object_name = 'kernel32:KERNEL32.dll'
    elif symbol.startswith('___safe_se_handler_'):
      # Reference: https://github.com/llvm-mirror/lld/blob/master/COFF/Driver.cpp#L1500
      object_name = '<linker-defined>'
    else:
      object_name = '<common>'

  else:
    raw_object_name = llvm_map[rva]

    if raw_object_name.startswith('obj/'):
      match = pattern.match(raw_object_name)
      object_name = match.group(1)
    elif 'libcmt.nativeproj' in raw_object_name or 'minkernel\\crts\\crtw32' in raw_object_name:
      match = pattern.match(raw_object_name)
      object_name = 'libcmt:' + match.group(1)
    elif 'libvcruntime.nativeproj' in raw_object_name:
      match = pattern.match(raw_object_name)
      object_name = 'libvcruntime:' + match.group(1)
    elif 'minkernel\\crts\\ucrt' in raw_object_name:
      match = pattern.match(raw_object_name)
      object_name = 'libucrt:' + match.group(1)
    else:
      object_name = '<unknown>'

  return object_name

def generate_msvs_map(msvs_map, dumpbin_map, llvm_map):
  pattern = re.compile('.*[\\\\/](.*[.]obj)')

  for mapping in dumpbin_map:
    rva = mapping['rva']
    symbol = mapping['symbol']

    msvs_map.append({
      'addr':   mapping['addr'],
      'rva':    rva,
      'symbol': symbol,
      'object': formatted_object_name(llvm_map, pattern, rva, symbol)
    })

# Write the MSVS style map to standard out
def write_msvs_map(msvs_map):
  # Parser requires: '.+Preferred load address is [0-9]{8,16}'
  # This is the base address of the image file.
  #
  # In the MSVS formatted map, each row contains the virtual address of the
  # function.  In the dumpbin formatted map, each row contains the RVA of the
  # function.  Since the parser only cares about the RVA value, we specify a
  # base address of 0.  This allows the parser to subtract 0 from the
  # "virtual address" to get the RVA.
  print("Fake Preferred load address is 00000000\n")
  print(" {0}         {1}              {2}       {3}\n".format(
    "Address",
    "Publics by Value",
    "Rva+Base",
    "Lib:Object"));

  for mapping in msvs_map:
    print(' {0}       {1: <26} {2}     {3}'.format(
        mapping['addr'],
        mapping['symbol'],
        mapping['rva'],
        mapping['object']))

def main(args):
  llvm_map_filename = None
  dumpbin_map_filename = None

  for i in range(len(args[1:])):
    if args[i] == '--llvm-map':
      llvm_map_filename = args[i+1]
    elif args[i] == '--dumpbin-map':
      dumpbin_map_filename = args[i+1]
    elif args[i].startswith('-'):
      print("Usage: {0} --llvm-map <input> --dumpbin-map <input>".format(args[0]))
      return 1

  if not llvm_map_filename or not dumpbin_map_filename:
    print("Usage: {0} --llvm-map <input> --dumpbin-map <input>".format(args[0]))
    return 1

  dumpbin_map = []
  read_dumpbin_map(dumpbin_map, dumpbin_map_filename)

  llvm_map = {}
  read_llvm_map(llvm_map, llvm_map_filename)

  msvs_map = []
  generate_msvs_map(msvs_map, dumpbin_map, llvm_map)
  write_msvs_map(msvs_map)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

