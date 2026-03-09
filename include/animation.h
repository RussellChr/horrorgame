#ifndef ANIMATION_H
#define ANIMATION_H

#define ANIM_MAX_FRAMES 8

typedef struct {
    int   frame_count;      /* total frames in the animation         */
    int   current_frame;    /* index of the currently displayed frame */
    float frame_timer;      /* seconds accumulated in current frame   */
    float frame_duration;   /* seconds per frame                      */
    int   looping;          /* 1 = loop, 0 = stop on last frame       */
    int   finished;         /* set to 1 when a non-looping anim ends  */
} Animation;

void animation_init(Animation *anim, int frame_count,
                    float fps, int looping);
void animation_update(Animation *anim, float dt);
int  animation_get_frame(const Animation *anim);
void animation_reset(Animation *anim);

#endif /* ANIMATION_H */
