#include "dialogue.h"
#include "render.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SDL3_IMAGE
#include <SDL3_image/SDL_image.h>
#endif

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

DialogueTree *dialogue_tree_create(void)
{
    return calloc(1, sizeof(DialogueTree));
}

void dialogue_tree_destroy(DialogueTree *tree)
{
    free(tree);
}

/* ── Node management ───────────────────────────────────────────────────── */

DialogueNode *dialogue_get_node(DialogueTree *tree, int node_id)
{
    if (!tree) return NULL;
    for (int i = 0; i < tree->node_count; i++)
        if (tree->nodes[i].id == node_id)
            return &tree->nodes[i];
    return NULL;
}

DialogueNode *dialogue_add_node(DialogueTree *tree, int id,
                                const char *speaker, const char *text,
                                int is_terminal)
{
    if (!tree || tree->node_count >= MAX_DIALOGUE_NODES) return NULL;
    DialogueNode *node = &tree->nodes[tree->node_count++];
    memset(node, 0, sizeof(DialogueNode));
    node->id          = id;
    node->is_terminal = is_terminal;
    strncpy(node->speaker, speaker ? speaker : "",  NPC_NAME_MAX - 1);
    strncpy(node->text,    text    ? text    : "",  DIALOGUE_TEXT_MAX - 1);
    return node;
}

void dialogue_add_choice(DialogueNode *node, const DialogueChoice *choice)
{
    if (!node || !choice || node->choice_count >= MAX_CHOICES) return;
    node->choices[node->choice_count++] = *choice;
}

/* ── Console (text-mode) helpers ───────────────────────────────────────── */

void dialogue_print_node(const DialogueNode *node,
                         int player_courage, int player_item_id)
{
    if (!node) return;
    printf("\n%s: \"%s\"\n", node->speaker, node->text);
    if (node->is_terminal || node->choice_count == 0) return;

    printf("\nWhat do you say?\n");
    int shown = 0;
    for (int i = 0; i < node->choice_count; i++) {
        const DialogueChoice *c = &node->choices[i];
        if (c->requires_courage > player_courage)   continue;
        if (c->requires_item_id && c->requires_item_id != player_item_id) continue;
        printf("  [%d] %s\n", ++shown, c->text);
    }
}

int dialogue_run(DialogueTree *tree, int start_node_id,
                 int player_courage, int player_item_id)
{
    if (!tree) return 0;

    int current_id = start_node_id;
    int last_flag  = 0;

    while (1) {
        DialogueNode *node = dialogue_get_node(tree, current_id);
        if (!node) break;

        dialogue_print_node(node, player_courage, player_item_id);
        if (node->is_terminal || node->choice_count == 0) break;

        int available[MAX_CHOICES];
        int count = 0;
        for (int i = 0; i < node->choice_count; i++) {
            const DialogueChoice *c = &node->choices[i];
            if (c->requires_courage > player_courage) continue;
            if (c->requires_item_id && c->requires_item_id != player_item_id) continue;
            available[count++] = i;
        }
        if (count == 0) break;

        printf("\nChoice (1-%d): ", count);
        fflush(stdout);
        int sel = 0;
        if (scanf("%d", &sel) != 1) sel = 1;
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);

        sel = utils_clamp(sel, 1, count) - 1;
        const DialogueChoice *chosen = &node->choices[available[sel]];
        last_flag  = chosen->story_flag;
        current_id = chosen->next_node_id;
    }
    return last_flag;
}

/* ── Visual dialogue state ─────────────────────────────────────────────── */

void dialogue_state_init(DialogueState *ds,
                         DialogueTree *tree, int start_node_id)
{
    if (!ds) return;
    ds->tree            = tree;
    ds->current_node_id = start_node_id;
    ds->text_timer      = 0.0f;
    ds->chars_visible   = 0;
    ds->text_complete   = 0;
    ds->selected_choice = 0;
}

void dialogue_state_update(DialogueState *ds, float dt)
{
    if (!ds || ds->text_complete) return;

    DialogueNode *node = dialogue_get_node(ds->tree, ds->current_node_id);
    if (!node) return;

    /* Typewriter: ~40 characters per second */
    ds->text_timer    += dt;
    ds->chars_visible  = (int)(ds->text_timer * 40.0f);
    int text_len       = (int)strlen(node->text);
    if (ds->chars_visible >= text_len) {
        ds->chars_visible = text_len;
        ds->text_complete = 1;
    }
}

/* Advance to the next node or end the dialogue.
 * Returns 1 if the dialogue should continue, 0 if it ended. */
