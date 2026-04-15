#include "event_state.h"
#include <string.h>
#include <stdio.h>

static User users[MAX_USERS];
static Mine mines[MAX_MINES];

static void copyString(char* dst, const char* src, uint16_t size)
{
    if (size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static int findUserIndex(uint16_t playerId)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].inUse && users[i].playerId == playerId) {
            return i;
        }
    }

    return -1;
}

static int findFreeUserIndex(void)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (!users[i].inUse) {
            return i;
        }
    }

    return -1;
}

static int findMineIndex(uint16_t mineId)
{
    for (int i = 0; i < MAX_MINES; i++) {
        if (mines[i].inUse && mines[i].mineId == mineId) {
            return i;
        }
    }

    return -1;
}

static int findFreeMineIndex(void)
{
    for (int i = 0; i < MAX_MINES; i++) {
        if (!mines[i].inUse) {
            return i;
        }
    }

    return -1;
}

static void escapeJsonString(const char* src, char* dst, size_t dstSize)
{
    size_t j = 0;

    if (dstSize == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (size_t i = 0; src[i] != '\0' && j < dstSize - 1; i++) {
        char c = src[i];

        if ((c == '"' || c == '\\') && j + 2 < dstSize) {
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            if (j + 2 < dstSize) {
                dst[j++] = ' ';
            } else {
                break;
            }
        } else {
            dst[j++] = c;
        }
    }

    dst[j] = '\0';
}

static void buildCreatePlayerJson(const User* user, char* jsonOut, uint16_t jsonOutSize)
{
    char firstNameEsc[NAME_SIZE * 2];
    char lastNameEsc[NAME_SIZE * 2];
    char msg[160];
    char msgEsc[320];

    escapeJsonString(user->firstName, firstNameEsc, sizeof(firstNameEsc));
    escapeJsonString(user->lastName, lastNameEsc, sizeof(lastNameEsc));

    snprintf(
        msg,
        sizeof(msg),
        "Nouveau joueur %s %s ajoute dans l'equipe %u",
        user->firstName,
        user->lastName,
        user->team
    );

    escapeJsonString(msg, msgEsc, sizeof(msgEsc));

    snprintf(
        jsonOut,
        jsonOutSize,
        "{\"event\":{\"action\":\"create_player\"},\"player\":{\"id\":%u,\"team\":%u,\"first_name\":\"%s\",\"last_name\":\"%s\",\"is_dead\":%s},\"message\":\"%s\"}\n",
        user->playerId,
        user->team,
        firstNameEsc,
        lastNameEsc,
        user->isDead ? "true" : "false",
        msgEsc
    );
}

static void buildCreateMineJson(const Mine* mine, char* jsonOut, uint16_t jsonOutSize)
{
    char msg[160];
    char msgEsc[320];

    snprintf(
        msg,
        sizeof(msg),
        "Nouvelle mine %u ajoutee en position %.2f %.2f %.2f",
        mine->mineId,
        mine->pos.x,
        mine->pos.y,
        mine->pos.z
    );

    escapeJsonString(msg, msgEsc, sizeof(msgEsc));

    snprintf(
        jsonOut,
        jsonOutSize,
        "{\"event\":{\"action\":\"create_mine\"},\"mine\":{\"id\":%u,\"active\":%s,\"found\":%s,\"position\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}},\"message\":\"%s\"}\n",
        mine->mineId,
        mine->isActive ? "true" : "false",
        mine->isFind ? "true" : "false",
        mine->pos.x,
        mine->pos.y,
        mine->pos.z,
        msgEsc
    );
}

static void buildExplosionJson(const User* user, const Mine* mine, char* jsonOut, uint16_t jsonOutSize)
{
    char firstNameEsc[NAME_SIZE * 2];
    char lastNameEsc[NAME_SIZE * 2];
    char msg[192];
    char msgEsc[384];

    escapeJsonString(user->firstName, firstNameEsc, sizeof(firstNameEsc));
    escapeJsonString(user->lastName, lastNameEsc, sizeof(lastNameEsc));

    snprintf(
        msg,
        sizeof(msg),
        "Explosion detectee sur la mine %u par %s %s",
        mine->mineId,
        user->firstName,
        user->lastName
    );

    escapeJsonString(msg, msgEsc, sizeof(msgEsc));

    snprintf(
        jsonOut,
        jsonOutSize,
        "{\"event\":{\"action\":\"explosion\"},\"player\":{\"id\":%u,\"team\":%u,\"first_name\":\"%s\",\"last_name\":\"%s\",\"is_dead\":%s},\"mine\":{\"id\":%u,\"active\":%s,\"found\":%s,\"position\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}},\"message\":\"%s\"}\n",
        user->playerId,
        user->team,
        firstNameEsc,
        lastNameEsc,
        user->isDead ? "true" : "false",
        mine->mineId,
        mine->isActive ? "true" : "false",
        mine->isFind ? "true" : "false",
        mine->pos.x,
        mine->pos.y,
        mine->pos.z,
        msgEsc
    );
}

static void buildFindJson(const User* user, const Mine* mine, char* jsonOut, uint16_t jsonOutSize)
{
    char firstNameEsc[NAME_SIZE * 2];
    char lastNameEsc[NAME_SIZE * 2];
    char msg[192];
    char msgEsc[384];

    escapeJsonString(user->firstName, firstNameEsc, sizeof(firstNameEsc));
    escapeJsonString(user->lastName, lastNameEsc, sizeof(lastNameEsc));

    snprintf(
        msg,
        sizeof(msg),
        "Mine %u localisee par %s %s",
        mine->mineId,
        user->firstName,
        user->lastName
    );

    escapeJsonString(msg, msgEsc, sizeof(msgEsc));

    snprintf(
        jsonOut,
        jsonOutSize,
        "{\"event\":{\"action\":\"find\"},\"player\":{\"id\":%u,\"team\":%u,\"first_name\":\"%s\",\"last_name\":\"%s\",\"is_dead\":%s},\"mine\":{\"id\":%u,\"active\":%s,\"found\":%s,\"position\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}},\"message\":\"%s\"}\n",
        user->playerId,
        user->team,
        firstNameEsc,
        lastNameEsc,
        user->isDead ? "true" : "false",
        mine->mineId,
        mine->isActive ? "true" : "false",
        mine->isFind ? "true" : "false",
        mine->pos.x,
        mine->pos.y,
        mine->pos.z,
        msgEsc
    );
}

void EventState_Init(void)
{
    memset(users, 0, sizeof(users));
    memset(mines, 0, sizeof(mines));
}

User* Event_GetUserById(uint16_t playerId)
{
    int index = findUserIndex(playerId);

    if (index < 0) {
        return NULL;
    }

    return &users[index];
}

Mine* Event_GetMineById(uint16_t mineId)
{
    int index = findMineIndex(mineId);

    if (index < 0) {
        return NULL;
    }

    return &mines[index];
}

bool Event_CreatePlayer(CreatePlayerOpts opts, char* jsonOut, uint16_t jsonOutSize)
{
    if (opts.firstName == NULL || opts.lastName == NULL || jsonOut == NULL || jsonOutSize == 0) {
        return false;
    }

    int index = findUserIndex(opts.playerId);

    if (index < 0) {
        index = findFreeUserIndex();
        if (index < 0) {
            return false;
        }
    }

    users[index].playerId = opts.playerId;
    users[index].team = opts.team;
    copyString(users[index].firstName, opts.firstName, NAME_SIZE);
    copyString(users[index].lastName, opts.lastName, NAME_SIZE);
    users[index].isDead = opts.isDead;
    users[index].inUse = true;

    buildCreatePlayerJson(&users[index], jsonOut, jsonOutSize);
    return true;
}

bool Event_CreateMine(CreateMineOpts opts, char* jsonOut, uint16_t jsonOutSize)
{
    if (jsonOut == NULL || jsonOutSize == 0) {
        return false;
    }

    int index = findMineIndex(opts.mineId);

    if (index < 0) {
        index = findFreeMineIndex();
        if (index < 0) {
            return false;
        }
    }

    mines[index].mineId = opts.mineId;
    mines[index].isActive = opts.isActive;
    mines[index].isFind = false;
    mines[index].pos = opts.pos;
    mines[index].inUse = true;

    buildCreateMineJson(&mines[index], jsonOut, jsonOutSize);
    return true;
}

bool Event_Explosion(uint16_t playerId, bool isDead, uint16_t mineId, bool isActive, char* jsonOut, uint16_t jsonOutSize)
{
    if (jsonOut == NULL || jsonOutSize == 0) {
        return false;
    }

    User* user = Event_GetUserById(playerId);
    Mine* mine = Event_GetMineById(mineId);

    if (user == NULL || mine == NULL) {
        return false;
    }

    user->isDead = isDead;
    mine->isActive = isActive;

    buildExplosionJson(user, mine, jsonOut, jsonOutSize);
    return true;
}

bool Event_Find(uint16_t playerId, uint16_t mineId, bool isFind, char* jsonOut, uint16_t jsonOutSize)
{
    if (jsonOut == NULL || jsonOutSize == 0) {
        return false;
    }

    User* user = Event_GetUserById(playerId);
    Mine* mine = Event_GetMineById(mineId);

    if (user == NULL || mine == NULL) {
        return false;
    }

    mine->isFind = isFind;

    buildFindJson(user, mine, jsonOut, jsonOutSize);
    return true;
}