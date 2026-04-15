#ifndef EVENT_STATE_H
#define EVENT_STATE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_USERS 64
#define MAX_MINES 64
#define NAME_SIZE 32
#define JSON_LOG_SIZE 512

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

typedef struct {
    uint16_t playerId;
    uint16_t team;
    char firstName[NAME_SIZE];
    char lastName[NAME_SIZE];
    bool isDead;
    bool inUse;
} User;

typedef struct {
    uint16_t mineId;
    bool isActive;
    bool isFind;
    Vector3 pos;
    bool inUse;
} Mine;

typedef struct {
    uint16_t playerId;
    uint16_t team;
    const char* firstName;
    const char* lastName;
    bool isDead;
} CreatePlayerOpts;

typedef struct {
    uint16_t mineId;
    bool isActive;
    Vector3 pos;
} CreateMineOpts;

void EventState_Init(void);

bool Event_CreatePlayer(CreatePlayerOpts opts, char* jsonOut, uint16_t jsonOutSize);
bool Event_CreateMine(CreateMineOpts opts, char* jsonOut, uint16_t jsonOutSize);
bool Event_Explosion(uint16_t playerId, bool isDead, uint16_t mineId, bool isActive, char* jsonOut, uint16_t jsonOutSize);
bool Event_Find(uint16_t playerId, uint16_t mineId, bool isFind, char* jsonOut, uint16_t jsonOutSize);

User* Event_GetUserById(uint16_t playerId);
Mine* Event_GetMineById(uint16_t mineId);

#endif