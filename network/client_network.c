#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../camera/camera.h"
#include "../entitylist.h"
#include "../math/math.h"
#include "../network/network.h"
#include "../physics/particle.h"
#include "../system/system.h"
#include "../utils/serial.h"
#include "../utils/id.h"
#include "../asteroids.h"
#include "../assetmanager.h"
#include "client_network.h"
#include "net_protocol.h"

// External symbols
extern EntityList_t entityList;

#define MAX_EMITTERS 1000

typedef struct
{
	RigidBody_t body;
	uint32_t emitterID, entityID;
	float life;
} PhyParticleEmitter_t;

extern PhyParticleEmitter_t emitters[MAX_EMITTERS];

extern matrix FighterTransform(const RigidBody_t *body);

extern ParticleSystem_t particleSystem;
extern void emitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle);

// Public state
NetPlayerState_t    netPlayers[NET_MAX_CLIENTS];
uint32_t            netPlayerCount=0;
uint32_t            localClientID=NET_INVALID_ID;

// Net ID to local entity ID remapping
static uint32_t     netIDToLocalID[ID_MAX];
static bool         netIDMapped[ID_MAX];

static void NetMap_Set(uint32_t netID, uint32_t localID)
{
    if(netID>=ID_MAX)
        return;

    netIDToLocalID[netID]=localID;
    netIDMapped[netID]=true;
}

static void NetMap_Clear(uint32_t netID)
{
    if(netID>=ID_MAX)
        return;

    netIDMapped[netID]=false;
}

static uint32_t NetMap_GetLocalID(uint32_t netID)
{
    if(netID>=ID_MAX||!netIDMapped[netID])
        return NET_INVALID_ID;

    return netIDToLocalID[netID];
}

// Remote player bodies
static Camera_t     remotePlayers[NET_MAX_CLIENTS];
static bool         remotePlayerActive[NET_MAX_CLIENTS];

// Spawn visual only particle emitter
void FireParticleEmitterVisualOnly(vec3 position, vec3 direction)
{
	// Create a new particle emitter
	vec3 randVec=Vec3(RandFloat(), RandFloat(), RandFloat());
	Vec3_Normalize(&randVec);
	randVec=Vec3_Muls(randVec, 100.0f);

	// Fire the emitter camera radius units away from Position in the direction of Direction, blend in some of the direction for particle vs emitter velocity for particle tails.
	uint32_t ID=ParticleSystem_AddEmitter(&particleSystem, position, Vec3_Muls(direction, 75.0f), Vec3b(0.0f), randVec, 5.0f, 500, PARTICLE_EMITTER_CONTINOUS, emitterCallback);

	// Emitter list full?
	if(ID==UINT32_MAX)
		return;

	// Search for first dead particle emitter
	for(uint32_t i=0;i<MAX_EMITTERS;i++)
	{
		// When found, assign the new emitter ID to that particle, set position/direction/life and break out
		if(emitters[i].life<0.0f)
		{
			emitters[i].emitterID=ID;
			emitters[i].body.position=position;

			Vec3_Normalize(&direction);

			emitters[i].body.velocity=Vec3_Muls(direction, 100.0f);
			emitters[i].life=15.0f;
			break;
		}
	}

	// Finally, play the audio SFX
	Audio_PlaySample(&AssetManager_GetAsset(assets, RandRange(SOUND_PEW1, SOUND_PEW3))->sound, false, 1.0f, position);
}

// Internal state
static Socket_t     clientSocket=-1;
static uint32_t     serverAddress=0;
static uint16_t     serverPort=0;
static Camera_t     *localCamera=NULL;
static bool         connected=false;

static uint8_t      sendBuffer[65536];
static uint8_t      recvBuffer[65536];

static uint32_t     lastServerTick=0;
static uint32_t     lastAckedEventSeq=0;
static uint32_t     pendingAckSeq=0;
static bool         hasPendingAck=false;

static double       lastStatusSend=0.0;

