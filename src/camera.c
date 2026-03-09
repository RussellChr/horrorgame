#include "camera.h"

void camera_init(Camera *cam, int viewport_w, int viewport_h,
                 int world_w, int world_h)
{
    if (!cam) return;
    cam->x          = 0.0f;
    cam->y          = 0.0f;
    cam->viewport_w = viewport_w;
    cam->viewport_h = viewport_h;
    cam->world_w    = world_w;
    cam->world_h    = world_h;
}

/* Clamp camera so it stays within the world bounds. */
static void camera_clamp(Camera *cam)
{
    float max_x = (float)(cam->world_w - cam->viewport_w);
    float max_y = (float)(cam->world_h - cam->viewport_h);

    if (cam->x < 0.0f)           cam->x = 0.0f;
    if (max_x > 0.0f && cam->x > max_x) cam->x = max_x;
    else if (max_x <= 0.0f)      cam->x = 0.0f;

    if (cam->y < 0.0f)           cam->y = 0.0f;
    if (max_y > 0.0f && cam->y > max_y) cam->y = max_y;
    else if (max_y <= 0.0f)      cam->y = 0.0f;
}

void camera_follow(Camera *cam, float target_x, float target_y, float dt)
{
    if (!cam) return;

    /* Target camera so the target is centred in the viewport. */
    float desired_x = target_x - (float)cam->viewport_w * 0.5f;
    float desired_y = target_y - (float)cam->viewport_h * 0.5f;

    /* Lerp towards target (smooth follow). */
    float speed = 8.0f;
    cam->x += (desired_x - cam->x) * speed * dt;
    cam->y += (desired_y - cam->y) * speed * dt;

    camera_clamp(cam);
}

void camera_snap(Camera *cam, float target_x, float target_y)
{
    if (!cam) return;
    cam->x = target_x - (float)cam->viewport_w * 0.5f;
    cam->y = target_y - (float)cam->viewport_h * 0.5f;
    camera_clamp(cam);
}

int camera_to_screen_x(const Camera *cam, float wx)
{
    return (int)(wx - cam->x);
}

int camera_to_screen_y(const Camera *cam, float wy)
{
    return (int)(wy - cam->y);
}

float camera_to_world_x(const Camera *cam, int sx)
{
    return (float)sx + cam->x;
}

float camera_to_world_y(const Camera *cam, int sy)
{
    return (float)sy + cam->y;
}
