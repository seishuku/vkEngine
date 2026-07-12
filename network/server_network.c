#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../network/network.h"
#include "../system/system.h"
#include "../entitylist.h"
#include "../utils/serial.h"
#include "../math/math.h"
#include "net_protocol.h"
#include "server_network.h"

extern uint32_t AddPlayer(uint32_t clientID);
extern void RemovePlayer(uint32_t clientID, uint32_t entityID);
extern uint32_t AddServerEmitter(vec3 position, vec3 velocity, float life);

// Internal state
static Socket_t			serverSocket=-1;
static EntityList_t		*entityList=NULL;
static uint32_t			serverSeed=0;
static uint32_t			serverTick=0;

static ServerClient_t	clients[NET_MAX_CLIENTS];
static uint32_t			clientCount=0;

static uint8_t			sendBuffer[65536];
static uint8_t			recvBuffer[65536];

// Helper functions
static ServerClient_t *FindClient(uint32_t address, uint16_t port)
{
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(clients[i].active&&clients[i].address==address&&clients[i].port==port)
            return &clients[i];
    }

    return NULL;
}

static ServerClient_t *FindClientByID(uint32_t id)
{
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(clients[i].active&&clients[i].id==id)
            return &clients[i];
    }

    return NULL;
}

static Entity_t *FindEntityByID(uint32_t id)
{
    for(uint32_t i=0;i<entityList->entityCount;i++)
    {
        if(entityList->entities[i].ID==id)
            return &entityList->entities[i];
    }

    return NULL;
}

static ServerClient_t *AllocClient(uint32_t address, uint16_t port, double now)
{
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(!clients[i].active)
        {
            memset(&clients[i], 0, sizeof(ServerClient_t));
            clients[i].active=true;
            clients[i].id=i;
            clients[i].address=address;
            clients[i].port=port;
            clients[i].lastSeen=now;
            clients[i].lastAckedSeq=UINT32_MAX;
            clients[i].playerEntityID=NET_INVALID_ID;
            NetEventQueue_Init(&clients[i].eventQueue);
            clientCount++;

            return &clients[i];
        }
    }

    return NULL;
}

static void RemoveClient(ServerClient_t *client)
{
    DBGPRINTF(DEBUG_INFO, "Client %d disconnected.\n", client->id);

    // Broadcast event
    if(client->playerEntityID!=NET_INVALID_ID)
    {
        NetEvent_t ev=
		{
			.type=NETEVENT_DESTROY,
			.destroy={ .id=client->playerEntityID }
		};

        for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
        {
            if(clients[i].active&&clients[i].id!=client->id)
                NetEventQueue_Push(&clients[i].eventQueue, &ev);
        }
    }

    RemovePlayer(client->id, client->playerEntityID);

    client->active=false;
    clientCount--;
}

// Delta compression
static bool EntityNeedsUpdate(ServerClient_t *client, const Entity_t *entity)
{
    if(entity->ID>=ID_MAX)
        return false;

    EntityDeltaState_t *delta=&client->deltaState[entity->ID];

    if(!delta->everSent)
        return true;

	if(Vec3_LengthSq(Vec3_Subv(entity->body->velocity, delta->lastSentVelocity))>NET_VELOCITY_EPSILON*NET_VELOCITY_EPSILON)
        return true;

	if(Vec3_LengthSq(Vec3_Subv(entity->body->angularVelocity, delta->lastSentAngularVelocity))>NET_VELOCITY_EPSILON*NET_VELOCITY_EPSILON)
        return true;

    if(fabsf(Vec4_Dot(entity->body->orientation, delta->lastSentOrientation))<NET_ORIENTATION_EPSILON)
        return true;

    return false;
}

static void MarkEntitySent(ServerClient_t *client, const Entity_t *entity)
{
    if(entity->ID>=ID_MAX)
        return;

    EntityDeltaState_t *delta=&client->deltaState[entity->ID];
    delta->lastSentVelocity=entity->body->velocity;
    delta->lastSentAngularVelocity=entity->body->angularVelocity;
    delta->lastSentOrientation=entity->body->orientation;
    delta->everSent=true;
}

