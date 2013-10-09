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
  SocketTest() : sock1(-1), sock2(-1) {}

  void TearDown() {
    if (sock1 != -1)
      EXPECT_EQ(0, close(sock1));
    if (sock2 != -1)
      EXPECT_EQ(0, close(sock2));
  }

  int Bind(int fd, uint32_t ip, uint16_t port) {
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    IP4ToSockAddr(ip, port, &addr);
    int err = bind(fd, (sockaddr*)&addr, addrlen);

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

class SocketTestTCP : public SocketTest {
 public:
  SocketTestTCP() {}

  void SetUp() {
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);

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

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GT(sock_, -1);
  }

  void TearDown() {
    // Stop the echo server and the background thread it runs on
    loop_.PostQuit(true);
    pthread_join(server_thread_, NULL);
    ASSERT_EQ(0, close(sock_));
  }

  static void ServerLog(const char* msg) {
    // Uncomment to see logs of echo server on stdout
    //printf("server: %s\n", msg);
  }

 protected:
  int sock_;
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

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);

  ASSERT_EQ(0, connect(sock_, (sockaddr*) &addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno) << "\n";

  // Send two different messages to the echo server and verify the
  // response matches.
  strcpy(outbuf, "hello");
  memset(inbuf, 0, sizeof(inbuf));
  ASSERT_EQ(sizeof(outbuf), write(sock_, outbuf, sizeof(outbuf)));
  ASSERT_EQ(sizeof(outbuf), read(sock_, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  strcpy(outbuf, "world");
  memset(inbuf, 0, sizeof(inbuf));
  ASSERT_EQ(sizeof(outbuf), write(sock_, outbuf, sizeof(outbuf)));
  ASSERT_EQ(sizeof(outbuf), read(sock_, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

TEST_F(SocketTest, Getsockopt) {
  sock1 = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock1, -1);
  int socket_error = 99;
  socklen_t len = sizeof(socket_error);

  // Test for valid option (SO_ERROR) which should be 0 when a socket
  // is first created.
  ASSERT_EQ(0, getsockopt(sock1, SOL_SOCKET, SO_ERROR, &socket_error, &len));
  ASSERT_EQ(0, socket_error);
  ASSERT_EQ(sizeof(socket_error), len);

  int reuse = 0;
  len = sizeof(reuse);
  ASSERT_EQ(0, getsockopt(sock1, SOL_SOCKET, SO_REUSEADDR, &reuse, &len));
  ASSERT_EQ(1, reuse);

  // Test for an invalid option (-1)
  ASSERT_EQ(-1, getsockopt(sock1, SOL_SOCKET, -1, &socket_error, &len));
  ASSERT_EQ(ENOPROTOOPT, errno);
}

TEST_F(SocketTest, Setsockopt) {
  sock1 = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock1, -1);

  // It should not be possible to set SO_ERROR using setsockopt.
  int socket_error = 10;
  socklen_t len = sizeof(socket_error);
  ASSERT_EQ(-1, setsockopt(sock1, SOL_SOCKET, SO_ERROR, &socket_error, len));
  ASSERT_EQ(ENOPROTOOPT, errno);

  int reuse = 1;
  len = sizeof(reuse);
  ASSERT_EQ(0, setsockopt(sock1, SOL_SOCKET, SO_REUSEADDR, &reuse, len));
}

TEST_F(SocketTestUDP, Listen) {
  EXPECT_EQ(-1, listen(sock1, 10));
  EXPECT_EQ(errno, ENOTSUP);
}

TEST_F(SocketTestTCP, Listen) {
  // Accept should fail when socket not listening
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int server_sock = sock1;

  // Accept before listen should fail
  ASSERT_EQ(-1, accept(server_sock, (sockaddr*)&addr, &addrlen));

  // Listen should fail on unbound socket
  ASSERT_EQ(-1, listen(server_sock, 10));

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // Connect to listening socket
  int client_sock = sock2;
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  addrlen = sizeof(addr);
  ASSERT_EQ(0, connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno) << "\n";

  ASSERT_EQ(5, send(client_sock, "hello", 5, 0));

  // Pass in addrlen that is larger than our actual address to make
  // sure that it is correctly set back to sizeof(sockaddr_in)
  addrlen = sizeof(addr) + 10;
  int new_socket = accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_GT(new_socket, -1)
    << "accept failed with " << errno << ": " << strerror(errno) << "\n";

  // Verify addr and addrlen were set correctly
  ASSERT_EQ(addrlen, sizeof(sockaddr_in));
  sockaddr_in client_addr;
  ASSERT_EQ(0, getsockname(client_sock, (sockaddr*)&client_addr, &addrlen));
  ASSERT_EQ(client_addr.sin_family, addr.sin_family);
  ASSERT_EQ(client_addr.sin_port, addr.sin_port);
  ASSERT_EQ(client_addr.sin_addr.s_addr, addr.sin_addr.s_addr);

  char inbuf[512];
  ASSERT_EQ(5, recv(new_socket, inbuf, 5, 0));
}

#endif  // PROVIDES_SOCKET_API