int dialogue_state_advance(DialogueState *ds,
                           int player_courage, int player_item_id)
{
    if (!ds) return 0;

    /* If typewriter is still running, just complete it first. */
    if (!ds->text_complete) {
        ds->text_complete = 1;
        DialogueNode *node = dialogue_get_node(ds->tree, ds->current_node_id);
        if (node) ds->chars_visible = (int)strlen(node->text);
        return 1;
    }

    DialogueNode *node = dialogue_get_node(ds->tree, ds->current_node_id);
    if (!node) return 0;
    if (node->is_terminal) return 0;

    if (node->choice_count == 0) return 0;

    /* Build list of available choices (filtered by requirements). */
    int available[MAX_CHOICES];
    int count = 0;
    for (int i = 0; i < node->choice_count; i++) {
        const DialogueChoice *c = &node->choices[i];
        if (c->requires_courage > player_courage)                   continue;
        if (c->requires_item_id && c->requires_item_id != player_item_id) continue;
        available[count++] = i;
    }
    if (count == 0) return 0;

    /* Use the player-selected index, clamped to available range. */
    int sel = ds->selected_choice;
    if (sel >= count) sel = count - 1;
    if (sel < 0)      sel = 0;

    const DialogueChoice *chosen = &node->choices[available[sel]];
    ds->current_node_id = chosen->next_node_id;
    ds->text_timer      = 0.0f;
    ds->chars_visible   = 0;
    ds->text_complete   = 0;
    ds->selected_choice = 0;
    return 1;
}

/* Return the choice that will be taken on the next advance, or NULL. */
const DialogueChoice *dialogue_state_get_selected(const DialogueState *ds,
                                                   int player_courage,
                                                   int player_item_id)
{
    if (!ds || !ds->text_complete) return NULL;

    const DialogueNode *node = dialogue_get_node(
        (DialogueTree *)ds->tree, ds->current_node_id);
    if (!node || node->is_terminal || node->choice_count == 0) return NULL;

    int available[MAX_CHOICES];
    int count = 0;
    for (int i = 0; i < node->choice_count; i++) {
        const DialogueChoice *c = &node->choices[i];
        if (c->requires_courage > player_courage)                   continue;
        if (c->requires_item_id && c->requires_item_id != player_item_id) continue;
        available[count++] = i;
    }
    if (count == 0) return NULL;

    int sel = ds->selected_choice;
    if (sel >= count) sel = count - 1;
    if (sel < 0)      sel = 0;

    return &node->choices[available[sel]];
}

/* ── Texture management ────────────────────────────────────────────────── */

void dialogue_load_texture(DialogueState *ds,
                           SDL_Renderer *renderer, const char *path)
{
    if (!ds || !renderer || !path) return;

    /* Release any previously loaded texture */
    if (ds->bg_texture) {
        SDL_DestroyTexture(ds->bg_texture);
        ds->bg_texture = NULL;
    }

#ifdef HAVE_SDL3_IMAGE
    ds->bg_texture = IMG_LoadTexture(renderer, path);
    if (!ds->bg_texture)
        SDL_Log("dialogue: failed to load '%s': %s", path, SDL_GetError());
#endif
}

void dialogue_unload_texture(DialogueState *ds)
{
    if (!ds) return;
    if (ds->bg_texture) {
        SDL_DestroyTexture(ds->bg_texture);
        ds->bg_texture = NULL;
    }
}

/* ── Visual rendering ──────────────────────────────────────────────────── */

#define DLGBOX_H      200
#define DLGBOX_MARGIN  20
#define DLGBOX_PAD     16
#define FONT_SCALE      2

