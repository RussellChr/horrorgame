#ifndef COLLISION_H
#define COLLISION_H

#define MAX_COLLISION_RECTS 1024
#define MAX_TRIGGER_ZONES   16

/* Axis-aligned bounding box. */
typedef struct {
    float x, y, w, h;
} Rect;

/* A zone that triggers something when the player overlaps it. */
typedef struct {
    Rect  bounds;
    int   target_location_id; /* >=0: room transition, -1: interaction  */
    int   trigger_id;         /* identifier for the interaction         */
    float spawn_x;            /* player x after transition (target room) */
    float spawn_y;            /* player y after transition (target room) */
} TriggerZone;

/* Return 1 if two rectangles overlap. */
int rect_overlaps(const Rect *a, const Rect *b);

/* Push rect a out of rect b; adjust *vx and *vy accordingly.             */
void rect_resolve_collision(Rect *a, const Rect *b,
                            float *vx, float *vy);

#endif /* COLLISION_H */
