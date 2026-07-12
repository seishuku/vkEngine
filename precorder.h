#ifndef __PRECORDER_H__
#define __PRECORDER_H__

#include <stdint.h>
#include <stdbool.h>
#include "physics/physics.h"

// File format (little-endian, all fields written explicitly, no raw struct dumps):
//
//   Header:
//     uint8_t  magic[4]      'P','R','E','C'
//     uint32_t version       currently 2
//     float    fixedTimestep
//
//   Per frame:
//     uint32_t frameIndex
//     uint32_t entityCount
//       per entity:
//         uint32_t id
//         uint8_t  objType       your game-level object type (player/field/projectile/etc)
//         uint8_t  type          RigidBodyType_e
//         float    px, py, pz
//         float    qx, qy, qz, qw
//         float    d0, d1, d2    size.xyz OR radius,0,0 OR radiusHeight.xy,0
//         float    wx, wy, wz    angular velocity (rad/s), for spin-axis visualization
//     uint32_t contactCount
//       per contact:
//         uint32_t idA, idB
//         float    px, py, pz
//         float    nx, ny, nz
//         float    penetration
//
//   Trailer (written once at Shutdown, lets playback seek without scanning):
//     uint64_t frameOffsets[frameCount]   file offset of each frame's `frameIndex` field
//     uint64_t indexOffset                offset where frameOffsets[] starts
//     uint32_t frameCount

// Starts a new recording. Truncates/creates the file at `path`.
// Returns false on failure to open the file.
bool PhysicsRecorder_Init(const char *path, float fixedTimestep);

// Bracket one physics frame's worth of logging.
// Call BeginFrame first, then LogEntity/LogContact any number of times, then EndFrame.
// All calls are no-ops if recording is disabled (see SetEnabled), so they're
// cheap to leave in place at the call sites permanently.
void PhysicsRecorder_BeginFrame(uint32_t frameIndex);
void PhysicsRecorder_LogEntity(uint32_t id, uint8_t objType, RigidBodyType_e type, vec3 position, vec4 orientation, vec3 dims, vec3 angularVelocity);
void PhysicsRecorder_LogContact(uint32_t idA, uint32_t idB, vec3 position, vec3 normal, float penetration);
void PhysicsRecorder_EndFrame(void);

// Flushes remaining data, writes the seek index/trailer, and closes the file.
// Safe to call even if Init was never called or already shut down.
void PhysicsRecorder_Shutdown(void);

// Runtime on/off switch — flip this instead of removing call sites.
void PhysicsRecorder_SetEnabled(bool enabled);
bool PhysicsRecorder_IsEnabled(void);

#endif