void dialogue_render(const DialogueState *ds,
                     SDL_Renderer *renderer,
                     int screen_w, int screen_h)
{
    if (!ds || !renderer) return;

    const DialogueNode *node = dialogue_get_node(
        (DialogueTree *)ds->tree, ds->current_node_id);
    if (!node) return;

    int box_y = screen_h - DLGBOX_H - DLGBOX_MARGIN;
    int box_w = screen_w - DLGBOX_MARGIN * 2;

    /* Background: PNG image when available, procedural rectangles otherwise */
    if (ds->bg_texture) {
        SDL_FRect bg_rect = {
            (float)DLGBOX_MARGIN,
            (float)box_y,
            (float)box_w,
            (float)DLGBOX_H
        };
        SDL_RenderTexture(renderer, ds->bg_texture, NULL, &bg_rect);
    } else {
        /* Fallback: procedural background panel and border */
        render_filled_rect(renderer,
            DLGBOX_MARGIN, box_y, box_w, DLGBOX_H,
            10, 8, 15, 220);
        render_rect_outline(renderer,
            DLGBOX_MARGIN, box_y, box_w, DLGBOX_H,
            80, 60, 100, 255);
        render_rect_outline(renderer,
            DLGBOX_MARGIN+2, box_y+2, box_w-4, DLGBOX_H-4,
            50, 40, 70, 200);
    }

    /* Speaker name background */
    int name_w = (int)strlen(node->speaker) * 8 * FONT_SCALE + DLGBOX_PAD * 2;
    render_filled_rect(renderer,
        DLGBOX_MARGIN, box_y - 28, name_w, 28,
        30, 20, 50, 220);
    render_rect_outline(renderer,
        DLGBOX_MARGIN, box_y - 28, name_w, 28,
        80, 60, 100, 255);

    /* Speaker name */
    render_text(renderer, node->speaker,
                DLGBOX_MARGIN + DLGBOX_PAD, box_y - 24,
                FONT_SCALE, 200, 180, 255);

    /* Dialogue text (limited by chars_visible) */
    char visible[DIALOGUE_TEXT_MAX];
    int len = ds->chars_visible;
    if (len > (int)sizeof(visible) - 1) len = (int)sizeof(visible) - 1;
    memcpy(visible, node->text, (size_t)len);
    visible[len] = '\0';

    int text_x     = DLGBOX_MARGIN + DLGBOX_PAD;
    int text_y     = box_y + DLGBOX_PAD;
    int text_max_w = box_w - DLGBOX_PAD * 2;
    int line_h     = 8 * FONT_SCALE + 6;

    render_text_wrapped(renderer, visible,
                        text_x, text_y,
                        text_max_w, FONT_SCALE, line_h,
                        220, 210, 230);

    /* ── Choice menu (shown when text is complete and >1 choice exists) ── */
    if (ds->text_complete && node->choice_count > 1) {
        int choice_x    = text_x + 8;
        int choice_start_y = box_y + DLGBOX_H / 2;
        int choice_line_h  = 8 * FONT_SCALE + 8;

        for (int i = 0; i < node->choice_count; i++) {
            const DialogueChoice *c = &node->choices[i];
            int cy = choice_start_y + i * choice_line_h;

            if (i == ds->selected_choice) {
                /* Highlighted row background */
                render_filled_rect(renderer,
                    DLGBOX_MARGIN + 4, cy - 4,
                    box_w - 8, choice_line_h,
                    60, 40, 90, 200);
                /* Arrow indicator */
                render_text(renderer, ">",
                            DLGBOX_MARGIN + DLGBOX_PAD, cy,
                            FONT_SCALE, 255, 220, 100);
                render_text(renderer, c->text,
                            choice_x + 12, cy,
                            FONT_SCALE, 255, 240, 180);
            } else {
                render_text(renderer, c->text,
                            choice_x + 12, cy,
                            FONT_SCALE, 160, 150, 170);
            }
        }
    }

    /* "Press ENTER to continue" prompt (blinks; hidden when choices shown) */
    if (ds->text_complete && node->choice_count <= 1) {
        static Uint64 blink_tick = 0;
        Uint64 now = SDL_GetTicks();
        if (!blink_tick) blink_tick = now;
        int blink_on = ((now - blink_tick) / 500) % 2 == 0;
        if (blink_on) {
            render_text(renderer,
                        "[ ENTER ]",
                        screen_w - DLGBOX_MARGIN - 100,
                        screen_h - DLGBOX_MARGIN - 24,
                        1, 140, 120, 160);
        }
    }
}

/* ── Dialogue tree builder ─────────────────────────────────────────────── */
/*
 * Builds a small dialogue tree for each location's interactive objects.
 * Node IDs are local to each tree.
 */
