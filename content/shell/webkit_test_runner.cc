// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner.h"

#include <algorithm>
#include <clocale>
#include <cmath>

#include "base/base64.h"
#include "base/debug/debugger.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/shell_messages.h"
#include "content/shell/shell_render_process_observer.h"
#include "content/shell/webkit_test_helpers.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBufferView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTask.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestInterfaces.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestProxy.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestRunner.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"

using WebKit::Platform;
using WebKit::WebArrayBufferView;
using WebKit::WebContextMenuData;
using WebKit::WebDevToolsAgent;
using WebKit::WebDeviceOrientation;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebVector;
using WebKit::WebView;
using WebTestRunner::WebPreferences;
using WebTestRunner::WebTask;
using WebTestRunner::WebTestInterfaces;

namespace content {

namespace {

void InvokeTaskHelper(void* context) {
  WebTask* task = reinterpret_cast<WebTask*>(context);
  task->run();
  delete task;
}

#if !defined(OS_MACOSX)
void MakeBitmapOpaque(SkBitmap* bitmap) {
  SkAutoLockPixels lock(*bitmap);
  DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
  for (int y = 0; y < bitmap->height(); ++y) {
    uint32_t* row = bitmap->getAddr32(0, y);
    for (int x = 0; x < bitmap->width(); ++x)
      row[x] |= 0xFF000000;  // Set alpha bits to 1.
  }
}
#endif

void CopyCanvasToBitmap(SkCanvas* canvas,  SkBitmap* snapshot) {
  SkDevice* device = skia::GetTopDevice(*canvas);
  const SkBitmap& bitmap = device->accessBitmap(false);
  bitmap.copyTo(snapshot, SkBitmap::kARGB_8888_Config);

#if !defined(OS_MACOSX)
  // Only the expected PNGs for Mac have a valid alpha channel.
  MakeBitmapOpaque(snapshot);
#endif

}

}  // namespace

// static
int WebKitTestRunner::window_count_ = 0;

WebKitTestRunner::WebKitTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<WebKitTestRunner>(render_view),
      is_main_window_(false) {
  ++window_count_;
}

WebKitTestRunner::~WebKitTestRunner() {
  --window_count_;
}

// WebTestDelegate  -----------------------------------------------------------

void WebKitTestRunner::clearEditCommand() {
  render_view()->ClearEditCommands();
}

void WebKitTestRunner::setEditCommand(const std::string& name,
                                      const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void WebKitTestRunner::setGamepadData(const WebGamepads& gamepads) {
  SetMockGamepads(gamepads);
}

void WebKitTestRunner::printMessage(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessage(routing_id(), message));
}

void WebKitTestRunner::postTask(WebTask* task) {
  Platform::current()->callOnMainThread(InvokeTaskHelper, task);
}

void WebKitTestRunner::postDelayedTask(WebTask* task, long long ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebTask::run, base::Owned(task)),
      base::TimeDelta::FromMilliseconds(ms));
}

WebString WebKitTestRunner::registerIsolatedFileSystem(
    const WebKit::WebVector<WebKit::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.push_back(webkit_base::WebStringToFilePath(absolute_filenames[i]));
  std::string filesystem_id;
  Send(new ShellViewHostMsg_RegisterIsolatedFileSystem(
      routing_id(), files, &filesystem_id));
  return WebString::fromUTF8(filesystem_id);
}

long long WebKitTestRunner::getCurrentTimeInMillisecond() {
    return base::TimeTicks::Now().ToInternalValue() /
        base::Time::kMicrosecondsPerMillisecond;
}

WebString WebKitTestRunner::getAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
#if defined(OS_WIN)
  base::FilePath path(UTF8ToWide(utf8_path));
