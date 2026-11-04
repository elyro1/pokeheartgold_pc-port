#include "global.h"

#include "constants/sndseq.h"

#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0309.h"
#include "pokeathlon/pokeathlon_course_types_def.h"
#include "pokeathlon/pokeathlon_pennant_capture.h"

#include "assert.h"
#include "bg_window.h"
#include "font.h"
#include "geometry_math.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "message_format.h"
#include "msgdata.h"
#include "pm_string.h"
#include "sound_playback_internal.h"
#include "text.h"

u8 *PokeathlonCourse_GetPerformance(PokeathlonCourseData *course, int participant, u16 member);
u8 *PokeathlonCourse_GetMonStats(PokeathlonCourseData *course, int participant, u8 member);
void PokeathlonCourse_AddMonCounter(PokeathlonCourseData *course, u8 participant, u8 member, enum PokeathlonMonCounter counter, int amount);
void PokeathlonCourse_SetRecordScore(PokeathlonCourseData *course, int score);
void PokeathlonCourseMonObject_SetDrawFlag(PokeathlonCourseMonObject *object, int visible);
void PokeathlonCourseMonObject_SetDrawPriority(PokeathlonCourseMonObject *object, int priority);
void PokeathlonCourseMonObject_SetAnimStateRaw(PokeathlonCourseMonObject *object, u8 size);
void PokeathlonCourseMonObject_SetAnimState(PokeathlonCourseMonObject *object, u8 size);
void PokeathlonCourseMonObject_SetAnim(PokeathlonCourseMonObject *object, int anim);
void PokeathlonCourseObject_GetCollisionCircle(PokeathlonCourseMonObject *object, int x, int y, int *outX, int *outY, int *outRadius);
int PokeathlonCourseObject_GetCollisionRadius(PokeathlonCourseMonObject *object);
void PokeathlonCourseObject_SetGroundPosition(PokeathlonCourseMonObject *object, int x, int y);
void PokeathlonCourseObject_GroundToCenter(PokeathlonCourseMonObject *object, int x, int y, int *outX, int *outY);
void PokeathlonCourseSpritePoolSprite_Show(PokeathlonCourseSpritePoolSprite *sprite, BOOL shown, BOOL show);
void PokeathlonCourseSpritePoolSprite_SetAnim(PokeathlonCourseSpritePoolSprite *sprite, int anim);
void PokeathlonCourseSpritePoolSprite_SetPosition(PokeathlonCourseSpritePoolSprite *sprite, VecFx32 *position);
VecFx32 *PokeathlonCourseSpritePoolSprite_GetPosition(PokeathlonCourseSpritePoolSprite *sprite);
void PokeathlonCourseSpritePoolSprite_SetDrawPriority(PokeathlonCourseSpritePoolSprite *sprite, int priority);

typedef struct PokeathlonPennantCaptureCollisionFrame {
    int bY;
    int bX;
    int aY;
    int aX;
    int radiusB;
    int radiusA;
    VecFx32 resolved;
    VecFx32 zeroB;
    VecFx32 zeroA;
    VecFx32 diff;
    VecFx32 pointB;
    VecFx32 pointA;
} PokeathlonPennantCaptureCollisionFrame;

static void PokeathlonPennantCapture_TickFreezeTimer(PokeathlonCourseData *course, PokeathlonPennantCapture *work, PokeathlonPennantCapturePlayer *player);
static void PokeathlonPennantCapture_UpdateMonSwap(PokeathlonPennantCaptureDisplay *display, PokeathlonPennantCapturePlayer *player, PokeathlonPennantCaptureFrame *frame, u8 participant, u8 member, u8 *block, int isHost, int isSelf, int remote);
static int PokeathlonPennantCapture_CollideWithObstacles(PokeathlonPennantCapture *work, int excluded, int radius, VecFx32 *position, VecFx32 *end, VecFx32 *out);
static fx32 PokeathlonPennantCapture_CalcCircleEntry(VecFx32 *a, VecFx32 *b, VecFx32 *point, fx32 radius);
static int PokeathlonPennantCapture_CollidePlayers(PokeathlonCourseData *course, PokeathlonPennantCapturePlayer *players, VecFx32 *out);
static void PokeathlonPennantCapture_SetPennantDrawPriority(PokeathlonPennantCapturePennant *pennant, int y);
static void PokeathlonPennantCapture_SetDropEffectDrawPriority(PokeathlonPennantCapture *work);
static void PokeathlonPennantCapture_SetPlayerDrawPriority(PokeathlonPennantCapturePlayer *player, int y);
static BOOL PokeathlonPennantCapture_PickUpPennants(PokeathlonPennantCapturePennants *pennants, PokeathlonPennantCapturePlayer *player, u8 *full);
static BOOL PokeathlonPennantCapture_CirclesOverlap(const VecFx32 *a, const VecFx32 *b, fx32 radiusA, fx32 radiusB);
static BOOL PokeathlonPennantCapture_TryKnockDown(PokeathlonCourseData *course, PokeathlonPennantCapturePlayer *a, PokeathlonPennantCapturePlayer *b);
static BOOL PokeathlonPennantCapture_CanDeposit(PokeathlonPennantCapturePlayer *player);
static u8 PokeathlonPennantCapture_GetDepositPoints(u8 pennants);
static int PokeathlonPennantCapture_GetDrawPriority(int y);
static void PokeathlonPennantCapture_PlaySEIfOnScreen(int x, int y, int seq, BOOL self);

