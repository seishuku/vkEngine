#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdint.h>
#include <stdbool.h>

typedef int Socket_t;

#define NETWORK_ADDRESS(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)

bool Network_Init(void);
void Network_Destroy(void);
Socket_t Network_CreateSocket(void);
bool Network_SocketBind(Socket_t sock, uint32_t address, uint16_t port);
bool Network_SocketSend(Socket_t sock, uint8_t *packet, uint32_t packet_size, uint32_t address, uint16_t port);
int32_t Network_SocketReceive(Socket_t sock, uint8_t *buffer, uint32_t buffer_size, uint32_t *address, uint16_t *port);
bool Network_SocketClose(Socket_t sock);

#endif