#else
  base::FilePath path(base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
#endif
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(current_working_directory_.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return webkit_base::FilePathToWebString(path);
}

WebURL WebKitTestRunner::localFileToDataURL(const WebURL& file_url) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(file_url, &local_path))
    return WebURL();

  std::string contents;
  Send(new ShellViewHostMsg_ReadFileToString(
        routing_id(), local_path, &contents));

  std::string contents_base64;
  if (!base::Base64Encode(contents, &contents_base64))
    return WebURL();

  const char data_url_prefix[] = "data:text/css:charset=utf-8;base64,";
  return WebURL(GURL(data_url_prefix + contents_base64));
}

WebURL WebKitTestRunner::rewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  base::FilePath replace_path =
      ShellRenderProcessObserver::GetInstance()->webkit_source_dir().Append(
          FILE_PATH_LITERAL("LayoutTests/"));
#if defined(OS_WIN)
  std::string utf8_path = WideToUTF8(replace_path.value());
#else
  std::string utf8_path =
      WideToUTF8(base::SysNativeMBToWide(replace_path.value()));
#endif
  std::string new_url =
      std::string("file://") + utf8_path + utf8_url.substr(kPrefixLen);
  return WebURL(GURL(new_url));
}

WebPreferences* WebKitTestRunner::preferences() {
  return &prefs_;
}

void WebKitTestRunner::applyPreferences() {
  webkit_glue::WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportLayoutTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  Send(new ShellViewHostMsg_OverridePreferences(routing_id(), prefs));
}

std::string WebKitTestRunner::makeURLErrorDescription(
    const WebURLError& error) {
  std::string domain = error.domain.utf8();
  int code = error.reason;

  if (domain == net::kErrorDomain) {
    domain = "NSURLErrorDomain";
    switch (error.reason) {
    case net::ERR_ABORTED:
      code = -999;  // NSURLErrorCancelled
      break;
    case net::ERR_UNSAFE_PORT:
      // Our unsafe port checking happens at the network stack level, but we
      // make this translation here to match the behavior of stock WebKit.
      domain = "WebKitErrorDomain";
      code = 103;
      break;
    case net::ERR_ADDRESS_INVALID:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_NETWORK_ACCESS_DENIED:
      code = -1004;  // NSURLErrorCannotConnectToHost
      break;
    }
  } else {
    DLOG(WARNING) << "Unknown error domain";
  }

  return base::StringPrintf("<NSError domain %s, code %d, failing URL \"%s\">",
      domain.c_str(), code, error.unreachableURL.spec().data());
}

void WebKitTestRunner::setClientWindowRect(const WebRect& rect) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setClientWindowRect"));
}

void WebKitTestRunner::showDevTools() {
  Send(new ShellViewHostMsg_ShowDevTools(routing_id()));
}

void WebKitTestRunner::closeDevTools() {
  Send(new ShellViewHostMsg_CloseDevTools(routing_id()));
}

void WebKitTestRunner::evaluateInWebInspector(long call_id,
                                              const std::string& script) {
  WebDevToolsAgent* agent = render_view()->GetWebView()->devToolsAgent();
  if (agent)
    agent->evaluateInWebInspector(call_id, WebString::fromUTF8(script));
}

void WebKitTestRunner::clearAllDatabases() {
  Send(new ShellViewHostMsg_ClearAllDatabases(routing_id()));
}

void WebKitTestRunner::setDatabaseQuota(int quota) {
  Send(new ShellViewHostMsg_SetDatabaseQuota(routing_id(), quota));
}

void WebKitTestRunner::setDeviceScaleFactor(float factor) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setDeviceScaleFactor"));
}

void WebKitTestRunner::setFocus(bool focus) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setFocus"));
}

void WebKitTestRunner::setAcceptAllCookies(bool accept) {
  Send(new ShellViewHostMsg_AcceptAllCookies(routing_id(), accept));
}

std::string WebKitTestRunner::pathToLocalResource(const std::string& resource) {
#if defined(OS_WIN)
  if (resource.find("/tmp/") == 0) {
    // We want a temp file.
    GURL base_url = net::FilePathToFileURL(temp_path_);
    return base_url.Resolve(resource.substr(strlen("/tmp/"))).spec();
  }
#endif

  // Some layout tests use file://// which we resolve as a UNC path. Normalize
  // them to just file:///.
  std::string result = resource;
  while (StringToLowerASCII(result).find("file:////") == 0) {
    result = result.substr(0, strlen("file:///")) +
             result.substr(strlen("file:////"));
  }
  return rewriteLayoutTestsURL(result).spec();
}