void PokeathlonPennantCapture_UpdatePlayers(PokeathlonCourseData *course) {
    PokeathlonPennantCapture *work = PokeathlonCourse_GetWork(course);
    u8 handled[4];
    int i;
    u8 *flag;
    PokeathlonPennantCapturePlayer *player;
    VecFx32 hit;
    VecFx32 previous;
    VecFx32 friction;

    work->hitReported = 0;
    i = 0;
    flag = handled;
    for (; i < POKEATHLON_PARTICIPANTS; flag++, i++) {
        *flag = 0;
        if (work->players[i].freezeTimer) {
            PokeathlonPennantCapture_TickFreezeTimer(course, work, &work->players[i]);
            *flag = 1;
        }
    }
    player = work->players;
    for (i = 0; i < POKEATHLON_PARTICIPANTS; i++, player++) {
        if (handled[i]) {
            continue;
        }
        if (PokeathlonPennantCapture_CanDeposit(player)) {
            player->pointsToAdd = PokeathlonPennantCapture_GetDepositPoints(player->pennants);
            player->pennants = 0;
            player->depositing = 1;
            player->velocity.x = 0;
            player->velocity.y = 0;
            player->velocity.z = 0;
            player->direction = 2;
            player->depositSteps = 10;
            PokeathlonCourse_AddMonCounter(course, player->participant, player->member, POKEATHLON_COUNTER_POINTS, player->pointsToAdd);
        }
        if (player->depositSteps && player->depositing) {
            if (player->depositDelay == 0) {
                if (player->pointsToAdd) {
                    player->score++;
                    if (player->score > POKEATHLON_SCORE_MAX) {
                        player->score = POKEATHLON_SCORE_MAX;
                    }
                    player->pointsToAdd--;
                }
                player->depositSteps--;
                if (player->depositSteps == 0) {
                    player->depositSteps = 0;
                    player->swapPending = 1;
                    player->swapPhase = 1;
                    PokeathlonCourse_AddMonCounter(course, player->participant, player->member, POKEATHLON_COUNTER_SWITCHES, 1);
                } else {
                    player->depositDelay = 4;
                }
            } else {
                player->depositDelay--;
            }
        }
        if (player->depositing) {
            continue;
        }
        previous = player->position;
        VEC_Add(&player->position, &player->velocity, &player->position);
        // the goal opening at the bottom is narrower than the field
        if (player->position.x < FX32_CONST(208) || player->position.x > FX32_CONST(304)) {
            if (player->position.y < FX32_CONST(104)) {
                player->position.y = FX32_CONST(104);
            } else if (player->position.y > FX32_CONST(392)) {
                player->position.y = FX32_CONST(392);
            }
        } else {
            if (player->position.y < FX32_CONST(104)) {
                player->position.y = FX32_CONST(104);
            } else if (player->position.y > FX32_CONST(416)) {
                player->position.y = FX32_CONST(416);
            }
        }
        if (player->position.y > FX32_CONST(392)) {
            if (player->position.x < FX32_CONST(208)) {
                player->position.x = FX32_CONST(208);
            } else if (player->position.x > FX32_CONST(304)) {
                player->position.x = FX32_CONST(304);
            }
        } else {
            if (player->position.x < FX32_CONST(144)) {
                player->position.x = FX32_CONST(144);
            } else if (player->position.x > FX32_CONST(368)) {
                player->position.x = FX32_CONST(368);
            }
        }
        player->wallSide = (u8)PokeathlonPennantCapture_CollideWithObstacles(work, player->wallSide, PokeathlonCourseObject_GetCollisionRadius(player->monObjects[player->member]), &previous, &player->position, &player->position);
        switch (player->wallSide) {
        case 1:
            if (player->velocity.y > 0) {
                player->velocity.y *= -0.3;
            }
            break;
        case 2:
            if (player->velocity.x < 0) {
                player->velocity.x *= -0.3;
            }
            break;
        case 3:
            if (player->velocity.y < 0) {
                player->velocity.y *= -0.3;
            }
            break;
        case 4:
            if (player->velocity.x > 0) {
                player->velocity.x *= -0.3;
            }
            break;
        case 5:
            if (player->velocity.x > 0) {
                player->velocity.x *= -0.3;
            }
            if (player->velocity.y > 0) {
                player->velocity.y *= -0.3;
            }
            break;
        case 6:
            if (player->velocity.x < 0) {
                player->velocity.x *= -0.3;
            }
            if (player->velocity.y > 0) {
                player->velocity.y *= -0.3;
            }
            break;
        case 7:
            if (player->velocity.x < 0) {
                player->velocity.x *= -0.3;
            }
            if (player->velocity.y < 0) {
                player->velocity.y *= -0.3;
            }
            break;
        case 8:
            if (player->velocity.x > 0) {
                player->velocity.x *= -0.3;
            }
            if (player->velocity.y < 0) {
                player->velocity.y *= -0.3;
            }
            break;
        }
        int collide = PokeathlonPennantCapture_CollidePlayers(course, work->players, &hit);
        if (!work->hitReported && collide) {
            work->hitReported = 1;
            work->hitX = hit.x / FX32_ONE;
            work->hitY = hit.y / FX32_ONE;
        }
        friction = player->velocity;
        if (VEC_Mag(&friction) > 0) {
            float force = 6.0f;
            int amount = 0.1 * force * 4096.0;
            if (work->heldTooLong) {
                amount += 0x3000;
            }
            VEC_Normalize(&friction, &friction);
            friction.x = ((s64)friction.x * amount + 2048) >> FX32_SHIFT;
            friction.y = ((s64)friction.y * amount + 2048) >> FX32_SHIFT;
            fx32 magnitude = VEC_Mag(&friction);
            if (VEC_Mag(&player->velocity) >= magnitude) {
                VEC_Subtract(&player->velocity, &friction, &player->velocity);
            } else {
                player->velocity.x = 0;
                player->velocity.y = 0;
            }
        }
        if (!player->swapPending && player->knockedDown == 1) {
            player->knockdownTimer--;
            if (player->knockdownTimer == 0) {
                player->knockedDown = 0;
            }
        }
        PokeathlonPennantCapture_PickUpPennants(&work->pennants, player, &work->pennantsFull[i]);
    }
}

static void PokeathlonPennantCapture_TickFreezeTimer(PokeathlonCourseData *course, PokeathlonPennantCapture *work, PokeathlonPennantCapturePlayer *player) {
    if (player->freezeTimer != 0) {
        player->freezeTimer--;
    }
}

typedef void (*PokeathlonPennantCaptureMonSwapFunc)(PokeathlonPennantCaptureDisplay *display, PokeathlonPennantCapturePlayer *player, PokeathlonPennantCaptureFrame *frame, u8 participant, u16 member, u8 *block, int isHost, int isSelf, int remote);
#define UpdateMonSwapAsU16 ((PokeathlonPennantCaptureMonSwapFunc)PokeathlonPennantCapture_UpdateMonSwap)