// Send delta-compressed entity updates to a client
static void SendEntityUpdates(ServerClient_t *client)
{
    uint32_t i=0;

    while(i<entityList->entityCount)
    {
        uint8_t *pBuffer=sendBuffer;
        uint32_t batchCount=0;

        // Reserve space for header
        uint8_t *pHeader=pBuffer;
        Serialize_uint32(&pBuffer, NETMAGIC_UPDATE);
        Serialize_uint32(&pBuffer, serverTick);
        uint8_t *pCount=pBuffer;
        Serialize_uint32(&pBuffer, 0);

        // Fill batch with entities that need updates
        while(i<entityList->entityCount&&batchCount<NET_UPDATE_BATCH)
        {
            const Entity_t *entity=&entityList->entities[i];

            // Skip client owned player entity
            if(entity->objectType==ENTITYOBJECTTYPE_PLAYER&&entity->ID==client->playerEntityID)
            {
                i++;
                continue;
            }

            if(EntityNeedsUpdate(client, entity))
            {
                NetEntityUpdate_t u=
				{
					.id=entity->ID,
					.position=entity->body->position,
					.velocity=entity->body->velocity,
					.orientation=entity->body->orientation
				};
                NetEntityUpdate_Serialize(&pBuffer, &u);
                MarkEntitySent(client, entity);
                batchCount++;
            }

            i++;
        }

        // Only send if batch has anything in it
        if(batchCount>0)
        {
            // Fill in count
            uint8_t *pTmp=pCount;
            Serialize_uint32(&pTmp, batchCount);

            Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), client->address, client->port);
        }
    }
}

// Retry unacked events
static void RetryEvents(ServerClient_t *client, double now)
{
    if(!NetEventQueue_NeedsRetry(&client->eventQueue, now, SERVER_EVENT_RETRY))
        return;

    NetEventQueue_t *q=&client->eventQueue;
    uint32_t i=q->tail;

    while(i!=q->head)
    {
        uint8_t *pBuffer=sendBuffer;
        size_t len=NetEvent_Serialize(&pBuffer, &q->events[i]);

        Network_SocketSend(serverSocket, sendBuffer, (uint32_t)len, client->address, client->port);

        q->sentTime[i]=now;
        i=(i+1)&(NET_EVENT_QUEUE_SIZE-1);
    }
}

// Broadcast player states to a client
static void SendPlayerStates(ServerClient_t *client, double now)
{
    if(now-client->lastStatus<SERVER_STATUS_RATE)
        return;

    client->lastStatus=now;

    uint8_t *pBuffer=sendBuffer;

    Serialize_uint32(&pBuffer, NETMAGIC_STATUS);
    Serialize_uint32(&pBuffer, clientCount);

    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(!clients[i].active)
            continue;

        NetPlayerState_t p=
		{
			.clientID=clients[i].id,
			.serverTick=serverTick,
			.position=clients[i].position,
			.velocity=clients[i].velocity,
			.orientation=clients[i].orientation
		};
        NetPlayerState_Serialize(&pBuffer, &p);
    }

    Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), client->address, client->port);
}

// Packet handlers
static void HandleConnect(uint32_t address, uint16_t port, double now)
{
    if(FindClient(address, port))
        return;

    ServerClient_t *client=AllocClient(address, port, now);

    if(!client)
    {
        DBGPRINTF(DEBUG_WARNING, "HandleConnect: server full, rejecting client.\n");
        return;
    }

    DBGPRINTF(DEBUG_INFO, "Client %d connected from 0x%X:%d\n", client->id, address, port);

    // Spawn player entity for this client
    client->playerEntityID=AddPlayer(client->id);

    // Send connect response
    uint8_t *pBuffer=sendBuffer;

    Serialize_uint32(&pBuffer, NETMAGIC_CONNECT);
    Serialize_uint32(&pBuffer, client->id);
    Serialize_uint32(&pBuffer, serverSeed);

    Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), address, port);

    // Notify existing clients of new player
    if(client->playerEntityID!=NET_INVALID_ID)
    {
        NetEvent_t ev=
		{
			.type=NETEVENT_SPAWN,
			.spawn=
			{
				.id=client->playerEntityID,
				.objectType=ENTITYOBJECTTYPE_PLAYER,
				.variant=client->id,
				.position=Vec3b(0.0f),
				.velocity=Vec3b(0.0f),
				.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				.radius=10.0f
			}
		};

        for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
        {
            if(clients[i].active&&clients[i].id!=client->id)
                NetEventQueue_Push(&clients[i].eventQueue, &ev);
        }
    }

    // Send snapshot in batches
    uint32_t sent=0;

    while(sent<entityList->entityCount)
    {
        pBuffer=sendBuffer;

        uint32_t batchSize=entityList->entityCount-sent;

        if(batchSize>NET_SNAPSHOT_BATCH)
            batchSize=NET_SNAPSHOT_BATCH;

        Serialize_uint32(&pBuffer, NETMAGIC_SNAPSHOT);
        Serialize_uint32(&pBuffer, batchSize);

        for(uint32_t i=0;i<batchSize;i++)
        {
            const Entity_t *entity=&entityList->entities[sent+i];

            NetSnapshotEntry_t e=
			{
				.id=entity->ID,
				.objectType=entity->objectType,
				.variant=entity->modelID,
				.position=entity->body->position,
				.velocity=entity->body->velocity,
				.orientation=entity->body->orientation,
				.radius=entity->body->radius
			};
            NetSnapshotEntry_Serialize(&pBuffer, &e);

            // Mark all snapshot entities as sent so delta compression won't re-send them
            MarkEntitySent(client, entity);
        }

        Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), address, port);
        sent+=batchSize;
    }
}