void WebKitTestRunner::setLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
}

void WebKitTestRunner::setDeviceOrientation(WebDeviceOrientation& orientation) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setDeviceOrientation"));
}

void WebKitTestRunner::didAcquirePointerLock() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "didAcquirePointerLock"));
}

void WebKitTestRunner::didNotAcquirePointerLock() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "didNotAcquirePointerLock"));
}

void WebKitTestRunner::didLosePointerLock() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "didLosePointerLock"));
}

void WebKitTestRunner::setPointerLockWillRespondAsynchronously() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(),
      "WebKitTestRunner",
      "setPointerLockWillRespondAsynchronously"));
}

void WebKitTestRunner::setPointerLockWillFailSynchronously() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setPointerLockWillFailSynchronously"));
}

int WebKitTestRunner::numberOfPendingGeolocationPermissionRequests() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(),
      "WebKitTestRunner",
      "numberOfPendingGeolocationPermissionRequests"));
  return 0;
}

void WebKitTestRunner::setGeolocationPermission(bool allowed) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setGeolocationPermission"));
}

void WebKitTestRunner::setMockGeolocationPosition(double latitude,
                                                  double longitude,
                                                  double precision) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setMockGeolocationPosition"));
}

void WebKitTestRunner::setMockGeolocationPositionUnavailableError(
    const std::string& message) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(),
      "WebKitTestRunner",
      "setMockGeolocationPositionUnavailableError"));
}

void WebKitTestRunner::addMockSpeechInputResult(const std::string& result,
                                                double confidence,
                                                const std::string& language) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "addMockSpeechInputResult"));
}

void WebKitTestRunner::setMockSpeechInputDumpRect(bool dump_rect) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setMockSpeechInputDumpRect"));
}

void WebKitTestRunner::addMockSpeechRecognitionResult(
    const std::string& transcript,
    double confidence) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "addMockSpeechRecognitionResult"));
}

void WebKitTestRunner::setMockSpeechRecognitionError(
    const std::string& error,
    const std::string& message) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "setMockSpeechRecognitionError"));
}

bool WebKitTestRunner::wasMockSpeechRecognitionAborted() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "wasMockSpeechRecognitionAborted"));
  return false;
}

void WebKitTestRunner::testFinished() {
  if (!is_main_window_)
    return;
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->setTestIsRunning(false);
  CaptureDump();
}

void WebKitTestRunner::testTimedOut() {
  if (!is_main_window_)
    return;
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->setTestIsRunning(false);
  Send(new ShellViewHostMsg_TestFinished(routing_id(), true));
}

bool WebKitTestRunner::isBeingDebugged() {
  return base::debug::BeingDebugged();
}

int WebKitTestRunner::layoutTestTimeout() {
  return layout_test_timeout_;
}

void WebKitTestRunner::closeRemainingWindows() {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "closeRemainingWindows"));
}

int WebKitTestRunner::navigationEntryCount() {
  return 0;
}

int WebKitTestRunner::windowCount() {
  return window_count_;
}

void WebKitTestRunner::goToOffset(int offset) {
  Send(new ShellViewHostMsg_GoToOffset(routing_id(), offset));
}

void WebKitTestRunner::reload() {
  Send(new ShellViewHostMsg_Reload(routing_id()));
}

void WebKitTestRunner::loadURLForFrame(const WebURL& url,
                             const std::string& frame_name) {
  Send(new ShellViewHostMsg_LoadURLForFrame(
      routing_id(), url, frame_name));
}

bool WebKitTestRunner::allowExternalPages() {
  return allow_external_pages_;
}

