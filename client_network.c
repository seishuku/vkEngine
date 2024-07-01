#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "system/threads.h"
#include "math/math.h"
#include "camera/camera.h"
#include "physics/physics.h"
#include "network/network.h"

#define NUM_ASTEROIDS 1000
extern RigidBody_t asteroids[NUM_ASTEROIDS];

extern Camera_t camera;

// Network stuff
#define CONNECT_PACKETMAGIC		('C'|('o'<<8)|('n'<<16)|('n'<<24)) // "Conn"
#define DISCONNECT_PACKETMAGIC	('D'|('i'<<8)|('s'<<16)|('C'<<24)) // "DisC"
#define STATUS_PACKETMAGIC		('S'|('t'<<8)|('a'<<16)|('t'<<24)) // "Stat"
#define FIELD_PACKETMAGIC		('F'|('e'<<8)|('l'<<16)|('d'<<24)) // "Feld"
#define MAX_CLIENTS 16

// PacketMagic determines packet type:
//
// Connect:
//		Client sends connect magic, server responds back with current random seed and slot.
// Disconnect:
//		Client sends disconnect magic, server closes socket and removes client from list.
// Status:
//		Client to server: Sends current camera data
//		Server to client: Sends all current connected client cameras.
// Field:
//		Server sends current play field (as it sees it) to all connected clients at a regular interval.

// Camera data for sending over the network
typedef struct
{
	vec3 position, velocity;
	vec3 forward, up;
} NetCamera_t;

// Connect data when connecting to server
typedef struct
{
	uint32_t seed;
	uint16_t port;
} NetConnect_t;

// Overall data network packet
typedef struct
{
	uint32_t packetMagic;
	uint32_t clientID;
	union
	{
		NetConnect_t connect;
		NetCamera_t camera;
	};
} NetworkPacket_t;

uint32_t serverAddress=NETWORK_ADDRESS(192, 168, 1, 10);
uint16_t serverPort=4545;

uint16_t clientPort=0;
uint32_t clientID=0;

Socket_t clientSocket=-1;

uint32_t connectedClients=0;
Camera_t netCameras[MAX_CLIENTS];

ThreadWorker_t threadNetUpdate;
bool NetUpdate_Run=true;
uint8_t NetBuffer[32767]={ 0 };

void NetUpdate(void *arg)
{
	memset(netCameras, 0, sizeof(Camera_t)*MAX_CLIENTS);

	if(clientSocket==-1)
	{
		NetUpdate_Run=false;
		return;
	}

	while(NetUpdate_Run)
	{
		uint8_t *pBuffer=NetBuffer;
		uint32_t Magic=0;
		uint32_t Address=0;
		uint16_t port=0;

		Network_SocketReceive(clientSocket, NetBuffer, sizeof(NetBuffer), &Address, &port);

		memcpy(&Magic, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

		if(Magic==STATUS_PACKETMAGIC)
		{
			memcpy(&connectedClients, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<connectedClients;i++)
			{
				uint32_t clientID=0;

				memcpy(&clientID, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

				memcpy(&netCameras[clientID].position, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].velocity, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].forward, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].up, pBuffer, sizeof(float)*3);			pBuffer+=sizeof(float)*3;
				netCameras[clientID].right=Vec3_Cross(netCameras[clientID].up, netCameras[clientID].forward);
				netCameras[clientID].radius=10.0f;

				//DBGPRINTF(DEBUG_INFO, "\033[%d;0H\033[KID %d Pos: %0.1f %0.1f %0.1f", clientID+1, clientID, NetCameras[clientID].position.x, NetCameras[clientID].position.y, NetCameras[clientID].position.z);
			}
		}
		else if(Magic==FIELD_PACKETMAGIC)
		{
			uint32_t asteroidCount=NUM_ASTEROIDS;

			memcpy(&asteroidCount, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<asteroidCount;i++)
			{
				memcpy(&asteroids[i].position, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
				memcpy(&asteroids[i].velocity, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
			}
		}
	}
}

bool ClientNetwork_Init(void)
{
	// Initialize the network API (mainly for winsock)
	Network_Init();

	// Create a new socket
	clientSocket=Network_CreateSocket();

	if(clientSocket==-1)
		return false;

	// Send connect magic to initiate connection
	uint32_t Magic=CONNECT_PACKETMAGIC;
	if(!Network_SocketSend(clientSocket, (uint8_t *)&Magic, sizeof(uint32_t), serverAddress, serverPort))
		return false;

	double Timeout=GetClock()+5.0; // Current time +5 seconds
	bool Response=false;

	while(!Response)
	{
		uint32_t Address=0;
		uint16_t port=0;
		NetworkPacket_t ResponsePacket;

		memset(&ResponsePacket, 0, sizeof(NetworkPacket_t));

		if(Network_SocketReceive(clientSocket, (uint8_t *)&ResponsePacket, sizeof(NetworkPacket_t), &Address, &port)>0)
		{
			if(ResponsePacket.packetMagic==CONNECT_PACKETMAGIC)
			{
				DBGPRINTF(DEBUG_INFO, "Response from server - ID: %d Seed: %d Port: %d Address: 0x%X Port: %d\n",
						  ResponsePacket.clientID,
						  ResponsePacket.connect.seed,
						  ResponsePacket.connect.port,
						  Address,
						  port);

				RandomSeed(ResponsePacket.connect.seed);
				clientID=ResponsePacket.clientID;
				Response=true;
			}
		}

		if(GetClock()>Timeout)
		{
			DBGPRINTF(DEBUG_WARNING, "Connection timed out...\n");
			Network_SocketClose(clientSocket);
			clientSocket=-1;
			break;
		}
	}

	Thread_Init(&threadNetUpdate);
	Thread_Start(&threadNetUpdate);
	Thread_AddJob(&threadNetUpdate, NetUpdate, NULL);

	return true;
}

void ClientNetwork_Destroy(void)
{
	NetUpdate_Run=false;
	Thread_Destroy(&threadNetUpdate);

	// Send disconnect message to server and close/destroy network stuff
	if(clientSocket!=-1)
	{
		Network_SocketSend(clientSocket, (uint8_t *)&(NetworkPacket_t)
		{
			.packetMagic=DISCONNECT_PACKETMAGIC, .clientID=clientID
		}, sizeof(NetworkPacket_t), serverAddress, serverPort);
		Network_SocketClose(clientSocket);

		clientSocket=-1;
		connectedClients=0;
	}

	Network_Destroy();
}

// Network status packet
void ClientNetwork_SendStatus(void)
{
	if(clientSocket!=-1)
	{
		NetworkPacket_t StatusPacket;

		memset(&StatusPacket, 0, sizeof(NetworkPacket_t));

		StatusPacket.packetMagic=STATUS_PACKETMAGIC;
		StatusPacket.clientID=clientID;

		StatusPacket.camera.position=camera.position;
		StatusPacket.camera.velocity=camera.velocity;
		StatusPacket.camera.forward=camera.forward;
		StatusPacket.camera.up=camera.up;

		Network_SocketSend(clientSocket, (uint8_t *)&StatusPacket, sizeof(NetworkPacket_t), serverAddress, serverPort);
	}
}
