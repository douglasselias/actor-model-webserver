#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct addrinfo addrinfo;

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;
typedef float    f32;
typedef double   f64;

#pragma comment (lib, "Ws2_32.lib")

void set_non_blocking(SOCKET socket) {
  DWORD non_blocking = 1;
  ioctlsocket(socket, FIONBIO, &non_blocking);
}

u32 count_threads() {
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
}

#define MAX_CLIENTS 2000
SOCKET client_sockets[MAX_CLIENTS] = {0};
u64 client_index = 0;

typedef struct {
  s32 value;
  u64 client_index;
} Message;

#define MAILBOX_CAPACITY 1000
typedef struct {
  Message mailbox[MAILBOX_CAPACITY];
  s64 total_messages;
  u64 read_index;
  volatile s64 write_index;
} ThreadArgs; /// A.K.A Actor

typedef struct {
  HANDLE handle;
  ThreadArgs* thread_args;
} Thread;

DWORD thread_proc(void* thread_args) {
  ThreadArgs* actor = (ThreadArgs*)thread_args;
  u64 thread_id = GetCurrentThreadId();
  printf("Thread started (ID: %llu)\n", thread_id);

  while(true) {
    while(actor->total_messages > 0) {
      Message message = actor->mailbox[actor->read_index];
      __try {
        try_again:
        s32 a = 100;
        s32 result = a / message.value;
        printf("Result: %d, for client %lld\n", result, message.client_index);
        actor->read_index = (actor->read_index + 1) % MAILBOX_CAPACITY;
        actor->total_messages--;

        #define BUFFER_SIZE 200
        char buffer[BUFFER_SIZE] = {0};
        sprintf(buffer, "HTTP/1.0 200 OK\r\n"
        "Server: actor-webserver-c\r\n"
        "Content-type: text/html\r\n\r\n"
        "<p>%d</p>", result);

        s32 send_result = send(client_sockets[message.client_index], buffer, (s32)strlen(buffer), 0);
        if (send_result == SOCKET_ERROR) {
          s32 error = WSAGetLastError();
          if(error != WSAEWOULDBLOCK) {
            printf("send failed with error: %d\n", error);
            closesocket(client_sockets[message.client_index]);
            return 1;
          }
        }
        closesocket(client_sockets[message.client_index]);
      }
      __except(EXCEPTION_EXECUTE_HANDLER) {
        printf("Error detected on thread (ID: %llu)\nRestarting\n", thread_id);
        message.value = 1;
        goto try_again;
      }
    }
  }

  return 0;
}

void send_message(ThreadArgs* actor, Message message, u64 message_size) {
  u64 write_index = InterlockedCompareExchange64(&actor->write_index, (actor->write_index + 1) % MAILBOX_CAPACITY, actor->write_index);
  memcpy(&(actor->mailbox[write_index]), &message, message_size);
  InterlockedIncrement64(&(actor->total_messages));
}

s32 main() {
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2,2), &wsa_data);

  addrinfo hints = {0};
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags    = AI_PASSIVE;

  addrinfo *address_info = NULL;
  #define DEFAULT_PORT "27015"
  getaddrinfo(NULL, DEFAULT_PORT, &hints, &address_info);

  SOCKET server_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
  bind(server_socket, address_info->ai_addr, (s32)address_info->ai_addrlen);
  freeaddrinfo(address_info);
  listen(server_socket, SOMAXCONN);
  set_non_blocking(server_socket);

  printf("Server is listening on http://localhost:%s\n", DEFAULT_PORT);

  /// Actors setup ///
  s32 total_threads = count_threads() - 2;
  Thread* threads = calloc(sizeof(Thread), total_threads);
  for(s32 i = 0; i < total_threads; i++) {
    ThreadArgs* thread_args = calloc(sizeof(ThreadArgs), 1);
    threads[i].thread_args = thread_args;
    threads[i].handle = CreateThread(NULL, 0, thread_proc, thread_args, 0, NULL);
  }

  while(true) {
    if(client_index + 1 < MAX_CLIENTS) {
      client_sockets[client_index] = accept(server_socket, NULL, NULL);
    }

    if(client_sockets[client_index] != INVALID_SOCKET) {
      set_non_blocking(client_sockets[client_index]);
      printf("Client connected, ID %lld\n", client_index);
      client_index++;
    } else {
      s32 error = WSAGetLastError();
      if(error != WSAEWOULDBLOCK) {
        // printf("accept failed with error: %d\n", WSAGetLastError());
        continue;
      }
    }

    for(u32 i = 0; i < client_index; i++) {
      #define MESSAGE_BUFFER_SIZE 1000
      char receive_buffer[MESSAGE_BUFFER_SIZE];
      s32 recv_result = recv(client_sockets[i], receive_buffer, MESSAGE_BUFFER_SIZE, 0);
      if(recv_result <= 0) continue;
      // if(recv_result < 0) continue;
      // else if(recv_result == 0) closesocket(client_sockets[i]);
      if (recv_result == 0) {
        closesocket(client_sockets[i]);
        // client_sockets[i] = INVALID_SOCKET;
      }
      printf("Received: %c\n", receive_buffer[5]);
  
      s32 thread_index_with_fewer_messages = 0;
      s64 min_messages = LLONG_MAX;
      for(s32 j = 0; j < total_threads; j++) {
        s64 total_messages = threads[j].thread_args->total_messages;
        if(total_messages < min_messages) {
          thread_index_with_fewer_messages = j;
          min_messages = total_messages;
        }
      }
  
      ThreadArgs* actor = threads[thread_index_with_fewer_messages].thread_args;
      s32 value = receive_buffer[5] - 48;
      Message message = {value, .client_index = i};
      send_message(actor, message, sizeof(message));
    }
  }

  return 0;
}