void PokeathlonPennantCapture_ApplyFrame(PokeathlonCourseData *course, int viewer, PokeathlonPennantCapturePlayer *players, PokeathlonPennantCaptureLinkData *link) {
    u32 baseX;
    u32 baseY;
    PokeathlonPennantCapture *work;
    PokeathlonPennantCaptureFrame *frame;
    PokeathlonCourseMonObject *object;
    u8 direction;
    u8 depositing;
    u8 knockedDown;
    int curY;
    int x;
    long px;
    u8 member;
    u8 self;
    long py;
    int isHost;
    int y;
    int curX;
    int i;
    int j;
    u8 *block;
    int remote;
    u8 pennants;
    u16 score;

    self = PokeathlonCourse_GetNetId(course);
    frame = (PokeathlonPennantCaptureFrame *)PokeathlonCourse_ToBytes(link->frame);
    work = PokeathlonCourse_GetWork(course);
    baseX = frame->words[viewer] & 0x1FF;
    baseY = (frame->words[viewer] >> 9) & 0x1FF;

    if ((u8)((frame->words[5] >> 18) & 1)) {
        x = frame->words[5] & 0x1FF;
        y = (frame->words[5] >> 9) & 0x1FF;
        work->pickupEffectX[work->nextPickupEffect] = x;
        work->pickupEffectY[work->nextPickupEffect] = y;
        PokeathlonCourseSpritePoolSprite_Show(work->pickupEffects[work->nextPickupEffect], TRUE, TRUE);
        PokeathlonCourseSpritePoolSprite_SetAnim(work->pickupEffects[work->nextPickupEffect], 7);
        work->nextPickupEffect = (work->nextPickupEffect + 1) % 4;
        PokeathlonPennantCapture_PlaySEIfOnScreen(x + 128 - baseX, y + 96 - baseY, SEQ_SE_GS_THLON06, TRUE);
    }
    if ((u8)(((u8)((frame->words[5] >> 19) & 0xF) >> self) & 1)) {
        PlaySEOnPlayer(SEQ_SE_GS_HATAMOTENAI, PLAYER_SE_1);
    }
    if ((u8)((frame->words[4] >> 18) & 1)) {
        x = frame->words[4] & 0x1FF;
        y = (frame->words[4] >> 9) & 0x1FF;
        work->dropEffectX[work->nextDropEffect] = x;
        work->dropEffectY[work->nextDropEffect] = y;
        PokeathlonCourseSpritePoolSprite_Show(work->dropEffects[work->nextDropEffect], TRUE, TRUE);
        PokeathlonCourseSpritePoolSprite_SetAnim(work->dropEffects[work->nextDropEffect], 4);
        work->nextDropEffect = (work->nextDropEffect + 1) % 4;
        PokeathlonPennantCapture_PlaySEIfOnScreen(x + 128 - baseX, y + 96 - baseY, SEQ_SE_GS_SAA, TRUE);
    }

    for (i = 0; i < 4; i++) {
        VecFx32 pickupPos = { 0, 0, 0 };
        y = (work->pickupEffectY[i] + 96 - baseY) << FX32_SHIFT;
        x = (work->pickupEffectX[i] + 128 - baseX) << FX32_SHIFT;
        pickupPos.x = x;
        pickupPos.y = y;
        PokeathlonCourseSpritePoolSprite_SetPosition(work->pickupEffects[i], &pickupPos);
    }
    for (i = 0; i < 4; i++) {
        VecFx32 dropPos = { 0, 0, 0 };
        y = (work->dropEffectY[i] + 96 - baseY) << FX32_SHIFT;
        x = (work->dropEffectX[i] + 128 - baseX) << FX32_SHIFT;
        dropPos.x = x;
        dropPos.y = y;
        PokeathlonCourseSpritePoolSprite_SetPosition(work->dropEffects[i], &dropPos);
    }
    PokeathlonPennantCapture_SetDropEffectDrawPriority(work);

    isHost = viewer == 0 ? TRUE : FALSE;

    for (i = 0; i < POKEATHLON_PARTICIPANTS; i++, players++) {
        PokeathlonPennantCapturePlayer *player = players;
        VecFx32 position;
        curX = frame->words[i] & 0x1FF;
        curY = (frame->words[i] >> 9) & 0x1FF;
        member = (frame->spots[i] >> 5) & 3;
        direction = (u8)((frame->spots[i] >> 7) & 3) + 1;
        knockedDown = (frame->spots[i] >> 9) & 1;
        depositing = ((u8)((frame->words[4] >> 19) & 0xF) >> i) & 1;
        object = player->monObjects[member];
        if (PokeathlonCourse_GetParticipantCount(course) <= i) {
            remote = TRUE;
            block = PokeathlonCourse_ToBytes(link->others[i]);
        } else {
            remote = FALSE;
            block = PokeathlonCourse_ToBytes(link->own);
        }
        if (viewer == i) {
            UpdateMonSwapAsU16(work->display, player, frame, i, member, block, isHost, TRUE, FALSE);
        } else {
            UpdateMonSwapAsU16(work->display, player, frame, i, member, block, isHost, FALSE, remote);
        }
        px = curX - baseX + 128;
        py = curY - baseY + 96;
        py += player->drawOffsetY;
        position.x = px << FX32_SHIFT;
        position.y = py << FX32_SHIFT;
        position.z = 0;
        PokeathlonPennantCaptureDisplay_UpdateMon(work->display, i, member, depositing, knockedDown);
        if (!depositing) {
            // BUG: unlike the other culls this needs only one of x and y on screen, so Pokemon level with the view
            // stay drawn.
            if ((px >= -32 && px <= 288) || (py >= -32 && py <= 224)) {
                PokeathlonCourseMonObject_SetDrawFlag(object, TRUE);
            } else {
                PokeathlonCourseMonObject_SetDrawFlag(object, FALSE);
            }
        }
        for (j = 0; j < POKEATHLON_TEAM_SIZE; j++) {
            PokeathlonCourseObject_SetGroundPosition(player->monObjects[j], px, py);
        }
        {
            VecFx32 carryPos = { 0, 0, 0 };
            carryPos.x = px << FX32_SHIFT;
            carryPos.y = py << FX32_SHIFT;
            if (PokeathlonCourse_GetPerformance(course, i, member)[5]) {
                carryPos.y -= FX32_CONST(24);
            } else {
                carryPos.y -= FX32_CONST(16);
            }
            PokeathlonCourseSpritePoolSprite_SetPosition(player->carrySprite, &carryPos);
        }
        PokeathlonPennantCapture_SetPlayerDrawPriority(player, py);
        if (!depositing) {
            remote = FALSE;
            if (knockedDown == TRUE) {
                if (knockedDown != player->active) {
                    PokeathlonCourseSpritePoolSprite_Show(player->carrySprite, TRUE, TRUE);
                }
                PokeathlonCourseMonObject_SetAnim(object, 21);
                player->flashTimer = 0;
                remote = TRUE;
                if (self == i) {
                    if (GF_SndCountPlayingSeqOnPlayer(PLAYER_SE_1) == 0) {
                        PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_GS_W146, TRUE);
                    }
                } else if (GF_SndCountPlayingSeqOnPlayer(PLAYER_SE_2) == 0) {
                    PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_GS_W146, FALSE);
                }
            } else {
                PokeathlonCourseSpritePoolSprite_Show(player->carrySprite, TRUE, FALSE);
            }
            if (!remote) {
                PokeathlonCourseMonObject_SetAnimStateRaw(object, direction);
                if (player->flashTimer) {
                    PokeathlonCourseMonObject_SetAnim(object, 26);
                    player->flashTimer--;
                } else {
                    PokeathlonCourseMonObject_SetAnim(object, 0);
                }
            }
        } else {
            PokeathlonCourseSpritePoolSprite_Show(player->carrySprite, TRUE, FALSE);
            PokeathlonCourseMonObject_SetAnimState(object, direction);
        }
        player->active = knockedDown;
        pennants = (frame->words[i] >> 28) & 0xF;
        PokeathlonPennantCaptureDisplay_SetPennants(work->display, i, pennants);
        if (player->shownPennants < pennants) {
            player->flashTimer = 10;
            PokeathlonCourseSpritePoolSprite_SetAnim(player->pennantSprite, 8);
            PokeathlonCourseSpritePoolSprite_Show(player->pennantSprite, TRUE, TRUE);
            if (self == i) {
                PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_DP_PIRORIRO, TRUE);
            } else {
                PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_DP_BOX02, FALSE);
            }
        } else if (!depositing && player->shownPennants > pennants) {
            PokeathlonCourseSpritePoolSprite_SetAnim(player->pennantSprite, 9);
            PokeathlonCourseSpritePoolSprite_Show(player->pennantSprite, TRUE, TRUE);
        }
        player->shownPennants = pennants;
        PokeathlonCourseSpritePoolSprite_SetPosition(player->pennantSprite, &position);
        score = (frame->words[i] >> 18) & 0x3FF;
        if (viewer == i) {
            PokeathlonPennantCaptureDisplay_SetScore(work->display, score);
        }
        if (player->shownScore < score) {
            PokeathlonCourseSpritePoolSprite_SetAnim(player->pointSprites[player->nextPointSprite], 3);
            PokeathlonCourseSpritePoolSprite_Show(player->pointSprites[player->nextPointSprite], TRUE, TRUE);
            player->nextPointSprite = (player->nextPointSprite + 1) % 2;
            PokeathlonPennantCaptureDisplay_ShowPoint(work->display, i);
            if (self == i) {
                PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_GS_POINT3, TRUE);
            } else {
                PokeathlonPennantCapture_PlaySEIfOnScreen(position.x / FX32_ONE, position.y / FX32_ONE, SEQ_SE_GS_POINT3, FALSE);
            }
        }
        for (pennants = 0; pennants < 2; pennants++) {
            PokeathlonCourseSpritePoolSprite_SetPosition(player->pointSprites[pennants], &position);
        }
        player->shownScore = score;
    }
}