void WebKitTestRunner::captureHistoryForWindow(
    size_t windowIndex,
    WebVector<WebKit::WebHistoryItem>* history,
    size_t* currentEntryIndex) {
  Send(new ShellViewHostMsg_NotImplemented(
      routing_id(), "WebKitTestRunner", "captureHistoryForWindow"));
}

// RenderViewObserver  --------------------------------------------------------

void WebKitTestRunner::DidClearWindowObject(WebFrame* frame) {
  ShellRenderProcessObserver::GetInstance()->BindTestRunnersToWindow(frame);
}

bool WebKitTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetTestConfiguration,
                        OnSetTestConfiguration)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Public methods - -----------------------------------------------------------

void WebKitTestRunner::Reset() {
  prefs_.reset();
  webkit_glue::WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportLayoutTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  enable_pixel_dumping_ = true;
  layout_test_timeout_ = 30 * 1000;
  allow_external_pages_ = false;
  expected_pixel_hash_ = std::string();
}

// Private methods  -----------------------------------------------------------

void WebKitTestRunner::CaptureDump() {
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();

  if (interfaces->testRunner()->shouldDumpAsAudio()) {
    const WebArrayBufferView* audio_data =
        interfaces->testRunner()->audioData();
    std::vector<unsigned char> vector_data(
        static_cast<const unsigned char*>(audio_data->baseAddress()),
        static_cast<const unsigned char*>(audio_data->baseAddress()) +
            audio_data->byteLength());
    Send(new ShellViewHostMsg_AudioDump(routing_id(), vector_data));
    Send(new ShellViewHostMsg_TestFinished(routing_id(), false));
    return;
  }

  Send(
      new ShellViewHostMsg_TextDump(routing_id(), proxy()->captureTree(false)));

  if (interfaces->testRunner()->shouldGeneratePixelResults()) {
    SkBitmap snapshot;
    CopyCanvasToBitmap(proxy()->capturePixels(), &snapshot);

    SkAutoLockPixels snapshot_lock(snapshot);
    base::MD5Digest digest;
#if defined(OS_ANDROID)
    // On Android, pixel layout is RGBA, however, other Chrome platforms use
    // BGRA.
    const uint8_t* raw_pixels =
        reinterpret_cast<const uint8_t*>(snapshot.getPixels());
    size_t snapshot_size = snapshot.getSize();
    scoped_array<uint8_t> reordered_pixels(new uint8_t[snapshot_size]);
    for (size_t i = 0; i < snapshot_size; i += 4) {
      reordered_pixels[i] = raw_pixels[i + 2];
      reordered_pixels[i + 1] = raw_pixels[i + 1];
      reordered_pixels[i + 2] = raw_pixels[i];
      reordered_pixels[i + 3] = raw_pixels[i + 3];
    }
    base::MD5Sum(reordered_pixels.get(), snapshot_size, &digest);
#else
    base::MD5Sum(snapshot.getPixels(), snapshot.getSize(), &digest);
#endif
    std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

    if (actual_pixel_hash == expected_pixel_hash_) {
      SkBitmap empty_image;
      Send(new ShellViewHostMsg_ImageDump(
          routing_id(), actual_pixel_hash, empty_image));
    } else {
      Send(new ShellViewHostMsg_ImageDump(
          routing_id(), actual_pixel_hash, snapshot));
    }
  }

  Send(new ShellViewHostMsg_TestFinished(routing_id(), false));
}

void WebKitTestRunner::OnSetTestConfiguration(
    const ShellViewMsg_SetTestConfiguration_Params& params) {
  current_working_directory_ = params.current_working_directory;
  temp_path_ = params.temp_path;
  enable_pixel_dumping_ = params.enable_pixel_dumping;
  layout_test_timeout_ = params.layout_test_timeout;
  allow_external_pages_ = params.allow_external_pages;
  expected_pixel_hash_ = params.expected_pixel_hash;
  is_main_window_ = true;

  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->setTestIsRunning(true);
  interfaces->configureForTestWithURL(params.test_url, enable_pixel_dumping_);
}

}  // namespace content
