#include "collision.h"

int rect_overlaps(const Rect *a, const Rect *b)
{
    if (!a || !b) return 0;
    return (a->x         < b->x + b->w &&
            a->x + a->w  > b->x        &&
            a->y         < b->y + b->h &&
            a->y + a->h  > b->y);
}

void rect_resolve_collision(Rect *a, const Rect *b,
                            float *vx, float *vy)
{
    if (!a || !b || !vx || !vy) return;
    if (!rect_overlaps(a, b)) return;

    /* Calculate overlap on each axis. */
    float overlap_x_left  =  (b->x + b->w) - a->x;
    float overlap_x_right = -(a->x + a->w  - b->x);
    float overlap_y_top   =  (b->y + b->h) - a->y;
    float overlap_y_bot   = -(a->y + a->h  - b->y);

    float ox = (overlap_x_left  < -overlap_x_right)
               ? overlap_x_left  : overlap_x_right;
    float oy = (overlap_y_top   < -overlap_y_bot)
               ? overlap_y_top   : overlap_y_bot;

    /* Resolve along the axis with the smallest overlap. */
    if (ox < 0.0f) ox = -ox;
    if (oy < 0.0f) oy = -oy;

    if (ox < oy) {
        /* Push horizontally */
        if (a->x < b->x)
            a->x = b->x - a->w;
        else
            a->x = b->x + b->w;
        *vx = 0.0f;
    } else {
        /* Push vertically */
        if (a->y < b->y)
            a->y = b->y - a->h;
        else
            a->y = b->y + b->h;
        *vy = 0.0f;
    }
}