void PokeathlonPennantCapture_UpdateObstacleSprites(PokeathlonPennantCapture *work, int x, int y) {
    u8 i;

    for (i = 0; i < PENNANT_CAPTURE_OBSTACLES; i++) {
        int dx;
        int dy;
        VecFx32 position;
        if (!work->obstacles[i].placed) {
            break;
        }
        dx = work->obstacles[i].x - x + 0x80;
        dy = work->obstacles[i].y - y + 0x60;
        position.z = 0;
        position.x = dx * FX32_ONE;
        position.y = dy * FX32_ONE;
        PokeathlonCourseSpritePoolSprite_SetPosition(work->obstacles[i].sprite, &position);
        if (dx >= -32 && dx <= 288 && dy >= -32 && dy <= 224) {
            PokeathlonCourseSpritePoolSprite_Show(work->obstacles[i].sprite, TRUE, TRUE);
        } else {
            PokeathlonCourseSpritePoolSprite_Show(work->obstacles[i].sprite, TRUE, FALSE);
        }
    }
}

void PokeathlonPennantCapture_UpdateFieldItemSprites(PokeathlonPennantCapture *work, int x, int y) {
    u8 i;

    for (i = 0; i < PENNANT_CAPTURE_FIELD_ITEMS; i++) {
        int dx;
        int dy;
        VecFx32 position;
        if (!work->fieldItems[i].placed) {
            break;
        }
        dx = work->fieldItems[i].x - x + 0x80;
        dy = work->fieldItems[i].y - y + 0x60;
        position.z = 0;
        position.x = dx * FX32_ONE;
        position.y = dy * FX32_ONE;
        PokeathlonCourseSpritePoolSprite_SetPosition(work->fieldItems[i].sprite, &position);
        PokeathlonCourseSpritePoolSprite_SetPosition(work->fieldItems[i].shadowSprite, &position);
        if (dx >= -32 && dx <= 288 && dy >= -32 && dy <= 224) {
            PokeathlonCourseSpritePoolSprite_Show(work->fieldItems[i].sprite, TRUE, TRUE);
            PokeathlonCourseSpritePoolSprite_Show(work->fieldItems[i].shadowSprite, TRUE, TRUE);
        } else {
            PokeathlonCourseSpritePoolSprite_Show(work->fieldItems[i].sprite, TRUE, FALSE);
            PokeathlonCourseSpritePoolSprite_Show(work->fieldItems[i].shadowSprite, TRUE, FALSE);
        }
    }
}

void PokeathlonPennantCapture_UpdatePennantSprites(PokeathlonPennantCapture *work, PokeathlonPennantCaptureFrame *frame, int x, int y) {
    u8 i;
    PokeathlonPennantCapturePennant *base;

    for (i = 0, base = work->pennants.pennants; i < PENNANT_CAPTURE_PENNANTS; i++) {
        int spot;
        int dx;
        int dy;
        VecFx32 position;
        spot = frame->spots[i] & 0x1F;
        dx = work->pennants.spots[spot].x - x + 0x80;
        dy = work->pennants.spots[spot].y - y + 0x60;
        position.z = 0;
        position.x = dx * FX32_ONE;
        position.y = dy * FX32_ONE;
        PokeathlonCourseSpritePoolSprite_SetPosition(work->pennants.pennants[i].sprite, &position);
        PokeathlonCourseSpritePoolSprite_SetPosition(work->pennants.pennants[i].poleSprite, &position);
        if (dx >= -32 && dx <= 288 && dy >= -32 && dy <= 224) {
            PokeathlonCourseSpritePoolSprite_Show(work->pennants.pennants[i].sprite, TRUE, TRUE);
            PokeathlonCourseSpritePoolSprite_Show(work->pennants.pennants[i].poleSprite, TRUE, TRUE);
        } else {
            PokeathlonCourseSpritePoolSprite_Show(work->pennants.pennants[i].sprite, TRUE, FALSE);
            PokeathlonCourseSpritePoolSprite_Show(work->pennants.pennants[i].poleSprite, TRUE, FALSE);
        }
        if (spot != work->shownPennantSpots[i]) {
            PokeathlonCourseSpritePoolSprite_SetAnim(work->pennants.pennants[i].poleSprite, 4);
            work->shownPennantSpots[i] = spot;
        }
        PokeathlonPennantCapture_SetPennantDrawPriority(&base[i], dy);
    }
}

// The Pokemon jumps off the field (32 frames), then the top screen swaps the team member's picture.
static void PokeathlonPennantCapture_RunMonExit(PokeathlonPennantCaptureDisplay *display, PokeathlonPennantCapturePlayer *player, int participant, int member, u8 *block, int waitOther, int isSelf) {
    switch (player->swapStep) {
    case 0:
        player->swapTimer = 0;
        player->swapStep++;
        break;
    case 1:
        player->drawOffsetY = player->drawOffsetY + 8;
        player->swapTimer++;
        if (player->swapTimer >= 0x20) {
            PokeathlonCourseMonObject_SetDrawFlag(player->monObjects[member], 0);
            player->swapStep++;
        }
        break;
    case 2:
        if (isSelf) {
            if (PokeathlonPennantCaptureDisplay_SlideMonPic(display, member)) {
                block[8] = 1;
                player->swapStep++;
            }
        } else if (waitOther) {
            if (PokeathlonPennantCaptureDisplay_StepSwapWait(display, participant)) {
                block[8] = 1;
                player->swapStep++;
            }
        } else {
            player->swapStep++;
        }
        break;
    case 3:
        break;
    }
}

// The new team member drops back onto the field (32 frames).
static void PokeathlonPennantCapture_RunMonEnter(PokeathlonPennantCapturePlayer *player, int participant, int member, u8 *block, int report) {
    switch (player->swapStep) {
    case 0:
        player->swapTimer = 0;
        PokeathlonCourseMonObject_SetDrawFlag(player->monObjects[member], 1);
        player->swapStep++;
        break;
    case 1:
        player->drawOffsetY = player->drawOffsetY - 8;
        player->swapTimer++;
        if (player->swapTimer >= 0x20) {
            player->drawOffsetY = 0;
            player->swapStep++;
        }
        break;
    case 2:
        if (report) {
            block[8] = 2;
        }
        player->swapStep++;
        break;
    case 3:
        break;
    }
}

