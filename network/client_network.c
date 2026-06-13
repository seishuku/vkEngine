#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../camera/camera.h"
#include "../entitylist.h"
#include "../math/math.h"
#include "../network/network.h"
#include "../system/system.h"
#include "../utils/serial.h"
#include "../utils/id.h"
#include "../asteroids.h"
#include "../assetmanager.h"
#include "client_network.h"
#include "net_protocol.h"

// External symbols
extern EntityList_t entityList;
extern matrix FighterTransform(const RigidBody_t *body);

// Public state
NetPlayerState_t    netPlayers[NET_MAX_CLIENTS];
uint32_t            netPlayerCount=0;
uint32_t            localClientID=NET_INVALID_ID;

// Internal state
static Camera_t     remotePlayers[NET_MAX_CLIENTS];
static bool         remotePlayerActive[NET_MAX_CLIENTS];

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

static Entity_t *FindEntityByID(uint32_t id)
{
    for(uint32_t i=0;i<entityList.entityCount;i++)
    {
        if(entityList.entities[i].ID==id)
            return &entityList.entities[i];
    }

    return NULL;
}

static void SpawnEntity(uint32_t serverID, EntityObjectType_e objectType, uint32_t extra, vec3 position, vec3 velocity, vec4 orientation, float radius)
{
    switch(objectType)
    {
        case ENTITYOBJECTTYPE_FIELD:
        {
            AddAsteroid(position, velocity, radius, extra);

            // Fix up ID to match server
            Entity_t *entity=&entityList.entities[entityList.entityCount-1];
            ID_Remove(entityList.IDPool, entity->ID);
            entity->ID=serverID;
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

            uint32_t id=EntityList_Add(&entityList, &remotePlayers[clientID].body, false, MODEL_FIGHTER, TEXTURE_FIGHTER1, TEXTURE_FIGHTER1_NORMAL, ENTITYOBJECTTYPE_PLAYER, FighterTransform);

            // Fix up ID to match server
            Entity_t *entity=&entityList.entities[entityList.entityCount-1];
            ID_Remove(entityList.IDPool, entity->ID);
            entity->ID=serverID;
            break;
        }

        case ENTITYOBJECTTYPE_PROJECTILE:
            // TODO
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

        Entity_t *entity=FindEntityByID(u.id);

        if(!entity||!entity->body)
            continue;

        // Skip local player
        if(entity->objectType==ENTITYOBJECTTYPE_PLAYER&&entity->body==&localCamera->body)
            continue;

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

        // Skip if already exists
        if(FindEntityByID(e.id))
            continue;

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
            if(FindEntityByID(ev.spawn.id))
                break;
 
            SpawnEntity(ev.spawn.id, ev.spawn.objectType, ev.spawn.variant, ev.spawn.position, ev.spawn.velocity, ev.spawn.orientation, ev.spawn.radius);
            break;
        }
 
        case NETEVENT_DESTROY:
        {
            Entity_t *entity=FindEntityByID(ev.destroy.id);
 
            if(!entity)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: DESTROY for unknown entity %d\n", ev.destroy.id);
                break;
            }
 
            for(uint32_t i=0;i<NET_MAX_CLIENTS;i++)
            {
                if(remotePlayerActive[i]&&entity->body==&remotePlayers[i].body)
                {
                    remotePlayerActive[i]=false;
                    break;
                }
            }
 
            EntityList_Remove(&entityList, ev.destroy.id);
            break;
        }
 
        case NETEVENT_IMPULSE:
        {
            localCamera->body.velocity=ev.impulse.velocity;
 
            // Correct position if it's too far from the server's position
            vec3 error=Vec3_Subv(ev.impulse.position, localCamera->body.position);
			const float posThreshold=5.0f;

			if(Vec3_LengthSq(error)>posThreshold*posThreshold)
                localCamera->body.position=ev.impulse.position;
 
            break;
        }
 
        case NETEVENT_SPLIT:
        {
            Entity_t *parent=FindEntityByID(ev.split.parentID);
 
            if(!parent)
            {
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: SPLIT for unknown entity %d\n", ev.split.parentID);
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
                DBGPRINTF(DEBUG_WARNING, "ClientNetwork: SPLIT body not found for entity %d\n", ev.split.parentID);
                break;
            }
 
            // Restore RNG state and replay split deterministically
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
    memset(remotePlayers, 0, sizeof(remotePlayers));
    memset(remotePlayerActive, 0, sizeof(remotePlayerActive));
    netPlayerCount=0;
    lastServerTick=0;
    lastAckedEventSeq=0;
    hasPendingAck=false;

    // Send connect request
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

        int32_t bytes=Network_SocketReceive(clientSocket, recvBuffer, sizeof(recvBuffer),
            &fromAddress, &fromPort);

        if(bytes<=0)
            break;

        if(fromAddress!=serverAddress||fromPort!=serverPort)
            continue;

        uint32_t magic=Deserialize_uint32(&pBuffer);

        switch(magic)
        {
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

    SendStatus(now);
}

bool ClientNetwork_IsConnected(void)
{
    return clientSocket!=-1&&connected;
}
