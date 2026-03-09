#ifndef CAMERA_H
#define CAMERA_H

typedef struct {
    float x, y;          /* top-left corner in world space       */
    int   viewport_w;    /* screen / viewport width  in pixels   */
    int   viewport_h;    /* screen / viewport height in pixels   */
    int   world_w;       /* world (room) width  in pixels        */
    int   world_h;       /* world (room) height in pixels        */
} Camera;

void camera_init(Camera *cam, int viewport_w, int viewport_h,
                 int world_w, int world_h);

/* Smoothly follow a world-space target point (typically the player centre). */
void camera_follow(Camera *cam, float target_x, float target_y, float dt);

/* Snap to target immediately (used on room transitions). */
void camera_snap(Camera *cam, float target_x, float target_y);

/* Convert world coordinates to screen coordinates. */
int  camera_to_screen_x(const Camera *cam, float wx);
int  camera_to_screen_y(const Camera *cam, float wy);

/* Convert screen coordinates to world coordinates. */
float camera_to_world_x(const Camera *cam, int sx);
float camera_to_world_y(const Camera *cam, int sy);

#endif /* CAMERA_H */