static void PokeathlonPennantCapture_UpdateMonSwap(PokeathlonPennantCaptureDisplay *display, PokeathlonPennantCapturePlayer *player, PokeathlonPennantCaptureFrame *frame, u8 participant, u8 member, u8 *block, int isHost, int isSelf, int remote) {
    u8 states = frame->words[4] >> 23;
    u8 state = (states >> (participant * 2)) & 3;

    if (player->swapState != state) {
        player->swapState = state;
        player->swapStep = 0;
    }
    switch (player->swapState) {
    case 0:
        break;
    case 1:
        PokeathlonPennantCapture_RunMonExit(display, player, participant, member, block, (isHost && remote) ? 1 : 0, isSelf);
        break;
    case 2:
        PokeathlonPennantCapture_RunMonEnter(player, participant, member, block, isSelf | ((isHost && remote) ? 1 : 0));
        break;
    }
}

void PokeathlonPennantCapture_SetMoveParams(PokeathlonCourseData *course, const PokeathlonPennantCaptureMoveTables *tables, int participant, u8 member, PokeathlonPennantCapturePlayer *player) {
    u8 *levels = PokeathlonCourse_GetMonStats(course, participant, member);

    player->moveParams[member].knockbackScale = (f32)tables->knockbackScale[levels[0]] / 10.0;
    player->moveParams[member].maxSpeed = tables->maxSpeed[levels[1]];
    player->moveParams[member].unk00 = (f32)tables->unk00[levels[4]] / 10.0;
    player->moveParams[member].unk04 = (f32)tables->unk04[levels[1]] / 10.0;
    player->moveParams[member].knockdownTime = tables->knockdownTime[levels[3]];
}

static void PokeathlonPennantCapture_GetObstaclesNear(PokeathlonPennantCapture *work, fx32 fieldX, fx32 fieldY, PokeathlonPennantCapturePoint **out, u8 *count) {
    int i;
    int x;
    int y;
    PokeathlonPennantCapturePoint *position;

    for (i = 0; i < PENNANT_CAPTURE_OBSTACLES; i++) {
        out[i] = NULL;
    }
    x = fieldX / FX32_ONE;
    y = fieldY / FX32_ONE;
    *count = 0;
    position = work->obstaclePositions;
    for (i = 0; i < PENNANT_CAPTURE_OBSTACLES; i++, position++) {
        int dx;
        int dy;
        if (!work->obstacles[i].placed) {
            break;
        }
        dx = work->obstacles[i].x - x + 0x80;
        dy = work->obstacles[i].y - y + 0x60;
        if (dx >= -32 && dx <= 288 && dy >= -32 && dy <= 224) {
            out[(*count)++] = position;
        }
    }
}

static void PokeathlonPennantCapture_CalcObstacleBounds(int radius, const PokeathlonPennantCapturePoint *center, PokeathlonPennantCaptureBounds *bounds) {
    Point2D *out = bounds->corners;
    int x = center->x;
    int y = center->y;

    out[0].x = x - 16;
    out[0].y = y - 16;
    out[1].x = x + 16;
    out[1].y = y - 16;
    out[2].x = x + 16;
    out[2].y = y + 16;
    out[3].x = x - 16;
    out[3].y = y + 16;
    out[4] = out[0];
    out[5] = out[1];
    out[6] = out[1];
    out[7] = out[2];
    out[8] = out[2];
    out[9] = out[3];
    out[10] = out[3];
    out[11] = out[0];
    out[4].y -= radius;
    out[5].y -= radius;
    out[6].x += radius;
    out[7].x += radius;
    out[8].y += radius;
    out[9].y += radius;
    out[10].x -= radius;
    out[11].x -= radius;
}

static int PokeathlonPennantCapture_CollideWithObstacle(VecFx32 *from, VecFx32 *to, int radius, PokeathlonPennantCaptureBounds *bounds, VecFx32 *out) {
    VecFx32 *end = to;
    VecFx32 *result = out;
    int i;
    Point2D hit;
    Point2D start;
    Point2D stop;
    fx32 entry;
    BOOL onEdge;
    int sideX, sideY;
    int lo;
    BOOL useX, useY;
    int hi;
    Point2D *corner;
    int x0, x1, y0, y1;
    s32 ret;
    start.x = from->x / FX32_ONE;
    int dx0, dx1, dy0, dy1;
    start.y = from->y / FX32_ONE;
    stop.x = end->x / FX32_ONE;
    stop.y = end->y / FX32_ONE;
    for (i = 0; i < 4; ++i) {
        PokeathlonPennantCaptureHitCodes codes = gPennantCaptureEdgeHits;
        if (Point2D_CalcSegmentIntersection(&bounds->edges[i][0], &bounds->edges[i][1], &start, &stop, &hit)) {
            result->x = hit.x << FX32_SHIFT;
            result->y = hit.y << FX32_SHIFT;
            result->z = 0;
            return codes.sides[i];
        }
        onEdge = FALSE;
        // BUG: i is never both 0 and 2, so every edge takes the second test (the one meant for the vertical edges).
        if (i == 0 && i == 2) {
            if (start.y == bounds->edges[i][0].y) {
                if (bounds->edges[i][0].x < bounds->edges[i][1].x) {
                    lo = bounds->edges[i][0].x;
                    hi = bounds->edges[i][1].x;
                } else {
                    lo = bounds->edges[i][1].x;
                    hi = bounds->edges[i][0].x;
                }
                if (lo <= start.x && start.x <= hi) {
                    onEdge = TRUE;
                }
            }
        } else {
            if (start.x == bounds->edges[i][0].x) {
                if (bounds->edges[i][0].y < bounds->edges[i][1].y) {
                    lo = bounds->edges[i][0].y;
                    hi = bounds->edges[i][1].y;
                } else {
                    lo = bounds->edges[i][1].y;
                    hi = bounds->edges[i][0].y;
                }
                if (lo <= start.y && start.y <= hi) {
                    onEdge = TRUE;
                }
            }
        }
        if (onEdge) {
            *result = *from;
            return codes.sides[i];
        }
    }
    i = 0, corner = bounds->corners;
    while (i < 4) {
        PokeathlonPennantCaptureHitCodes codes = gPennantCaptureCornerHits;
        VecFx32 point;
        VecFx32 delta;
        point.x = corner->x << FX32_SHIFT;
        point.y = corner->y << FX32_SHIFT;
        point.z = 0;
        VEC_Subtract(end, &point, &delta);
        if (VEC_Mag(&delta) <= radius << FX32_SHIFT) {
            VecFx32 path;
            VecFx32 touch;
            entry = PokeathlonPennantCapture_CalcCircleEntry(from, end, &point, radius * FX32_ONE);
            VEC_Subtract(end, from, &path);
            VEC_MultAdd(entry, &path, from, &touch);
            result->x = touch.x;
            result->y = touch.y;
            result->z = 0;
            return codes.sides[i];
        }
        i++, corner++;
    }
    // ended inside the block: push out through the nearest side
    if (bounds->corners[0].x << FX32_SHIFT <= end->x && end->x <= bounds->corners[2].x << FX32_SHIFT && bounds->corners[0].y << FX32_SHIFT <= end->y && end->y <= bounds->corners[2].y << FX32_SHIFT) {
        *result = *end;
        x0 = bounds->corners[0].x;
        useX = useY = FALSE;
        dx0 = (x0 << FX32_SHIFT) - end->x;
        x1 = bounds->corners[2].x;
        dx1 = (x1 << FX32_SHIFT) - end->x;
        if (dx0 < 0) {
            dx0 *= -1;
        }
        if (dx1 < 0) {
            dx1 *= -1;
        }
        if (dx0 < dx1) {
            sideX = 12;
        } else {
            dx0 = dx1;
            sideX = 10;
        }
        y0 = bounds->corners[0].y;
        dy0 = (y0 << FX32_SHIFT) - end->y;
        y1 = bounds->corners[2].y;
        dy1 = (y1 << FX32_SHIFT) - end->y;
        if (dy0 < 0) {
            dy0 *= -1;
        }
        if (dy1 < 0) {
            dy1 *= -1;
        }
        if (dy0 < dy1) {
            sideY = 9;
        } else {
            dy0 = dy1;
            sideY = 11;
        }
        if (dx0 <= dy0) {
            useX = TRUE;
            ret = sideX;
        } else {
            useY = TRUE;
            ret = sideY;
        }
        if (useX) {
            if (sideX == 12) {
                result->x = (x0 - radius) << FX32_SHIFT;
            } else {
                result->x = (x1 + radius) << FX32_SHIFT;
            }
            return ret;
        }
        if (useY) {
            if (sideY == 9) {
                result->y = (y0 - radius) << FX32_SHIFT;
            } else {
                result->y = (y1 + radius) << FX32_SHIFT;
            }
            return ret;
        }
        return ret;
    }
    return 0;
}

