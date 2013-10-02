// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "echo_server.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi_simple/ps.h"

#ifdef PROVIDES_SOCKET_API

using namespace nacl_io;
using namespace sdk_util;

#define LOCAL_HOST 0x7F000001
#define PORT1 4006
#define PORT2 4007
#define ANY_PORT 0

namespace {

void IP4ToSockAddr(uint32_t ip, uint16_t port, struct sockaddr_in* addr) {
  memset(addr, 0, sizeof(*addr));

  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(ip);
}

class SocketTest : public ::testing::Test {
 public:
  SocketTest() : sock1(0), sock2(0) {}

  void TearDown() {
    EXPECT_EQ(0, close(sock1));
    EXPECT_EQ(0, close(sock2));
  }

  int Bind(int fd, uint32_t ip, uint16_t port) {
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    IP4ToSockAddr(ip, port, &addr);
    int err = bind(fd, (sockaddr*) &addr, addrlen);

    if (err == -1)
      return errno;
    return 0;
  }

 public:
  int sock1;
  int sock2;
};

class SocketTestUDP : public SocketTest {
 public:
  SocketTestUDP() {}

  void SetUp() {
    sock1 = socket(AF_INET, SOCK_DGRAM, 0);
    sock2 = socket(AF_INET, SOCK_DGRAM, 0);

    EXPECT_GT(sock1, -1);
    EXPECT_GT(sock2, -1);
  }
};

class SocketTestWithServer : public ::testing::Test {
 public:
  SocketTestWithServer() : instance_(PSGetInstanceId()) {
    pthread_mutex_init(&ready_lock_, NULL);
    pthread_cond_init(&ready_cond_, NULL);
  }

  void ServerThreadMain() {
    loop_.AttachToCurrentThread();
    pp::Instance instance(PSGetInstanceId());
    EchoServer server(&instance, PORT1, ServerLog, &ready_cond_, &ready_lock_);
    loop_.Run();
  }

  static void* ServerThreadMainStatic(void* arg) {
    SocketTestWithServer* test = (SocketTestWithServer*)arg;
    test->ServerThreadMain();
    return NULL;
  }

  void SetUp() {
    loop_ = pp::MessageLoop(&instance_);
    pthread_mutex_lock(&ready_lock_);

    // Start an echo server on a background thread.
    pthread_create(&server_thread_, NULL, ServerThreadMainStatic, this);

    // Wait for thread to signal that it is ready to accept connections.
    pthread_cond_wait(&ready_cond_, &ready_lock_);
    pthread_mutex_unlock(&ready_lock_);
  }

  void TearDown() {
    // Stop the echo server and the background thread it runs on
    loop_.PostQuit(true);
    pthread_join(server_thread_, NULL);
  }

  static void ServerLog(const char* msg) {
    // Uncomment to see logs of echo server on stdout
    //printf("server: %s\n", msg);
  }

 protected:
  pp::MessageLoop loop_;
  pp::Instance instance_;
  pthread_cond_t ready_cond_;
  pthread_mutex_t ready_lock_;
  pthread_t server_thread_;
};

}  // namespace

TEST(SocketTestSimple, Socket) {
  EXPECT_EQ(-1, socket(AF_UNIX, SOCK_STREAM, 0));
  EXPECT_EQ(errno, EAFNOSUPPORT);
  EXPECT_EQ(-1, socket(AF_INET, SOCK_RAW, 0));
  EXPECT_EQ(errno, EPROTONOSUPPORT);

  int sock1 = socket(AF_INET, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock1);

  int sock2 = socket(AF_INET6, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock2);

  int sock3 = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock3);

  int sock4 = socket(AF_INET6, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock4);

  close(sock1);
  close(sock2);
  close(sock3);
  close(sock4);
}

TEST_F(SocketTestUDP, Bind) {
  // Bind away.
  EXPECT_EQ(0, Bind(sock1, LOCAL_HOST, PORT1));

  // Invalid to rebind a socket.
  EXPECT_EQ(EINVAL, Bind(sock1, LOCAL_HOST, PORT1));

  // Addr in use.
  EXPECT_EQ(EADDRINUSE, Bind(sock2, LOCAL_HOST, PORT1));

  // Bind with a wildcard.
  EXPECT_EQ(0, Bind(sock2, LOCAL_HOST, ANY_PORT));

  // Invalid to rebind after wildcard
  EXPECT_EQ(EINVAL, Bind(sock2, LOCAL_HOST, PORT1));

}