// Client->server event queue - reliable delivery
static NetEventQueue_t clientEventQueue;

static Entity_t *FindEntityByLocalID(uint32_t localID)
{
    if(localID==NET_INVALID_ID)
        return NULL;

    for(uint32_t i=0;i<entityList.entityCount;i++)
    {
        if(entityList.entities[i].ID==localID)
            return &entityList.entities[i];
    }

    return NULL;
}

static Entity_t *FindEntityByNetID(uint32_t netID)
{
    return FindEntityByLocalID(NetMap_GetLocalID(netID));
}

static void SpawnEntity(uint32_t netID, EntityObjectType_e objectType, uint32_t extra, vec3 position, vec3 velocity, vec4 orientation, float radius)
{
    // Don't spawn if already mapped
    if(netIDMapped[netID])
        return;

    switch(objectType)
    {
        case ENTITYOBJECTTYPE_FIELD:
        {
            AddAsteroid(position, velocity, radius, extra);

            uint32_t localID=entityList.entities[entityList.entityCount-1].ID;
            // Also fix up asteroidModels entityID to use local ID and orientation
            asteroidModels[numAsteroids-1].entityID=localID;
			asteroids[numAsteroids-1].orientation=orientation;
            NetMap_Set(netID, localID);
            break;
        }

        case ENTITYOBJECTTYPE_PLAYER:
        {
            // extra carries clientID for remote players
            uint32_t clientID=extra;

            // Skip local player
            if(clientID==localClientID)
                break;

            if(clientID>=NET_MAX_CLIENTS)
                break;

            CameraInit(&remotePlayers[clientID], position, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));

            remotePlayers[clientID].body.orientation=orientation;
            remotePlayerActive[clientID]=true;

            uint32_t localID=EntityList_Add(&entityList, &remotePlayers[clientID].body, false, MODEL_FIGHTER, TEXTURE_FIGHTER1, TEXTURE_FIGHTER1_NORMAL, ENTITYOBJECTTYPE_PLAYER, FighterTransform);

            NetMap_Set(netID, localID);
            break;
        }

        case ENTITYOBJECTTYPE_PROJECTILE:
        {
            // Find a free emitter slot
            for(uint32_t i=0;i<MAX_EMITTERS;i++)
            {
                if(emitters[i].life>0.0f)
                    continue;

                // Set up the physics body
                emitters[i].body.position=position;
                emitters[i].body.velocity=velocity;
                emitters[i].body.type=RIGIDBODY_SPHERE;
                emitters[i].body.radius=radius;

                // Add physics entity - purely local ID, no server ID fixup
                uint32_t localID=EntityList_Add(&entityList, &emitters[i].body, true, 0, 0, 0, ENTITYOBJECTTYPE_PROJECTILE, NULL);

                emitters[i].entityID=localID;
                NetMap_Set(netID, localID);

                // Visual only - no entity add
                vec3 dir=velocity;
                Vec3_Normalize(&dir);
                FireParticleEmitterVisualOnly(position, dir);

                break;
            }
        }

		default:
			break;
    }
}

// Packet handlers
static void HandleUpdate(uint8_t **pBuffer, float dt)
{
    uint32_t tick=Deserialize_uint32(pBuffer);

    // Drop stale packets
    if(tick<lastServerTick)
        return;

    lastServerTick=tick;

    uint32_t count=Deserialize_uint32(pBuffer);

    for(uint32_t i=0;i<count;i++)
    {
        NetEntityUpdate_t u;
        NetEntityUpdate_Deserialize(pBuffer, &u);

        // Look up local entity via net->local mapping
        Entity_t *entity=FindEntityByNetID(u.id);

        if(!entity||!entity->body)
            continue;

        // Skip local player
        // if(entity->objectType==ENTITYOBJECTTYPE_PLAYER&&entity->body==&localCamera->body)
        //     continue;

        entity->body->position=u.position;
        entity->body->velocity=u.velocity;
        entity->body->orientation=u.orientation;
    }
}

