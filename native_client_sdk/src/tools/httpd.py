# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import logging
import multiprocessing
import optparse
import os
import SimpleHTTPServer  # pylint: disable=W0611
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_SDK_ROOT = os.path.dirname(SCRIPT_DIR)


serve_dir = None
delegate_map = {}


# We only run from the examples directory so that not too much is exposed
# via this HTTP server.  Everything in the directory is served, so there should
# never be anything potentially sensitive in the serving directory, especially
# if the machine might be a multi-user machine and not all users are trusted.
# We only serve via the loopback interface.
def SanityCheckDirectory(dirname):
  abs_serve_dir = os.path.abspath(dirname)

  # Verify we don't serve anywhere above NACL_SDK_ROOT.
  if abs_serve_dir[:len(NACL_SDK_ROOT)] == NACL_SDK_ROOT:
    return
  logging.error('For security, httpd.py should only be run from within the')
  logging.error('example directory tree.')
  logging.error('Attempting to serve from %s.' % abs_serve_dir)
  logging.error('Run with --no_dir_check to bypass this check.')
  sys.exit(1)


class PluggableHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  def _FindDelegateAtPath(self, dirname):
    # First check the cache...
    logging.debug('Looking for cached delegate in %s...' % dirname)
    handler_script = os.path.join(dirname, 'handler.py')

    if dirname in delegate_map:
      result = delegate_map[dirname]
      if result is None:
        logging.debug('Found None.')
      else:
        logging.debug('Found delegate.')
      return result

    # Don't have one yet, look for one.
    delegate = None
    logging.debug('Testing file %s for existence...' % handler_script)
    if os.path.exists(handler_script):
      logging.debug(
          'File %s exists, looking for HTTPRequestHandlerDelegate.' %
          handler_script)

      module = imp.load_source('handler', handler_script)
      delegate_class = getattr(module, 'HTTPRequestHandlerDelegate', None)
      delegate = delegate_class()
      if not delegate:
        logging.warn(
            'Unable to find symbol HTTPRequestHandlerDelegate in module %s.' %
            handler_script)

    return delegate

  def _FindDelegateForURLRecurse(self, cur_dir, abs_root):
    delegate = self._FindDelegateAtPath(cur_dir)
    if not delegate:
      # Didn't find it, try the parent directory, but stop if this is the server
      # root.
      if cur_dir != abs_root:
        parent_dir = os.path.dirname(cur_dir)
        delegate = self._FindDelegateForURLRecurse(parent_dir, abs_root)

    logging.debug('Adding delegate to cache for %s.' % cur_dir)
    delegate_map[cur_dir] = delegate
    return delegate

  def _FindDelegateForURL(self, url_path):
    path = self.translate_path(url_path)
    if os.path.isdir(path):
      dirname = path
    else:
      dirname = os.path.dirname(path)

    abs_serve_dir = os.path.abspath(serve_dir)
    delegate = self._FindDelegateForURLRecurse(dirname, abs_serve_dir)
    if not delegate:
      logging.info('No handler found for path %s. Using default.' % url_path)
    return delegate

  def send_head(self):
    delegate = self._FindDelegateForURL(self.path)
    if delegate:
      return delegate.send_head(self)
    return self.base_send_head()

  def base_send_head(self):
    return SimpleHTTPServer.SimpleHTTPRequestHandler.send_head(self)

  def do_GET(self):
    delegate = self._FindDelegateForURL(self.path)
    if delegate:
      return delegate.do_GET(self)
    return self.base_do_GET()

  def base_do_GET(self):
    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

  def do_POST(self):
    delegate = self._FindDelegateForURL(self.path)
    if delegate:
      return delegate.do_POST(self)
    return self.base_do_POST()

  def base_do_POST(self):
    pass


class LocalHTTPServer(object):
  """Class to start a local HTTP server as a child process."""

  def __init__(self, dirname, port):
    global serve_dir
    serve_dir = dirname
    parent_conn, child_conn = multiprocessing.Pipe()
    self.process = multiprocessing.Process(
        target=_HTTPServerProcess,
        args=(child_conn, serve_dir, port))
    self.process.start()
    if parent_conn.poll(10):  # wait 10 seconds
      self.port = parent_conn.recv()
    else:
      raise Exception('Unable to launch HTTP server.')

    self.conn = parent_conn

  def Shutdown(self):
    """Send a message to the child HTTP server process and wait for it to
        finish."""
    self.conn.send(False)
    self.process.join()

  def GetURL(self, rel_url):
    """Get the full url for a file on the local HTTP server.

    Args:
      rel_url: A URL fragment to convert to a full URL. For example,
          GetURL('foobar.baz') -> 'http://localhost:1234/foobar.baz'
    """
    return 'http://localhost:%d/%s' % (self.port, rel_url)


def _HTTPServerProcess(conn, dirname, port):
  """Run a local httpserver with the given port or an ephemeral port.

  This function assumes it is run as a child process using multiprocessing.

  Args:
    conn: A connection to the parent process. The child process sends
        the local port, and waits for a message from the parent to
        stop serving.
    dirname: The directory to serve. All files are accessible through
       http://localhost:<port>/path/to/filename.
    port: The port to serve on. If 0, an ephemeral port will be chosen.
  """
  import BaseHTTPServer

  try:
    os.chdir(dirname)
    httpd = BaseHTTPServer.HTTPServer(('', port), PluggableHTTPRequestHandler)
    conn.send(httpd.server_address[1])  # the chosen port number
    httpd.timeout = 0.5  # seconds
    running = True
    while running:
      # Flush output for MSVS Add-In.
      sys.stdout.flush()
      sys.stderr.flush()
      httpd.handle_request()
      if conn.poll():
        running = conn.recv()
  except KeyboardInterrupt:
    pass
  finally:
    conn.close()


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('-C', '--serve-dir',
      help='Serve files out of this directory.',
      dest='serve_dir', default=os.path.abspath('.'))
  parser.add_option('-p', '--port',
      help='Run server on this port.',
      dest='port', default=5103)
  parser.add_option('--no_dir_check',
      help='No check to ensure serving from safe directory.',
      dest='do_safe_check', action='store_false', default=True)
  options, args = parser.parse_args(args)
  if options.do_safe_check:
    SanityCheckDirectory(options.serve_dir)

  server = LocalHTTPServer(options.serve_dir, options.port)

  # Serve forever.
  print 'Serving %s on %s...' % (options.serve_dir, server.GetURL(''))
  try:
    while True:
      pass
  except KeyboardInterrupt:
    pass
  finally:
    print 'Stopping server.'
    server.Shutdown()

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
