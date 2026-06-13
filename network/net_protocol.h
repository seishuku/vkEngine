#ifndef __NET_PROTOCOL_H__
#define __NET_PROTOCOL_H__

#include "../entitylist.h"
#include "../math/math.h"
#include "../utils/id.h"
#include "../utils/serial.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NETMAGIC_CONNECT	('C'|'o'<<8|'n'<<16|'n'<<24) // "Conn"
#define NETMAGIC_DISCONNECT	('D'|'i'<<8|'s'<<16|'C'<<24) // "DisC"
#define NETMAGIC_STATUS		('S'|'t'<<8|'a'<<16|'t'<<24) // "Stat"
#define NETMAGIC_SNAPSHOT	('S'|'n'<<8|'a'<<16|'p'<<24) // "Snap"
#define NETMAGIC_UPDATE		('U'|'p'<<8|'d'<<16|'t'<<24) // "Updt"
#define NETMAGIC_EVENT		('E'|'v'<<8|'n'<<16|'t'<<24) // "Evnt"
#define NETMAGIC_ACK		('A'|'c'<<8|'k'<<16|'!'<<24) // "Ack!"

#define NET_MAX_CLIENTS		16
#define NET_UPDATE_BATCH	64 // Max updates per UPDATE packet
#define NET_SNAPSHOT_BATCH	64 // Max entries per SNAPSHOT packet
#define NET_INVALID_ID		UINT32_MAX

#define NET_VELOCITY_EPSILON 0.001f
#define NET_ORIENTATION_EPSILON 0.9999f

typedef enum
{
	NETEVENT_SPAWN,
	NETEVENT_DESTROY,
	NETEVENT_SPLIT,
	NETEVENT_IMPULSE,
} NetEventType_e;

typedef struct
{
	uint32_t id;
	EntityObjectType_e objectType;
	uint32_t variant;
	vec3 position;
	vec3 velocity;
	vec4 orientation;
	float radius;
} NetEventSpawn_t;

typedef struct
{
	uint32_t id;
} NetEventDestroy_t;

typedef struct
{
	uint32_t parentID;
	uint32_t rngSnapshot;
	vec3 contactPoint;
	vec3 contactNormal;
	float impactSpeed;
} NetEventSplit_t;

typedef struct
{
	vec3 velocity;
	vec3 position;
} NetEventImpulse_t;

typedef struct
{
	uint32_t seq;
	NetEventType_e type;

	union
	{
		NetEventSpawn_t spawn;
		NetEventDestroy_t destroy;
		NetEventSplit_t split;
		NetEventImpulse_t impulse;
	};
} NetEvent_t;

typedef struct
{
	uint32_t id;
	vec3 position;
	vec3 velocity;
	vec4 orientation;
} NetEntityUpdate_t;

typedef struct
{
	uint32_t id;
	EntityObjectType_e objectType;
	uint32_t variant;
	vec3 position;
	vec3 velocity;
	vec4 orientation;
	float radius;
} NetSnapshotEntry_t;

typedef struct
{
	uint32_t clientID;
	uint32_t serverTick;
	vec3 position;
	vec3 velocity;
	vec4 orientation;
} NetPlayerState_t;

#define NET_EVENT_QUEUE_SIZE 256

typedef struct
{
	NetEvent_t events[NET_EVENT_QUEUE_SIZE];
	double sentTime[NET_EVENT_QUEUE_SIZE];
	uint32_t head;
	uint32_t tail;
	uint32_t nextSeq;
} NetEventQueue_t;

static inline size_t NetEvent_Serialize(uint8_t **buf, const NetEvent_t *ev)
{
	uint8_t *start=*buf;

	Serialize_uint32(buf, NETMAGIC_EVENT);
	Serialize_uint32(buf, ev->seq);
	Serialize_uint32(buf, (uint32_t)ev->type);

	switch(ev->type)
	{
		case NETEVENT_SPAWN:
			Serialize_uint32(buf, ev->spawn.id);
			Serialize_uint32(buf, (uint32_t)ev->spawn.objectType);
			Serialize_uint32(buf, ev->spawn.variant);
			Serialize_vec3(buf, ev->spawn.position);
			Serialize_vec3(buf, ev->spawn.velocity);
			Serialize_vec4(buf, ev->spawn.orientation);
			Serialize_float(buf, ev->spawn.radius);
			break;

		case NETEVENT_DESTROY:
			Serialize_uint32(buf, ev->destroy.id);
			break;

		case NETEVENT_SPLIT:
			Serialize_uint32(buf, ev->split.parentID);
			Serialize_uint32(buf, ev->split.rngSnapshot);
			Serialize_vec3(buf, ev->split.contactPoint);
			Serialize_vec3(buf, ev->split.contactNormal);
			Serialize_float(buf, ev->split.impactSpeed);
			break;

		case NETEVENT_IMPULSE:
			Serialize_vec3(buf, ev->impulse.velocity);
			Serialize_vec3(buf, ev->impulse.position);
			break;
	}

	return (size_t)(*buf-start);
}

