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
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;
typedef float  f32;
typedef double f64;

#pragma comment (lib, "Ws2_32.lib")

void set_non_blocking(SOCKET socket) {
  DWORD non_blocking = 1;
  ioctlsocket(socket, FIONBIO, &non_blocking);
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
  // set_non_blocking(server_socket);

  printf("Server is listening on http://localhost:%s\n", DEFAULT_PORT);

  #define MAX_CLIENTS 100
  SOCKET client_sockets[MAX_CLIENTS] = {0};
  u8 client_index = 0;

  while(true) {
    client_sockets[client_index] = accept(server_socket, NULL, NULL);
    printf("After accept\n");
    if(client_sockets[client_index] != INVALID_SOCKET) {
      // set_non_blocking(client_sockets[client_index]);
      printf("Client connected, ID %d\n", client_index);

      // char receive_buffer[MESSAGE_BUFFER_SIZE];
      // s32 recv_result = recv(client_sockets[i], receive_buffer, MESSAGE_BUFFER_SIZE, 0);
      // if(recv_result <= 0) continue;

      char response[] = "HTTP/1.0 200 OK\r\n"
        "Server: actor-webserver-c\r\n"
        "Content-type: text/html\r\n\r\n"
        "<p>Hello Sailor</p>";

      s32 send_result = send(client_sockets[client_index], response, (int)strlen(response), 0);
      if(send_result == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        return 1;
      }
      client_index++;
    }

    // closesocket(client_sockets[client_index-1]);
  }

  return 0;
}