static void HandleDisconnect(uint8_t **pBuffer, uint32_t address, uint16_t port)
{
    uint32_t id=Deserialize_uint32(pBuffer);
    ServerClient_t *client=FindClientByID(id);

    if(!client||client->address!=address||client->port!=port)
    {
        DBGPRINTF(DEBUG_WARNING, "HandleDisconnect: unknown or mismatched client ID %d.\n", id);
        return;
    }

    RemoveClient(client);
}

static void HandleStatus(uint8_t **pBuffer, uint32_t address, uint16_t port, double now)
{
    ServerClient_t *client=FindClient(address, port);

    if(!client)
    {
        DBGPRINTF(DEBUG_WARNING, "HandleStatus: received STATUS from unknown client.\n");
        return;
    }

    client->lastSeen=now;

    uint32_t clientID=Deserialize_uint32(pBuffer);
    (void)clientID;

    uint32_t ackSeq=Deserialize_uint32(pBuffer);
    NetEventQueue_Ack(&client->eventQueue, ackSeq);

    client->position=Deserialize_vec3(pBuffer);
    client->velocity=Deserialize_vec3(pBuffer);
    client->orientation=Deserialize_vec4(pBuffer);

    // Update player entity body
    if(client->playerEntityID!=NET_INVALID_ID)
    {
        Entity_t *entity=FindEntityByID(client->playerEntityID);

        if(entity&&entity->body)
        {
            entity->body->position=client->position;
            entity->body->velocity=client->velocity;
            entity->body->orientation=client->orientation;
        }
    }
}

static void HandleAck(uint8_t **pBuffer, uint32_t address, uint16_t port)
{
    ServerClient_t *client=FindClient(address, port);

    if(!client)
        return;

    uint32_t ackSeq=Deserialize_uint32(pBuffer);
    NetEventQueue_Ack(&client->eventQueue, ackSeq);
}

// Handle events sent from client to server (e.g. projectile spawn)
static void HandleClientEvent(uint8_t **pBuffer, uint32_t address, uint16_t port)
{
    ServerClient_t *client=FindClient(address, port);

    if(!client)
        return;

    NetEvent_t ev;

    if(!NetEvent_Deserialize(pBuffer, &ev))
    {
        DBGPRINTF(DEBUG_WARNING, "HandleClientEvent: failed to deserialize event from client %d\n", client->id);
        return;
    }

    switch(ev.type)
    {
        case NETEVENT_SPAWN:
        {
            if(ev.spawn.objectType!=ENTITYOBJECTTYPE_PROJECTILE)
            {
                DBGPRINTF(DEBUG_WARNING, "HandleClientEvent: client %d tried to spawn non-projectile\n", client->id);
                break;
            }

            // Create authoritative projectile on server
            uint32_t entityID=AddServerEmitter(ev.spawn.position, ev.spawn.velocity, ev.spawn.radius);

            if(entityID==NET_INVALID_ID)
                break;

            // Broadcast to all clients
            NetEvent_t spawnEv=
			{
				.type=NETEVENT_SPAWN,
				.spawn=
				{
					.id=entityID,
					.objectType=ENTITYOBJECTTYPE_PROJECTILE,
					.variant=client->id,
					.position=ev.spawn.position,
					.velocity=ev.spawn.velocity,
					.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f),
					.radius=ev.spawn.radius,
				},
			};

            ServerNetwork_BroadcastEvent(&spawnEv);
            break;
        }

        default:
            DBGPRINTF(DEBUG_WARNING, "HandleClientEvent: unhandled event type %d from client %d\n", ev.type, client->id);
            break;
    }

    // Send ACK back to client so it can retire the event from its retry queue
	// TODO: If this fails to send, the client will retry and projectile duplication will happen.
	//           Maybe just ditch this?
	{
		uint8_t *pBuffer=sendBuffer;
		Serialize_uint32(&pBuffer, NETMAGIC_ACK);
		Serialize_uint32(&pBuffer, ev.seq);
		Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), client->address, client->port);
	}
}

