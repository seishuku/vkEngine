#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "system/threads.h"
#include "utils/lz4.h"
#include "utils/serial.h"
#include "math/math.h"
#include "camera/camera.h"
#include "physics/physics.h"
#include "network/network.h"
#include "client_network.h"

#define NUM_ASTEROIDS 1000
extern RigidBody_t asteroids[NUM_ASTEROIDS];

extern Camera_t camera;

// Network stuff
static const uint32_t CONNECT_PACKETMAGIC   ='C'|'o'<<8|'n'<<16|'n'<<24; // "Conn"
static const uint32_t DISCONNECT_PACKETMAGIC='D'|'i'<<8|'s'<<16|'C'<<24; // "DisC"
static const uint32_t STATUS_PACKETMAGIC    ='S'|'t'<<8|'a'<<16|'t'<<24; // "Stat"
static const uint32_t FIELD_PACKETMAGIC     ='F'|'e'<<8|'l'<<16|'d'<<24; // "Feld"

#define MAX_CLIENTS 16

// PacketMagic determines packet type:
//
// Connect:
//		Client only needs to send connect magic.
//		Server responds back with connect magic and current random seed and client ID.
// Disconnect:
//		Client sends disconnect magic and client ID.
//		Server closes socket and removes client from list.
// Status:
//		Client sends status magic and current camera data, also serves as a keep-alive to the server.
//		Server sends status magic and *all* current connected client cameras.
// Field:
//		Server sends current play field (as it sees it) to all connected clients at a regular interval.
//
//
// THIS IS ALL VERY INSECURE AND DANGERUS

uint32_t serverAddress;
const uint16_t serverPort=4545;

uint32_t clientID=0;

Socket_t clientSocket=-1;

uint32_t connectedClients=0;
Camera_t netCameras[MAX_CLIENTS];

uint32_t currentSeed=0;

static ThreadWorker_t threadNetUpdate;
static bool netUpdateRun=true;
static uint8_t netBuffer[65536]={ 0 };
static uint8_t statusBuffer[1024]={ 0 };

static void NetUpdate(void *arg)
{
	memset(netCameras, 0, sizeof(Camera_t)*MAX_CLIENTS);

	if(clientSocket==-1)
	{
		netUpdateRun=false;
		return;
	}

	while(netUpdateRun)
	{
		uint8_t *pBuffer=NULL;
		uint32_t address=0;
		uint16_t port=0;

		memset(netBuffer, 0, sizeof(netBuffer));
		pBuffer=netBuffer;

		int32_t bytesRec=Network_SocketReceive(clientSocket, netBuffer, sizeof(netBuffer), &address, &port);

		if(bytesRec>0)
		{
			uint32_t magic=Deserialize_uint32(&pBuffer);

			if(magic==STATUS_PACKETMAGIC)
			{
				connectedClients=Deserialize_uint32(&pBuffer);

				if(connectedClients>MAX_CLIENTS)
				{
					DBGPRINTF(DEBUG_ERROR, "Mangled status packet: connected clients field > max clients (got %d, max %d).\n", connectedClients, MAX_CLIENTS);
					goto error;
				}

				for(uint32_t i=0;i<connectedClients;i++)
				{
					uint32_t clientID=Deserialize_uint32(&pBuffer);

					if(clientID>MAX_CLIENTS)
					{
						DBGPRINTF(DEBUG_ERROR, "Mangled status packet: client ID field > MAX_CLIENTS (got %d, max %d).\n", clientID, MAX_CLIENTS);
						goto error;
					}

					CameraInit(&netCameras[clientID], Vec3b(0.0f), Vec3b(0.0f), Vec3b(0.0f));

					netCameras[clientID].body.position=Deserialize_vec3(&pBuffer);
					netCameras[clientID].body.velocity=Deserialize_vec3(&pBuffer);
					netCameras[clientID].body.orientation=Deserialize_vec4(&pBuffer);

					//DBGPRINTF(DEBUG_INFO, "\033[%d;0H\033[KID %d Pos: %0.1f %0.1f %0.1f", clientID+1, clientID, NetCameras[clientID].position.x, NetCameras[clientID].position.y, NetCameras[clientID].position.z);
				}
			}
			else if(magic==FIELD_PACKETMAGIC)
			{
				uint32_t asteroidCount=Deserialize_uint32(&pBuffer);

				if(asteroidCount>NUM_ASTEROIDS)
				{
					DBGPRINTF(DEBUG_ERROR, "Mangled field packet: asteroid count field > NUM_ASTEROIDS (got %d, max %d).\n", asteroidCount, NUM_ASTEROIDS);
					goto error;
				}

				for(uint32_t i=0;i<asteroidCount;i++)
				{
					asteroids[i].position=Deserialize_vec3(&pBuffer);
					asteroids[i].velocity=Deserialize_vec3(&pBuffer);
					asteroids[i].orientation=Deserialize_vec4(&pBuffer);
					asteroids[i].radius=Deserialize_float(&pBuffer);
				}
			}
			else if(magic==DISCONNECT_PACKETMAGIC)
				ClientNetwork_Destroy();
		}

error:
		continue;
	}
}