static inline bool NetEvent_Deserialize(uint8_t **buf, NetEvent_t *ev)
{
	ev->seq=Deserialize_uint32(buf);
	ev->type=(NetEventType_e)Deserialize_uint32(buf);

	switch(ev->type)
	{
		case NETEVENT_SPAWN:
			ev->spawn.id=Deserialize_uint32(buf);
			ev->spawn.objectType=(EntityObjectType_e)Deserialize_uint32(buf);
			ev->spawn.variant=Deserialize_uint32(buf);
			ev->spawn.position=Deserialize_vec3(buf);
			ev->spawn.velocity=Deserialize_vec3(buf);
			ev->spawn.orientation=Deserialize_vec4(buf);
			ev->spawn.radius=Deserialize_float(buf);
			break;

		case NETEVENT_DESTROY:
			ev->destroy.id=Deserialize_uint32(buf);
			break;

		case NETEVENT_SPLIT:
			ev->split.parentID=Deserialize_uint32(buf);
			ev->split.rngSnapshot=Deserialize_uint32(buf);
			ev->split.contactPoint=Deserialize_vec3(buf);
			ev->split.contactNormal=Deserialize_vec3(buf);
			ev->split.impactSpeed=Deserialize_float(buf);
			break;

		case NETEVENT_IMPULSE:
			ev->impulse.velocity=Deserialize_vec3(buf);
			ev->impulse.position=Deserialize_vec3(buf);
			break;

		default:
			return false;
	}

	return true;
}

static inline void NetEntityUpdate_Serialize(uint8_t **buf, const NetEntityUpdate_t *u)
{
	Serialize_uint32(buf, u->id);
	Serialize_vec3(buf, u->position);
	Serialize_vec3(buf, u->velocity);
	Serialize_vec4(buf, u->orientation);
}

static inline void NetEntityUpdate_Deserialize(uint8_t **buf, NetEntityUpdate_t *u)
{
	u->id=Deserialize_uint32(buf);
	u->position=Deserialize_vec3(buf);
	u->velocity=Deserialize_vec3(buf);
	u->orientation=Deserialize_vec4(buf);
}

static inline void NetSnapshotEntry_Serialize(uint8_t **buf, const NetSnapshotEntry_t *e)
{
	Serialize_uint32(buf, e->id);
	Serialize_uint32(buf, (uint32_t)e->objectType);
	Serialize_uint32(buf, e->variant);
	Serialize_vec3(buf, e->position);
	Serialize_vec3(buf, e->velocity);
	Serialize_vec4(buf, e->orientation);
	Serialize_float(buf, e->radius);
}

static inline void NetSnapshotEntry_Deserialize(uint8_t **buf, NetSnapshotEntry_t *e)
{
	e->id=Deserialize_uint32(buf);
	e->objectType=(EntityObjectType_e)Deserialize_uint32(buf);
	e->variant=Deserialize_uint32(buf);
	e->position=Deserialize_vec3(buf);
	e->velocity=Deserialize_vec3(buf);
	e->orientation=Deserialize_vec4(buf);
	e->radius=Deserialize_float(buf);
}

static inline void NetPlayerState_Serialize(uint8_t **buf, const NetPlayerState_t *p)
{
	Serialize_uint32(buf, p->clientID);
	Serialize_uint32(buf, p->serverTick);
	Serialize_vec3(buf, p->position);
	Serialize_vec3(buf, p->velocity);
	Serialize_vec4(buf, p->orientation);
}

static inline void NetPlayerState_Deserialize(uint8_t **buf, NetPlayerState_t *p)
{
	p->clientID=Deserialize_uint32(buf);
	p->serverTick=Deserialize_uint32(buf);
	p->position=Deserialize_vec3(buf);
	p->velocity=Deserialize_vec3(buf);
	p->orientation=Deserialize_vec4(buf);
}

static inline void NetEventQueue_Init(NetEventQueue_t *q)
{
	memset(q, 0, sizeof(*q));
}

static inline bool NetEventQueue_Push(NetEventQueue_t *q, const NetEvent_t *ev)
{
	uint32_t next=(q->head+1)&(NET_EVENT_QUEUE_SIZE-1);

	if(next==q->tail)
		return false;

	q->events[q->head]=*ev;
	q->events[q->head].seq=q->nextSeq++;
	q->sentTime[q->head]=0.0;
	q->head=next;

	return true;
}

static inline void NetEventQueue_Ack(NetEventQueue_t *q, uint32_t ackSeq)
{
	while(q->tail!=q->head)
	{
		if(q->events[q->tail].seq<=ackSeq)
			q->tail=(q->tail+1)&(NET_EVENT_QUEUE_SIZE-1);
		else
			break;
	}
}

static inline bool NetEventQueue_NeedsRetry(const NetEventQueue_t *q, double now, double retryInterval)
{
	if(q->tail==q->head)
		return false;

	double t=q->sentTime[q->tail];

	return (t==0.0)||(now-t>=retryInterval);
}

#endif