TEST_F(SocketTestUDP, SendRcv) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));
  memset(inbuf, 0, sizeof(inbuf));

  EXPECT_EQ(0, Bind(sock1, LOCAL_HOST, PORT1));
  EXPECT_EQ(0, Bind(sock2, LOCAL_HOST, PORT2));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr);

  int len1 =
     sendto(sock1, outbuf, sizeof(outbuf), 0, (sockaddr *) &addr, addrlen);
  EXPECT_EQ(sizeof(outbuf), len1);

  // Ensure the buffers are different
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  memset(&addr, 0, sizeof(addr));

  // Try to receive the previously sent packet
  int len2 =
    recvfrom(sock2, inbuf, sizeof(inbuf), 0, (sockaddr *) &addr, &addrlen);
  EXPECT_EQ(sizeof(outbuf), len2);
  EXPECT_EQ(sizeof(sockaddr_in), addrlen);
  EXPECT_EQ(PORT1, htons(addr.sin_port));

  // Now they should be the same
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

const size_t kQueueSize = 65536 * 8;
TEST_F(SocketTestUDP, FullFifo) {
  char outbuf[16 * 1024];

  EXPECT_EQ(0, Bind(sock1, LOCAL_HOST, PORT1));
  EXPECT_EQ(0, Bind(sock2, LOCAL_HOST, PORT2));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr);

  size_t total = 0;
  while (total < kQueueSize * 8) {
     int len = sendto(sock1, outbuf, sizeof(outbuf), MSG_DONTWAIT,
                      (sockaddr *) &addr, addrlen);

     if (len <= 0) {
       EXPECT_EQ(-1, len);
       EXPECT_EQ(EWOULDBLOCK, errno);
       break;
     }

     if (len >= 0) {
       EXPECT_EQ(sizeof(outbuf), len);
       total += len;
     }

  }
  EXPECT_GT(total, kQueueSize - 1);
  EXPECT_LT(total, kQueueSize * 8);
}

TEST_F(SocketTestWithServer, TCPConnect) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock, -1);

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);

  EXPECT_EQ(0, connect(sock, (sockaddr*) &addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno) << "\n";

  // Send two different messages to the echo server and verify the
  // response matches.
  strcpy(outbuf, "hello");
  memset(inbuf, 0, sizeof(inbuf));
  EXPECT_EQ(sizeof(outbuf), write(sock, outbuf, sizeof(outbuf)));
  EXPECT_EQ(sizeof(outbuf), read(sock, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  strcpy(outbuf, "world");
  memset(inbuf, 0, sizeof(inbuf));
  EXPECT_EQ(sizeof(outbuf), write(sock, outbuf, sizeof(outbuf)));
  EXPECT_EQ(sizeof(outbuf), read(sock, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  ASSERT_EQ(0, close(sock));
}

TEST_F(SocketTest, Setsockopt) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock, -1);
  int socket_error = 99;
  socklen_t len = sizeof(socket_error);

  // Test for valid option (SO_ERROR) which should be 0 when a socket
  // is first created.
  ASSERT_EQ(0, getsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error, &len));
  ASSERT_EQ(0, socket_error);
  ASSERT_EQ(sizeof(socket_error), len);

  // Test for an invalid option (-1)
  ASSERT_EQ(-1, getsockopt(sock, SOL_SOCKET, -1, &socket_error, &len));
  ASSERT_EQ(ENOPROTOOPT, errno);
}

TEST_F(SocketTest, Getsockopt) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock, -1);

  // It should not be possible to set SO_ERROR using setsockopt.
  int socket_error = 10;
  socklen_t len = sizeof(socket_error);
  ASSERT_EQ(-1, setsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error, len));
  ASSERT_EQ(ENOPROTOOPT, errno);
}

#endif  // PROVIDES_SOCKET_API