bool ClientNetwork_Init(uint32_t address)
{
	// Initialize the network API (mainly for winsock)
	Network_Init();

	// Create a new socket
	clientSocket=Network_CreateSocket();

	if(clientSocket==-1)
		return false;

	serverAddress=address;

	// Send connect magic to initiate connection
	uint32_t magic=CONNECT_PACKETMAGIC;
	if(!Network_SocketSend(clientSocket, (uint8_t *)&magic, sizeof(uint32_t), serverAddress, serverPort))
		return false;

	double timeout=GetClock()+5.0; // Current time +5 seconds
	bool response=false;

	while(!response)
	{
		uint8_t *pBuffer=NULL;
		uint32_t address=0;
		uint16_t port=0;

		memset(&statusBuffer, 0, sizeof(statusBuffer));
		pBuffer=statusBuffer;

		int32_t bytesRec=Network_SocketReceive(clientSocket, statusBuffer, sizeof(statusBuffer), &address, &port);

		if(bytesRec>0)
		{
			uint32_t magic=Deserialize_uint32(&pBuffer);

			if(magic==CONNECT_PACKETMAGIC)
			{
				clientID=Deserialize_uint32(&pBuffer);

				if(clientID>MAX_CLIENTS)
				{
					DBGPRINTF(DEBUG_ERROR, "Mangled connect packet: client ID field > max clients (got %d, max %d).\n", clientID, MAX_CLIENTS);
					Network_SocketClose(clientSocket);

					return false;
				}

				currentSeed=Deserialize_uint32(&pBuffer);
				RandomSeed(currentSeed);

				response=true;

				DBGPRINTF(DEBUG_INFO, "Response from server - ID: %d Seed: %d Address: 0x%X Port: %d\n",
						  clientID, currentSeed, address, port);
			}
		}

		if(GetClock()>timeout)
		{
			DBGPRINTF(DEBUG_WARNING, "Connection timed out...\n");

			Network_SocketClose(clientSocket);
			clientSocket=-1;

			return false;
		}
	}

	Thread_Init(&threadNetUpdate);
	Thread_Start(&threadNetUpdate);
	Thread_AddJob(&threadNetUpdate, NetUpdate, NULL);

	return true;
}

void ClientNetwork_Destroy(void)
{
	netUpdateRun=false;
	Thread_Destroy(&threadNetUpdate);

	// Send disconnect message to server and close/destroy network stuff
	if(clientSocket!=-1)
	{
		uint8_t *pBuffer=statusBuffer;
		size_t iota=0;
		memset(&statusBuffer, 0, sizeof(statusBuffer));

		Serialize_uint32(&pBuffer, DISCONNECT_PACKETMAGIC);	iota+=sizeof(uint32_t);
		Serialize_uint32(&pBuffer, clientID);				iota+=sizeof(uint32_t);

		Network_SocketSend(clientSocket, statusBuffer, iota, serverAddress, serverPort);
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
		uint8_t *pBuffer=statusBuffer;
		size_t iota=0;
		memset(&netBuffer, 0, sizeof(statusBuffer));

		Serialize_uint32(&pBuffer, STATUS_PACKETMAGIC);		iota+=sizeof(uint32_t);
		Serialize_uint32(&pBuffer, clientID);				iota+=sizeof(uint32_t);
		Serialize_vec3(&pBuffer, camera.body.position);		iota+=sizeof(vec3);
		Serialize_vec3(&pBuffer, camera.body.velocity);		iota+=sizeof(vec3);
		Serialize_vec4(&pBuffer, camera.body.orientation);	iota+=sizeof(vec4);

		Network_SocketSend(clientSocket, statusBuffer, iota, serverAddress, serverPort);
	}
}
