#ifndef __CLIENT_NETWORK_H__
#define __CLIENT_NETWORK_H__

#include <stdint.h>
#include <stdbool.h>
#include "network/network.h"
#include "camera/camera.h"

#define MAX_CLIENTS 16

extern uint32_t clientID;
extern Socket_t clientSocket;
extern uint32_t connectedClients;
extern Camera_t netCameras[MAX_CLIENTS];

bool ClientNetwork_Init(uint32_t address);
void ClientNetwork_Destroy(void);
void ClientNetwork_SendStatus(void);

#endif
