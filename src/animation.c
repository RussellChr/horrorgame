#include "animation.h"

void animation_init(Animation *anim, int frame_count,
                    float fps, int looping)
{
    if (!anim) return;
    anim->frame_count    = frame_count > 0 ? frame_count : 1;
    anim->current_frame  = 0;
    anim->frame_timer    = 0.0f;
    anim->frame_duration = (fps > 0.0f) ? (1.0f / fps) : 0.1f;
    anim->looping        = looping;
    anim->finished       = 0;
}

void animation_update(Animation *anim, float dt)
{
    if (!anim || anim->finished) return;

    anim->frame_timer += dt;
    while (anim->frame_timer >= anim->frame_duration) {
        anim->frame_timer -= anim->frame_duration;
        anim->current_frame++;

        if (anim->current_frame >= anim->frame_count) {
            if (anim->looping) {
                anim->current_frame = 0;
            } else {
                anim->current_frame = anim->frame_count - 1;
                anim->finished = 1;
            }
        }
    }
}

int animation_get_frame(const Animation *anim)
{
    if (!anim) return 0;
    return anim->current_frame;
}

void animation_reset(Animation *anim)
{
    if (!anim) return;
    anim->current_frame = 0;
    anim->frame_timer   = 0.0f;
    anim->finished      = 0;
}