static void HandleSnapshot(uint8_t **pBuffer)
{
    uint32_t count=Deserialize_uint32(pBuffer);

    for(uint32_t i=0;i<count;i++)
    {
        NetSnapshotEntry_t e;
        NetSnapshotEntry_Deserialize(pBuffer, &e);

        SpawnEntity(e.id, e.objectType, e.variant, e.position, e.velocity, e.orientation, e.radius);
    }
}

static void HandleEvent(uint8_t **pBuffer)
{
    NetEvent_t ev;

    if(!NetEvent_Deserialize(pBuffer, &ev))
    {
        DBGPRINTF(DEBUG_ERROR, "ClientNetwork: failed to deserialize event.\n");
        return;
    }

    if(ev.seq<=lastAckedEventSeq&&lastAckedEventSeq!=0)
        return;

    switch(ev.type)
    {
        case NETEVENT_SPAWN:
        {
            SpawnEntity(ev.spawn.id, ev.spawn.objectType, ev.spawn.variant, ev.spawn.position, ev.spawn.velocity, ev.spawn.orientation, ev.spawn.radius);
            break;
        }

        case NETEVENT_DESTROY:
        {
            uint32_t localID=NetMap_GetLocalID(ev.destroy.id);

            if(localID==NET_INVALID_ID)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: DESTROY for unmapped net ID %d\n", ev.destroy.id);
                break;
            }

            Entity_t *entity=FindEntityByLocalID(localID);

            if(!entity)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: DESTROY local entity %d not found\n", localID);
                NetMap_Clear(ev.destroy.id);
                break;
            }

            // Clean up remote player slot if needed
            if(entity->objectType==ENTITYOBJECTTYPE_PLAYER)
            {
                for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
                {
                    if(remotePlayerActive[i]&&entity->body==&remotePlayers[i].body)
                    {
                        remotePlayerActive[i]=false;
                        break;
                    }
                }
            }

            // Kill projectile emitter if needed
            if(entity->objectType==ENTITYOBJECTTYPE_PROJECTILE)
            {
                for(uint32_t i=0;i<MAX_EMITTERS;i++)
                {
                    if(emitters[i].entityID==localID)
                    {
                        emitters[i].life=0.0f;
                        break;
                    }
                }
            }

            EntityList_Remove(&entityList, localID);
            NetMap_Clear(ev.destroy.id);
            break;
        }

	    case NETEVENT_IMPULSE:
	    {
		    localCamera->body.velocity=ev.impulse.velocity;
		    localCamera->body.position=ev.impulse.position;
		    break;
	    }

	    case NETEVENT_SPLIT:
        {
            // Find local entity via net->local mapping
            uint32_t localID=NetMap_GetLocalID(ev.split.parentID);

            if(localID==NET_INVALID_ID)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: SPLIT for unmapped net ID %d\n", ev.split.parentID);
                break;
            }

            Entity_t *parent=FindEntityByLocalID(localID);

            if(!parent)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: SPLIT local entity %d not found\n", localID);
                break;
            }

            // Find asteroid index by body pointer
            uint32_t asteroidIndex=UINT32_MAX;

            for(uint32_t i=0;i<numAsteroids;i++)
            {
                if(parent->body==&asteroids[i])
                {
                    asteroidIndex=i;
                    break;
                }
            }

            if(asteroidIndex==UINT32_MAX)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: SPLIT body not found for local entity %d\n", localID);
                break;
            }

            // Clear mapping for parent - it's going away
            NetMap_Clear(ev.split.parentID);

            RandomSeed(ev.split.rngSnapshot);

            SplitAsteroid(asteroidIndex, (ContactPoint_t){ .position=ev.split.contactPoint, .normal=ev.split.contactNormal, }, ev.split.impactSpeed);
            break;
        }
    }

    lastAckedEventSeq=ev.seq;
    pendingAckSeq=ev.seq;
    hasPendingAck=true;
}