static int PokeathlonPennantCapture_CollideWithObstacles(PokeathlonPennantCapture *work, int excluded, int radius, VecFx32 *position, VecFx32 *end, VecFx32 *out) {
    u8 count;
    u8 result;
    int i;
    VecFx32 localOut;
    PokeathlonPennantCapturePoint *candidates[20];
    PokeathlonPennantCaptureBounds bounds;

    PokeathlonPennantCapture_GetObstaclesNear(work, position->x, position->y, candidates, &count);
    i = 0;
    result = 0;
    for (; i < count; i++) {
        int code;
        PokeathlonPennantCapture_CalcObstacleBounds(radius, candidates[i], &bounds);
        code = PokeathlonPennantCapture_CollideWithObstacle(position, end, radius, &bounds, &localOut);
        if (code != 0) {
            if (code >= 9) {
                out->x = localOut.x;
                out->y = localOut.y;
                out->z = 0;
            } else if (code != excluded) {
                out->x = localOut.x;
                out->y = localOut.y;
                out->z = 0;
            }
            result = code;
            break;
        }
    }
    return result;
}

// The smaller root t of |a + t (b - a) - point| = radius.
static fx32 PokeathlonPennantCapture_CalcCircleEntry(VecFx32 *a, VecFx32 *b, VecFx32 *point, fx32 radius) {
    VecFx32 delta;
    fx32 magnitude;
    fx32 dot;
    fx32 length;
    fx32 root;
    fx32 first;
    fx32 second;
    VEC_Subtract(b, a, &delta);
    magnitude = VEC_Mag(&delta);
    magnitude *= magnitude;
    dot = VEC_DotProduct(&delta, point);
    length = VEC_Mag(point);
    length *= length;
    root = FX_Sqrt(dot - (length - radius * radius) * magnitude);
    first = FX_Div(root - dot, magnitude);
    second = FX_Div(-(dot + root), magnitude);
    if (first <= second) {
        second = first;
    }
    return second;
}

float PokeathlonPennantCapture_GetMaxSpeed(PokeathlonPennantCapturePlayer *player, int member) {
    return player->moveParams[member].maxSpeed;
}

void PokeathlonPennantCapture_AddPoint2D(const Point2D *a, const Point2D *b, Point2D *out) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
}

static inline void PokeathlonPennantCapture_ZeroVec(VecFx32 *v) {
    memset(v, 0, sizeof(*v));
}

#if defined(SDK_PORT)
#define CONTACT(player, i) ((player)->contacts[(i)])
#else
#define CONTACT(player, i) (*((u8 *)((u32)(player) + (u32)(i)) + offsetof(PokeathlonPennantCapturePlayer, contacts)))
#endif

static int PokeathlonPennantCapture_CollidePlayers(PokeathlonCourseData *course, PokeathlonPennantCapturePlayer *players, VecFx32 *out) {
    PokeathlonCourseMonObject *objectA;
    PokeathlonCourseMonObject *objectB;
    int applied;
    int i, j;
    PokeathlonPennantCapturePlayer *player, *other;
    int aX;
    PokeathlonPennantCaptureCollisionFrame frame;
    applied = 0;
    frame.pointA.z = 0;
    frame.pointB.z = 0;
    for (i = 0, player = players; i < POKEATHLON_PARTICIPANTS; i++, player++) {
        int member = player->member;
        if (player->depositing) {
            continue;
        }
        objectA = player->monObjects[member];
        PokeathlonCourseObject_GroundToCenter(objectA, player->position.x / FX32_ONE, player->position.y / FX32_ONE, &frame.aX, &frame.aY);
        for (j = 0, other = players; j < POKEATHLON_PARTICIPANTS; j++, other++) {
            if (i == j || other->depositing) {
                continue;
            }
            aX = frame.aX;
            objectB = other->monObjects[other->member];
            PokeathlonCourseObject_GroundToCenter(objectB, other->position.x / FX32_ONE, other->position.y / FX32_ONE, &frame.bX, &frame.bY);
            PokeathlonCourseObject_GetCollisionCircle(objectB, frame.bX << FX32_SHIFT, frame.bY << FX32_SHIFT, &frame.pointB.x, &frame.pointB.y, &frame.radiusB);
            PokeathlonCourseObject_GetCollisionCircle(objectA, aX << FX32_SHIFT, frame.aY << FX32_SHIFT, &frame.pointA.x, &frame.pointA.y, &frame.radiusA);
            VEC_Subtract(&frame.pointA, &frame.pointB, &frame.diff);
            if (VEC_Mag(&frame.diff) < (s32)((frame.radiusB + frame.radiusA) << FX32_SHIFT)) {
                if (CONTACT(player, j)) {
                    continue;
                }
                if (!PokeathlonPennantCapture_TryKnockDown(course, player, other)) {
                    player->freezeTimer = 6;
                    other->freezeTimer = 6;
                }
                if (VEC_Mag(&player->velocity) > 11 * FX32_ONE) {
                    PokeathlonPennantCapture_ZeroVec(&frame.zeroA);
                    VEC_Normalize(&player->velocity, &player->velocity);
                    VEC_MultAdd(11 * FX32_ONE, &player->velocity, &frame.zeroA, &player->velocity);
                }
                if (VEC_Mag(&other->velocity) > 11 * FX32_ONE) {
                    PokeathlonPennantCapture_ZeroVec(&frame.zeroB);
                    VEC_Normalize(&other->velocity, &other->velocity);
                    VEC_MultAdd(11 * FX32_ONE, &other->velocity, &frame.zeroB, &other->velocity);
                }
                GF_ASSERT(CONTACT(player, j) == 0);
                GF_ASSERT(CONTACT(other, i) == 0);
                CONTACT(player, j) = 1;
                CONTACT(other, i) = 1;
                if (!applied) {
                    PokeathlonPennantCapture_ZeroVec(&frame.resolved);
                    applied = 1;
                    VEC_Subtract(&frame.pointB, &frame.pointA, &frame.resolved);
                    VEC_Normalize(&frame.resolved, &frame.resolved);
                    VEC_MultAdd(frame.radiusA << FX32_SHIFT, &frame.resolved, &frame.pointA, &frame.resolved);
                    *out = frame.resolved;
                }
                PokeathlonCourse_AddMonCounter(course, player->participant, player->member, POKEATHLON_COUNTER_TACKLES, 1);
                PokeathlonCourse_AddMonCounter(course, other->participant, other->member, POKEATHLON_COUNTER_TACKLES, 1);
                break;
            } else {
                CONTACT(player, j) = 0;
#if defined(SDK_PORT)
                other->contacts[i] = 0;
#else
                *((u8 *)(i + (u32)other) + offsetof(PokeathlonPennantCapturePlayer, contacts)) = 0;
#endif
            }
        }
    }
    return applied;
}

