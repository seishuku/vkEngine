#ifndef __CLIENT_NETWORK_H__
#define __CLIENT_NETWORK_H__

#include <stdint.h>
#include <stdbool.h>
#include "../entitylist.h"
#include "../camera/camera.h"
#include "net_protocol.h"

#define CLIENT_STATUS_RATE      (1.0/30.0)  // STATUS send rate (seconds)
#define CLIENT_EVENT_RETRY      0.1         // Client event retry interval (seconds)

bool ClientNetwork_Init(uint32_t address, uint16_t port, Camera_t *camera);
void ClientNetwork_Destroy(void);
void ClientNetwork_Update(double now, float dt);
void ClientNetwork_SendEvent(const NetEvent_t *ev);
bool ClientNetwork_IsConnected(void);

extern NetPlayerState_t netPlayers[NET_MAX_CLIENTS];
extern uint32_t         netPlayerCount;
extern uint32_t         localClientID;

#endif