DialogueTree *dialogue_build_for_location(int location_id)
{
    DialogueTree *tree = dialogue_tree_create();
    if (!tree) return NULL;

    DialogueChoice next = {
        .id=0, .text="...", .next_node_id=99,
        .requires_courage=0, .requires_item_id=0,
        .sanity_delta=0, .courage_delta=0, .story_flag=0
    };

    switch (location_id) {

    case 0: /* Entrance Hall – general */
        dialogue_add_node(tree, 0, "You",
            "The entrance hall stretches before me. Dust motes float in the "
            "sickly light. I shouldn't be here.", 0);
        strncpy(next.text, "...", DIALOGUE_TEXT_MAX-1);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "You",
            "The front door is swollen shut with moisture. "
            "I have no choice but to go deeper.", 1);
        break;

    case 30: /* Entrance Hall – mysterious portrait */
        dialogue_add_node(tree, 0, "You",
            "A portrait hangs on the wall. The face in the painting stares "
            "back at me, its eyes following my every movement. "
            "Something about it feels deeply wrong.", 0);

        /* Choice 1: Stare back intently */
        strncpy(next.text, "Stare back at the portrait intently.",
                DIALOGUE_TEXT_MAX - 1);
        next.next_node_id  = 1;
        next.sanity_delta  = -5;
        next.courage_delta = 10;
        next.story_flag    = 0;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        /* Choice 2: Look away quickly */
        strncpy(next.text, "Look away quickly — this is unsettling.",
                DIALOGUE_TEXT_MAX - 1);
        next.next_node_id  = 2;
        next.sanity_delta  = 5;
        next.courage_delta = -10;
        next.story_flag    = 0;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        /* Choice 3: Ignore it */
        strncpy(next.text, "Ignore it and move on.",
                DIALOGUE_TEXT_MAX - 1);
        next.next_node_id  = 3;
        next.sanity_delta  = 0;
        next.courage_delta = 0;
        next.story_flag    = 0;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        /* Node 1: Brave path */
        dialogue_add_node(tree, 1, "You",
            "The portrait's painted eyes bore into mine. "
            "A chill runs down my spine, yet I hold my gaze. "
            "Whatever is in this house, I will face it.", 1);

        /* Node 2: Afraid path */
        dialogue_add_node(tree, 2, "You",
            "As I look away, I catch a faint whisper — or perhaps "
            "just the house settling. I tell myself it's nothing. "
            "I'm not sure I believe it.", 1);

        /* Node 3: Indifferent path */
        dialogue_add_node(tree, 3, "You",
            "I walk past without a second glance. "
            "If I stop to examine every strange thing in this place, "
            "I'll never get out.", 1);
        break;

    case 1: /* Dark Corridor – basement door */
        dialogue_add_node(tree, 0, "You",
            "The basement door is heavy iron, cold as winter. "
            "A padlock the size of my fist hangs on the latch.", 0);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "You",
            "I need a key. Maybe there's one somewhere in this house.", 1);
        break;

    case 1+100: /* Corridor – after key obtained */
        dialogue_add_node(tree, 0, "You",
            "The brass key fits perfectly. The padlock clicks open with "
            "a sound like a gunshot in the silence.", 0);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "You",
            "Cold air rushes up from the darkness below. "
            "Whatever is down there has been waiting a long time.", 1);
        break;

    case 2: /* Library – diary */
        dialogue_add_node(tree, 0, "You",
            "A leather-bound diary lies open on the desk. "
            "The ink is faded but still legible.", 0);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "Professor Ashwood's Diary",
            "October 12th. The ritual is complete. The creature is bound. "
            "But the price... God forgive me, the price was Lily.", 0);
        next.next_node_id = 2;
        dialogue_add_choice(dialogue_get_node(tree, 1), &next);

        dialogue_add_node(tree, 2, "Professor Ashwood's Diary",
            "It will keep for a generation. The circle holds. "
            "But someone will come eventually. God help them.", 1);
        break;

    case 4: /* Child's Room – key */
        dialogue_add_node(tree, 0, "You",
            "A small brass key sits in the toy box, half-buried under "
            "a threadbare teddy bear.", 0);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "Lily",
            "That's for the door downstairs.", 0);
        next.next_node_id = 2;
        dialogue_add_choice(dialogue_get_node(tree, 1), &next);

        dialogue_add_node(tree, 2, "Lily",
            "Don't open it. Please. Whatever happens... "
            "don't open that door.", 1);
        break;

    case 40: /* Entrance Hall - mysterious stranger */
        dialogue_add_node(tree, 0, "Stranger",
            "You shouldn't be in this house. No one should.", 0);
        strncpy(next.text, "...", DIALOGUE_TEXT_MAX - 1);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "You",
            "Who are you? How did you get in here?", 0);
        next.next_node_id = 2;
        dialogue_add_choice(dialogue_get_node(tree, 1), &next);

        dialogue_add_node(tree, 2, "Stranger",
            "I have always been here. Long before you. Long before Ashwood. "
            "The house remembers everything.", 0);
        next.next_node_id = 3;
        dialogue_add_choice(dialogue_get_node(tree, 2), &next);

        dialogue_add_node(tree, 3, "Stranger",
            "Leave while you still can. Or stay — and learn what the house "
            "does to those who stay too long.", 1);
        break;

    case 5: /* Ritual Room */
        dialogue_add_node(tree, 0, "You",
            "The symbols carved into the floor pulse with a dull red light. "
            "The air tastes of iron and old fear.", 0);
        next.next_node_id = 1;
        dialogue_add_choice(dialogue_get_node(tree, 0), &next);

        dialogue_add_node(tree, 1, "You",
            "I can see the shape of the ritual now. A binding. "
            "Something was imprisoned here. Something that has been "
            "slowly... waking up.", 0);
        next.next_node_id = 2;
        dialogue_add_choice(dialogue_get_node(tree, 1), &next);

        dialogue_add_node(tree, 2, "Lily",
            "You found the truth. There is a name written in the centre. "
            "Speak it aloud and it ends. "
            "But only if you're sure. Only if you're brave enough.", 1);
        break;

    default:
        dialogue_add_node(tree, 0, "You",
            "There is nothing more to see here.", 1);
        break;
    }

    return tree;
}