void PokeathlonPennantCapture_UpdateFieldDrawPriorities(PokeathlonPennantCapture *work) {
    int i;

    for (i = 0; i < PENNANT_CAPTURE_FIELD_ITEMS; i++) {
        int priority;
        PokeathlonCourseSpritePoolSprite_SetDrawPriority(work->fieldItems[i].shadowSprite, 3);
        priority = PokeathlonPennantCapture_GetDrawPriority(PokeathlonCourseSpritePoolSprite_GetPosition(work->fieldItems[i].sprite)->y / FX32_ONE);
        PokeathlonCourseSpritePoolSprite_SetDrawPriority(work->fieldItems[i].sprite, priority + 7);
    }

    for (i = 0; i < PENNANT_CAPTURE_OBSTACLES; i++) {
        int priority = PokeathlonPennantCapture_GetDrawPriority(PokeathlonCourseSpritePoolSprite_GetPosition(work->obstacles[i].sprite)->y / FX32_ONE);
        PokeathlonCourseSpritePoolSprite_SetDrawPriority(work->obstacles[i].sprite, priority + 1);
    }
}

static void PokeathlonPennantCapture_SetPennantDrawPriority(PokeathlonPennantCapturePennant *pennant, int y) {
    int priority = PokeathlonPennantCapture_GetDrawPriority(y);

    PokeathlonCourseSpritePoolSprite_SetDrawPriority(pennant->sprite, priority + 9);
    PokeathlonCourseSpritePoolSprite_SetDrawPriority(pennant->poleSprite, priority + 8);
}

static void PokeathlonPennantCapture_SetDropEffectDrawPriority(PokeathlonPennantCapture *work) {
    int i;

    for (i = 0; i < POKEATHLON_PARTICIPANTS; i++) {
        int priority = PokeathlonPennantCapture_GetDrawPriority(PokeathlonCourseSpritePoolSprite_GetPosition(work->dropEffects[i])->y / FX32_ONE);
        PokeathlonCourseSpritePoolSprite_SetDrawPriority(work->dropEffects[i], priority + 2);
    }
}

static void PokeathlonPennantCapture_SetPlayerDrawPriority(PokeathlonPennantCapturePlayer *player, int y) {
    int i;
    int priority = PokeathlonPennantCapture_GetDrawPriority(y);

    PokeathlonCourseSpritePoolSprite_SetDrawPriority(player->carrySprite, priority + 3);
    for (i = 0; i < POKEATHLON_TEAM_SIZE; i++) {
        PokeathlonCourseMonObject_SetDrawPriority(player->monObjects[i], priority + 4);
    }
}

void PokeathlonPennantCapture_InitScoreWindow(PokeathlonPennantCapture *work) {
    AddWindow(work->bgConfig, &work->scoreWindow, &gPennantCaptureScoreWindowTemplate);
    BG_FillCharDataRange(work->bgConfig, GF_BG_LYR_MAIN_3, 0, 1, 0);
    LoadFontPal0(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_15_OFFSET, work->heapId);
}

static void PokeathlonPennantCapture_PrintScore(PokeathlonPennantCapture *work, u16 score) {
    MsgData *msgData;
    MessageFormat *format;
    String *string;

    FillWindowPixelBuffer(&work->scoreWindow, 0);
    msgData = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, NARC_msg_msg_0309_bin, work->heapId);
    format = MessageFormat_New(work->heapId);
    BufferIntegerAsString(format, 0, score, 3, PRINTING_MODE_LEFT_ALIGN, TRUE);
    string = ReadMsgData_ExpandPlaceholders(format, msgData, msg_0309_00157, work->heapId);
    AddTextPrinterParameterizedWithColor(&work->scoreWindow, 4, string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    String_Delete(string);
    MessageFormat_Delete(format);
    DestroyMsgData(msgData);
    CopyWindowToVram(&work->scoreWindow);
}

void PokeathlonPennantCapture_ResetPennantSpots(PokeathlonPennantCapture *work) {
    work->pennants.nextSpot = 0;
}

static void PokeathlonPennantCapture_AdvancePennantSpot(PokeathlonPennantCapturePennants *pennants) {
    pennants->nextSpot = (pennants->nextSpot + 1) % 20;
}

static void PokeathlonPennantCapture_PlacePennant(PokeathlonPennantCapturePennants *pennants) {
    int i;

    for (i = 0; i < PENNANT_CAPTURE_PENNANTS; i++) {
        if (pennants->pennants[i].active == 0) {
            pennants->pennants[i].spot = pennants->nextSpot;
            pennants->pennants[i].x = pennants->spots[pennants->nextSpot].x;
            pennants->pennants[i].y = pennants->spots[pennants->nextSpot].y;
            PokeathlonPennantCapture_AdvancePennantSpot(pennants);
            pennants->pennants[i].active = 1;
            return;
        }
    }
    GF_ASSERT(FALSE);
}

void PokeathlonPennantCapture_RefillPennants(PokeathlonPennantCapture *work) {
    int i;
    int missing = 5 - work->pennants.count;

    for (i = 0; i < missing; i++) {
        PokeathlonPennantCapture_PlacePennant(&work->pennants);
        work->pennants.count++;
        GF_ASSERT(work->pennants.count <= PENNANT_CAPTURE_PENNANTS);
    }
}