static void HandleStatus(uint8_t **pBuffer)
{
    netPlayerCount=Deserialize_uint32(pBuffer);

    if(netPlayerCount>NET_MAX_CLIENTS)
    {
        DBGPRINTF(DEBUG_ERROR, "ClientNetwork: mangled STATUS - player count %d > max %d\n", netPlayerCount, NET_MAX_CLIENTS);
        return;
    }

    for(uint32_t i=0;i<netPlayerCount;i++)
    {
        NetPlayerState_t p;
        NetPlayerState_Deserialize(pBuffer, &p);

        if(p.clientID<NET_MAX_CLIENTS)
            netPlayers[p.clientID]=p;
    }
}

static void HandleDisconnect(void)
{
    DBGPRINTF(DEBUG_INFO, "ClientNetwork: server disconnected.\n");
    ClientNetwork_Destroy();
}

static void SendStatus(double now)
{
    if(now-lastStatusSend<CLIENT_STATUS_RATE)
        return;

    lastStatusSend=now;

    uint8_t *pBuffer=sendBuffer;
    uint32_t ackSeq=hasPendingAck?pendingAckSeq:lastAckedEventSeq;

    Serialize_uint32(&pBuffer, NETMAGIC_STATUS);
    Serialize_uint32(&pBuffer, localClientID);
    Serialize_uint32(&pBuffer, ackSeq);
    Serialize_vec3(&pBuffer, localCamera->body.position);
    Serialize_vec3(&pBuffer, localCamera->body.velocity);
    Serialize_vec4(&pBuffer, localCamera->body.orientation);

    Network_SocketSend(clientSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), serverAddress, serverPort);

    hasPendingAck=false;
}

// Public API
bool ClientNetwork_Init(uint32_t address, uint16_t port, Camera_t *camera)
{
    Network_Init();

    clientSocket=Network_CreateSocket();

    if(clientSocket==-1)
        return false;

    serverAddress=address;
    serverPort=port;
    localCamera=camera;

    memset(netPlayers, 0, sizeof(netPlayers));
    memset(netIDToLocalID, 0, sizeof(netIDToLocalID));
    memset(netIDMapped, 0, sizeof(netIDMapped));
    memset(remotePlayers, 0, sizeof(remotePlayers));
    memset(remotePlayerActive, 0, sizeof(remotePlayerActive));
    NetEventQueue_Init(&clientEventQueue);
    netPlayerCount=0;
    lastServerTick=0;
    lastAckedEventSeq=0;
    hasPendingAck=false;

    uint8_t *pBuffer=sendBuffer;

    Serialize_uint32(&pBuffer, NETMAGIC_CONNECT);

    Network_SocketSend(clientSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), serverAddress, serverPort);

    // Wait for connect response
    double timeout=GetClock()+5.0;

    while(GetClock()<timeout)
    {
        uint8_t *pRecv=recvBuffer;
        uint32_t fromAddress=0;
        uint16_t fromPort=0;

        memset(recvBuffer, 0, sizeof(recvBuffer));

        int32_t bytes=Network_SocketReceive(clientSocket, recvBuffer, sizeof(recvBuffer), &fromAddress, &fromPort);

        if(bytes<=0)
            continue;

        uint32_t magic=Deserialize_uint32(&pRecv);

        if(magic!=NETMAGIC_CONNECT)
            continue;

        localClientID=Deserialize_uint32(&pRecv);

        if(localClientID>=NET_MAX_CLIENTS)
        {
            DBGPRINTF(DEBUG_ERROR, "ClientNetwork_Init: invalid client ID %d.\n", localClientID);
            Network_SocketClose(clientSocket);
            clientSocket=-1;
            return false;
        }

        uint32_t seed=Deserialize_uint32(&pRecv);
        RandomSeed(seed);

        DBGPRINTF(DEBUG_INFO, "ClientNetwork_Init: connected, ID=%d seed=%d\n", localClientID, seed);

        connected=true;
        return true;
    }

    DBGPRINTF(DEBUG_WARNING, "ClientNetwork_Init: connection timed out.\n");

    Network_SocketClose(clientSocket);
    clientSocket=-1;

    return false;
}

