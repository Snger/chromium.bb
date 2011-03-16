#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import optparse
import os.path
import sys

# Allow the import of third party modules
script_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(script_dir, '../../../third_party/pylib/'))

import browsertester.browserlauncher
import browsertester.rpclistener
import browsertester.server


def BuildArgParser():
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage)

  parser.add_option('-p', '--port', dest='port', action='store', type='int',
                    default='0', help='The TCP port the server will bind to. '
                    'The default is to pick an unused port number.')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Use the browser located here.')
  parser.add_option('--map_file', dest='map_files', action='append',
                    type='string', nargs=2, default=[],
                    metavar='DEST SRC',
                    help='Add file SRC to be served from the HTTP server, '
                    'to be made visible under the path DEST.')
  parser.add_option('-f', '--file', dest='files', action='append',
                    type='string', default=[],
                    metavar='FILENAME',
                    help='Add a file to serve from the HTTP server, to be '
                    'made visible in the root directory.  '
                    '"--file path/to/foo.html" is equivalent to '
                    '"--map_file foo.html path/to/foo.html"')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None,
                    help='The webpage to load.')
  parser.add_option('--ppapi_plugin', dest='ppapi_plugin', action='store',
                    type='string', default=None,
                    help='Use the browser plugin located here.')
  parser.add_option('--sel_ldr', dest='sel_ldr', action='store',
                    type='string', default=None,
                    help='Use the sel_ldr located here.')
  parser.add_option('--interactive', dest='interactive', action='store_true',
                    default=False, help='Do not quit after testing is done. '
                    'Handy for iterative development.  Disables timeout.')
  parser.add_option('--debug', dest='debug', action='store_true', default=False,
                    help='Request debugging output from browser.')
  parser.add_option('--timeout', dest='timeout', action='store', type='float',
                    default=5.0,
                    help='The maximum amount of time to wait for the browser to'
                    ' make a request. The timer resets with each request.')
  parser.add_option('--allow_404', dest='allow_404', action='store_true',
                    default=False,
                    help='Allow 404s to occur without failing the test.')
  parser.add_option('--extension', dest='browser_extensions', action='append',
                    type='string', default=[],
                    help='Load the browser extensions located at the list of '
                    'paths.  Note: this currently only works with the Chrome '
                    'browser.')

  return parser


def Run(url, options):
  # Create server
  server = browsertester.server.Create('localhost', options.port)

  # If port 0 has been requested, an arbitrary port will be bound so we need to
  # query it.  Older version of Python do not set server_address correctly when
  # The requested port is 0 so we need to break encapsulation and query the
  # socket directly.
  host, port = server.socket.getsockname()

  file_mapping = dict(options.map_files)
  for filename in options.files:
    file_mapping[os.path.basename(filename)] = filename
  for server_path, real_path in file_mapping.iteritems():
    if not os.path.exists(real_path):
      raise AssertionError('\'%s\' does not exist.' % real_path)

  listener = browsertester.rpclistener.RPCListener(server.TestingEnded)
  server.Configure(file_mapping, options.allow_404, listener)

  browser = browsertester.browserlauncher.ChromeLauncher(options)

  browser.Run('http://%s:%d/%s' % (host, port, url))
  server.TestingBegun(0.125)

  try:
    while server.test_in_progress or options.interactive:
      if not browser.IsRunning():
        listener.ServerError('Browser process ended during test')
        break
      elif not options.interactive and server.TimedOut(options.timeout):
        listener.ServerError('Did not hear from the browser for %.1f seconds' %
                             options.timeout)
        break
      else:
        server.handle_request()
  finally:
    browser.Cleanup()
    server.server_close()

  if listener.ever_failed:
    return 1
  else:
    return 0


def RunFromCommandLine():
  parser = BuildArgParser()
  options, args = parser.parse_args()

  if len(args) != 0:
    parser.error('Invalid arguments')

  # Validate the URL
  url = options.url
  if url is None:
    parser.error('Must specify a URL')
  if not (url.endswith('.html') or url.endswith('.htm')):
    parser.error('URL must be a HTML file.')

  # Look for files in the browserdata directory as a last resort
  options.files.append(os.path.join(script_dir, 'browserdata', 'nacltest.js'))

  return Run(url, options)


if __name__ == '__main__':
  sys.exit(RunFromCommandLine())