static BOOL PokeathlonPennantCapture_PickUpPennants(PokeathlonPennantCapturePennants *pennants, PokeathlonPennantCapturePlayer *player, u8 *full) {
    int i;
    VecFx32 position;

    *full = FALSE;
    for (i = 0; i < PENNANT_CAPTURE_PENNANTS; i++) {
        if (pennants->pennants[i].active != 0) {
            position.x = pennants->spots[pennants->pennants[i].spot].x << FX32_SHIFT;
            position.y = pennants->spots[pennants->pennants[i].spot].y << FX32_SHIFT;
            position.z = 0;
            if (PokeathlonPennantCapture_CirclesOverlap(&player->position, &position, PokeathlonCourseObject_GetCollisionRadius(player->monObjects[player->member]) << FX32_SHIFT, 0x8000)) {
                if (player->pennants >= PENNANT_CAPTURE_MAX_CARRIED) {
                    if (player->touchingPennant[i] == 0) {
                        *full = TRUE;
                        player->touchingPennant[i] = TRUE;
                    }
                    return FALSE;
                }
                player->touchingPennant[i] = TRUE;
                pennants->pennants[i].active = FALSE;
                player->pennants++;
                GF_ASSERT(pennants->count != 0);
                pennants->count--;
                return TRUE;
            }
            player->touchingPennant[i] = FALSE;
        } else {
            player->touchingPennant[i] = FALSE;
        }
    }
    return FALSE;
}

static BOOL PokeathlonPennantCapture_CirclesOverlap(const VecFx32 *a, const VecFx32 *b, fx32 radiusA, fx32 radiusB) {
    VecFx32 delta;

    VEC_Subtract(a, b, &delta);
    if (VEC_Mag(&delta) < radiusA + radiusB) {
        return TRUE;
    }
    return FALSE;
}

// 1 = up, 2 = right, 3 = down, 4 = left (from the angle to gPennantCaptureReferenceVector)
int PokeathlonPennantCapture_GetDirection(VecFx32 *vector) {
    VecFx32 normalized;
    VecFx32 reference;
    VecFx32 rotated;
    u16 angle;
    int result = 1;

    VEC_Normalize(vector, &normalized);
    if (VEC_Mag(&normalized) != 0) {
        reference = gPennantCaptureReferenceVector;
        rotated.x = normalized.y;
        rotated.z = normalized.x;
        rotated.y = 0;
        angle = CalcAngleBetweenVecs(&reference, &rotated);
        if (angle <= 0x2000 || angle >= 0xE000) {
            result = 4;
        } else if (angle > 0x2000 && angle < 0x6000) {
            result = 2;
        } else if (angle >= 0x6000 && angle <= 0xA000) {
            result = 3;
        } else {
            result = 1;
        }
    }
    return result;
}

// A faster Pokemon running into another one that faces the same way knocks it down and takes up to two pennants.
static BOOL PokeathlonPennantCapture_TryKnockDown(PokeathlonCourseData *course, PokeathlonPennantCapturePlayer *a, PokeathlonPennantCapturePlayer *b) {
    fx32 magA;
    fx32 magB;
    PokeathlonPennantCapturePlayer *fast;
    fx32 scale;
    u8 member;

    magA = VEC_Mag(&a->velocity);
    magB = VEC_Mag(&b->velocity);
    if (magA > magB) {
        fast = a;
    } else if (magA < magB) {
        fast = b;
        b = a;
    } else {
        return FALSE;
    }
    if (fast->direction != b->direction) {
        return FALSE;
    }
    if (PokeathlonPennantCapture_GetDirection(&fast->velocity) != fast->direction) {
        return FALSE;
    }
    if (fast->knockedDown == 1) {
        return FALSE;
    }
    if (b->pennants >= 2) {
        b->pennants -= 2;
        fast->pennants += 2;
    } else {
        fast->pennants += b->pennants;
        b->pennants = 0;
    }
    if (fast->pennants > PENNANT_CAPTURE_MAX_CARRIED) {
        fast->pennants = PENNANT_CAPTURE_MAX_CARRIED;
    }
    b->knockedDown = 1;
    PokeathlonCourse_AddMonCounter(course, b->participant, b->member, POKEATHLON_COUNTER_FAILURES, 1);
    member = b->member;
    b->knockdownTimer = b->moveParams[member].knockdownTime + VEC_Mag(&fast->velocity) / FX32_ONE;
    {
        VecFx32 zero = { 0, 0, 0 };
        scale = 4096.0f * fast->moveParams[fast->member].knockbackScale;
        VEC_MultAdd(scale, &fast->velocity, &zero, &b->velocity);
        fast->velocity = zero;
    }
    fast->freezeTimer = 0x12;
    b->freezeTimer = 6;
    return TRUE;
}

static BOOL PokeathlonPennantCapture_CanDeposit(PokeathlonPennantCapturePlayer *player) {
    BOOL inGoal = FALSE;

    if (player->position.y > 0x198000 && player->position.x >= 0xD0000 && player->position.x <= 0x130000) {
        inGoal = TRUE;
    }
    if (!inGoal) {
        return FALSE;
    }
    if (player->pennants == 0) {
        return FALSE;
    }
    if (player->knockedDown == 0) {
        return TRUE;
    }
    return FALSE;
}

// A deposit scores one point per pennant.
static u8 PokeathlonPennantCapture_GetDepositPoints(u8 pennants) {
    return pennants;
}

static int PokeathlonPennantCapture_GetDrawPriority(int y) {
    return (0x100 - y) * 8 + 5;
}

void PokeathlonPennantCapture_AnimateBg(PokeathlonPennantCapture *work) {
    if (work->bgAnimTimer >= 4) {
        void *chars = work->bgChars->pRawData;
        u32 offset = work->bgAnimFrame << 11;
        DC_FlushRange((u8 *)chars + offset, 0x800);
        GX_LoadBG0Char((u8 *)chars + offset, 0x1800, 0x800);
        work->bgAnimTimer = 0;
        work->bgAnimFrame = (work->bgAnimFrame + 1) % 6;
    } else {
        work->bgAnimTimer++;
    }
}

static void PokeathlonPennantCapture_PlaySEIfOnScreen(int x, int y, int seq, BOOL self) {
    if (x >= 0 && x < 256 && y >= 0 && y < 192) {
        PlaySEOnPlayer(seq, self ? PLAYER_SE_1 : PLAYER_SE_2);
    }
}

void PokeathlonPennantCapture_UpdateOwnScore(PokeathlonCourseData *course) {
    PokeathlonPennantCaptureFrame *frame = (PokeathlonPennantCaptureFrame *)PokeathlonCourse_ToBytes(PokeathlonCourse_GetDataCopyArea(course) + POKEATHLON_COURSE_DATA_FRAME_IN);
    PokeathlonPennantCapture *work = PokeathlonCourse_GetWork(course);
    u8 self = PokeathlonCourse_GetNetId(course);
    u16 score = (frame->words[self] >> 18) & 1023;
    u8 bonus = (frame->spots[self] >> 11) & 31;
    score = score + bonus;
    if (score > POKEATHLON_SCORE_MAX) {
        score = POKEATHLON_SCORE_MAX;
    }
    PokeathlonCourse_SetRecordScore(course, score);
    PokeathlonPennantCapture_PrintScore(work, score);
}
