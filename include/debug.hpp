#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <net/net.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEBUG

void debugPrintf(const char* fmt, ...){};
void debugInit(){};
void debugStop(){};

#else

static int SocketFD;
#define DEBUG_IP "192.168.1.101"
#define DEBUG_PORT 18194

void debugPrintf(const char* fmt, ...)
{
  char buffer[0x800];
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, arg);
  va_end(arg);

  netSend(SocketFD, buffer, strlen(buffer), 0);

  usleep(5000);
}

void debugInit()
{
  netInitialize();
  struct sockaddr_in stSockAddr;
  SocketFD = netSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  memset(&stSockAddr, 0, sizeof stSockAddr);

  stSockAddr.sin_family = AF_INET;
  stSockAddr.sin_port = htons(DEBUG_PORT);
  inet_pton(AF_INET, DEBUG_IP, &stSockAddr.sin_addr);

  netConnect(SocketFD, (struct sockaddr *)&stSockAddr, sizeof stSockAddr);
	
  debugPrintf("network debug module initialized\n") ;
  debugPrintf("ready to have a lot of fun\n") ;
}

void debugStop(){
  netDeinitialize();
}

#endif

#ifdef __cplusplus
}
#endif
