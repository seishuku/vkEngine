#ifndef __SERVER_NETWORK_H__
#define __SERVER_NETWORK_H__

#include "../entitylist.h"
#include "../utils/id.h"
#include "net_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#define SERVER_EVENT_RETRY		0.1
#define SERVER_CLIENT_TIMEOUT	10.0
#define SERVER_STATUS_RATE		(1.0f/20.0f)

typedef struct
{
	vec3 lastSentVelocity;
	vec3 lastSentAngularVelocity;
	vec4 lastSentOrientation;
	bool everSent;
} EntityDeltaState_t;

typedef struct
{
	bool active;
	uint32_t id;
	uint32_t address;
	uint16_t port;
	double lastSeen;
	double lastStatus;
	uint32_t lastAckedSeq;
	uint32_t playerEntityID;
	NetEventQueue_t eventQueue;

	EntityDeltaState_t deltaState[ID_MAX];

	vec3 position;
	vec3 velocity;
	vec4 orientation;
} ServerClient_t;

bool ServerNetwork_Init(uint16_t port, EntityList_t *list, uint32_t seed);
void ServerNetwork_Destroy(void);
void ServerNetwork_Update(double now);
void ServerNetwork_BroadcastEvent(const NetEvent_t *ev);
void ServerNetwork_SendEvent(uint32_t clientID, const NetEvent_t *ev);
void ServerNetwork_SendPlayerImpulse(uint32_t playerEntityID, vec3 position, vec3 velocity);

#endif