void ClientNetwork_Destroy(void)
{
    if(clientSocket==-1)
        return;

    uint8_t *pBuffer=sendBuffer;

    Serialize_uint32(&pBuffer, NETMAGIC_DISCONNECT);
    Serialize_uint32(&pBuffer, localClientID);

    Network_SocketSend(clientSocket, sendBuffer, (uint32_t)(pBuffer-sendBuffer), serverAddress, serverPort);

    // Clean up remote player entities
    for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
    {
        if(!remotePlayerActive[i])
            continue;

        for(uint32_t j=0;j<entityList.entityCount;j++)
        {
            if(entityList.entities[j].body==&remotePlayers[i].body)
            {
                EntityList_Remove(&entityList, entityList.entities[j].ID);
                break;
            }
        }

        remotePlayerActive[i]=false;
    }

    Network_SocketClose(clientSocket);
    clientSocket=-1;
    connected=false;

    Network_Destroy();

    localClientID=NET_INVALID_ID;
    netPlayerCount=0;

    memset(netIDToLocalID, 0, sizeof(netIDToLocalID));
    memset(netIDMapped, 0, sizeof(netIDMapped));
}

void ClientNetwork_Update(double now, float dt)
{
    if(!ClientNetwork_IsConnected())
        return;

    while(true)
    {
        memset(recvBuffer, 0, sizeof(recvBuffer));
        uint8_t *pBuffer=recvBuffer;

        uint32_t fromAddress=0;
        uint16_t fromPort=0;

        int32_t bytes=Network_SocketReceive(clientSocket, recvBuffer, sizeof(recvBuffer), &fromAddress, &fromPort);

        if(bytes<=0)
            break;

        if(fromAddress!=serverAddress||fromPort!=serverPort)
            continue;

        uint32_t magic=Deserialize_uint32(&pBuffer);

        switch(magic)
        {
            case NETMAGIC_ACK:
            {
                uint32_t ackSeq=Deserialize_uint32(&pBuffer);
                NetEventQueue_Ack(&clientEventQueue, ackSeq);
                break;
            }

            case NETMAGIC_UPDATE:
                HandleUpdate(&pBuffer, dt);
                break;

            case NETMAGIC_SNAPSHOT:
                HandleSnapshot(&pBuffer);
                break;

            case NETMAGIC_EVENT:
                HandleEvent(&pBuffer);
                break;

            case NETMAGIC_STATUS:
                HandleStatus(&pBuffer);
                break;

            case NETMAGIC_DISCONNECT:
                HandleDisconnect();
                return;

            default:
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork_Update: unknown magic 0x%X\n", magic);
                break;
        }
    }

    // Retry unacked client events
    if(NetEventQueue_NeedsRetry(&clientEventQueue, now, CLIENT_EVENT_RETRY))
    {
        NetEventQueue_t *q=&clientEventQueue;
        uint32_t i=q->tail;

        while(i!=q->head)
        {
            uint8_t *pBuffer=sendBuffer;
            size_t len=NetEvent_Serialize(&pBuffer, &q->events[i]);

            Network_SocketSend(clientSocket, sendBuffer, (uint32_t)len,
                serverAddress, serverPort);

            q->sentTime[i]=now;
            i=(i+1)&(NET_EVENT_QUEUE_SIZE-1);
        }
    }

    SendStatus(now);
}

void ClientNetwork_SendEvent(const NetEvent_t *ev)
{
    if(!ClientNetwork_IsConnected())
        return;

    NetEventQueue_Push(&clientEventQueue, ev);
}

bool ClientNetwork_IsConnected(void)
{
    return clientSocket!=-1&&connected;
}