// ============================================================
// Public API
bool ServerNetwork_Init(uint16_t port, EntityList_t *list, uint32_t seed)
{
    memset(clients, 0, sizeof(clients));
    clientCount=0;
    serverTick=0;
    entityList=list;
    serverSeed=seed;

    Network_Init();

    serverSocket=Network_CreateSocket();

    if(serverSocket==-1)
        return false;

    if(!Network_SocketBind(serverSocket, 0, port))
    {
        Network_SocketClose(serverSocket);
        serverSocket=-1;
        return false;
    }

    DBGPRINTF(DEBUG_INFO, "Server listening on port %d\n", port);

    return true;
}

void ServerNetwork_Destroy(void)
{
    if(serverSocket!=-1)
    {
        for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
        {
            if(!clients[i].active)
                continue;

            uint8_t *pBuffer=sendBuffer;

            Serialize_uint32(&pBuffer, NETMAGIC_DISCONNECT);
            Serialize_uint32(&pBuffer, clients[i].id);

            Network_SocketSend(serverSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), clients[i].address, clients[i].port);
        }

        Network_SocketClose(serverSocket);
        serverSocket=-1;
    }

    Network_Destroy();

    memset(clients, 0, sizeof(clients));
    clientCount=0;
}

void ServerNetwork_Update(double now)
{
    if(serverSocket==-1)
        return;

    uint32_t address=0;
    uint16_t port=0;

    while(true)
    {
        memset(recvBuffer, 0, sizeof(recvBuffer));
        uint8_t *pBuffer=recvBuffer;

        int32_t bytesRec=Network_SocketReceive(serverSocket, recvBuffer, sizeof(recvBuffer),
            &address, &port);

        if(bytesRec<=0)
            break;

        uint32_t magic=Deserialize_uint32(&pBuffer);

        switch(magic)
        {
            case NETMAGIC_CONNECT:
                HandleConnect(address, port, now);
                break;

            case NETMAGIC_DISCONNECT:
                HandleDisconnect(&pBuffer, address, port);
                break;

            case NETMAGIC_STATUS:
                HandleStatus(&pBuffer, address, port, now);
                break;

            case NETMAGIC_ACK:
                HandleAck(&pBuffer, address, port);
                break;

            case NETMAGIC_EVENT:
                HandleClientEvent(&pBuffer, address, port);
                break;

            default:
                DBGPRINTF(DEBUG_WARNING, "ServerNetwork_Update: unknown magic 0x%X from 0x%X:%d\n", magic, address, port);
                break;
        }
    }

    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(!clients[i].active)
            continue;

        if(now-clients[i].lastSeen>SERVER_CLIENT_TIMEOUT)
        {
            DBGPRINTF(DEBUG_WARNING, "Client %d timed out.\n", clients[i].id);
            RemoveClient(&clients[i]);
            continue;
        }

        SendEntityUpdates(&clients[i]);
        RetryEvents(&clients[i], now);
        SendPlayerStates(&clients[i], now);
    }

    serverTick++;
}

void ServerNetwork_SendPlayerImpulse(uint32_t playerEntityID, vec3 position, vec3 velocity)
{
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(!clients[i].active||clients[i].playerEntityID!=playerEntityID)
            continue;

        NetEvent_t ev=
		{
			.type=NETEVENT_IMPULSE,
			.impulse=
			{
				.position=position,
				.velocity=velocity
			}
		};
        NetEventQueue_Push(&clients[i].eventQueue, &ev);
        break;
    }
}

void ServerNetwork_BroadcastEvent(const NetEvent_t *ev)
{
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(clients[i].active)
            NetEventQueue_Push(&clients[i].eventQueue, ev);
    }
}

void ServerNetwork_SendEvent(uint32_t clientID, const NetEvent_t *ev)
{
    ServerClient_t *client=FindClientByID(clientID);

    if(client)
        NetEventQueue_Push(&client->eventQueue, ev);
}
