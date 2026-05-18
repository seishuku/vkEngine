#ifndef __ASTEROIDS_H__
#define __ASTEROIDS_H__

#define MAX_ASTEROIDS 5000

typedef struct { uint32_t modelID, tex0ID, tex1ID; } AsteroidModel_t;

extern uint32_t numAsteroids;
extern RigidBody_t asteroids[MAX_ASTEROIDS];
extern AsteroidModel_t asteroidModels[MAX_ASTEROIDS];

void ResetAsteroids(void);
void AddAsteroid(vec3 position, vec3 velocity, float radius, uint32_t variant);
void SplitAsteroid(uint32_t index, ContactPoint_t contact);

#endif
