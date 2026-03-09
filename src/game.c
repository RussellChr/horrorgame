#include "game.h"
#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Forward declarations ───────────────────────────────────────────────── */

static void game_setup_dialogue(GameState *gs);
static void game_setup_world(GameState *gs);
static void game_setup_items(GameState *gs);

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

GameState *game_init(const char *player_name, const GameConfig *cfg) {
    GameState *gs = calloc(1, sizeof(GameState));
    if (!gs) return NULL;

    if (cfg) gs->config = *cfg;

    /* Subsystem init */
    ui_init();
    utils_srand_seed((unsigned int)42); /* deterministic for now */

    gs->player   = player_create(player_name);
    gs->world    = world_create();
    gs->story    = story_create();
    gs->dialogue = dialogue_tree_create();
    gs->status   = GAME_STATUS_RUNNING;

    if (!gs->player || !gs->world || !gs->story || !gs->dialogue) {
        game_shutdown(gs);
        return NULL;
    }

    /* Populate world from assets */
    char loc_path[512];
    snprintf(loc_path, sizeof(loc_path), "%s/locations.txt",
             gs->config.asset_path[0] ? gs->config.asset_path : "assets");
    story_populate_world(gs->world, loc_path);

    game_setup_world(gs);
    game_setup_dialogue(gs);
    game_setup_items(gs);

    return gs;
}

void game_run(GameState *gs) {
    if (!gs) return;

    /* Show prologue */
    story_show_chapter_intro(gs->story);

    char cmd[256];
    while (gs->status == GAME_STATUS_RUNNING) {
        /* Refresh HUD and location description */
        ui_draw_hud(gs->player, gs->story);
        Location *loc = world_get_location(gs->world,
                                            gs->player->current_location_id);
        ui_show_location(loc);
        if (loc) loc->visited = 1;

        /* Danger zone penalty */
        if (loc && loc->is_danger_zone) {
            int sanity_loss = utils_rand_range(5, 15);
            player_modify_sanity(gs->player, -sanity_loss);
            ui_horror_flash("A suffocating dread fills the air...");
        }

        /* Check win/lose conditions */
        game_check_win_lose(gs);
        if (gs->status != GAME_STATUS_RUNNING) break;

        /* Get player command */
        ui_get_string("\n> ", cmd, sizeof(cmd));
        game_process_command(gs, cmd);
        gs->turn_count++;
    }

    /* End-game sequence */
    if (gs->status == GAME_STATUS_WIN) {
        Ending e = story_determine_ending(gs->story, gs->player);
        story_show_ending(e);
    } else if (gs->status == GAME_STATUS_LOSE) {
        ui_show_game_over();
    }
}

void game_shutdown(GameState *gs) {
    if (!gs) return;
    player_destroy(gs->player);
    world_destroy(gs->world);
    story_destroy(gs->story);
    dialogue_tree_destroy(gs->dialogue);
    ui_shutdown();
    free(gs);
}

/* ── Command processing ─────────────────────────────────────────────────── */

void game_process_command(GameState *gs, const char *cmd) {
    if (!gs || !cmd) return;

    char buf[256];
    strncpy(buf, cmd, sizeof(buf) - 1);

    /* Normalize to lowercase for matching */
    for (char *p = buf; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    }

    char *tok = utils_strtrim(buf);

    /* Movement */
    if (strcmp(tok, "north") == 0 || strcmp(tok, "n") == 0)
        game_handle_movement(gs, "north");
    else if (strcmp(tok, "south") == 0 || strcmp(tok, "s") == 0)
        game_handle_movement(gs, "south");
    else if (strcmp(tok, "east") == 0 || strcmp(tok, "e") == 0)
        game_handle_movement(gs, "east");
    else if (strcmp(tok, "west") == 0 || strcmp(tok, "w") == 0)
        game_handle_movement(gs, "west");
    else if (strcmp(tok, "up") == 0)
        game_handle_movement(gs, "up");
    else if (strcmp(tok, "down") == 0)
        game_handle_movement(gs, "down");

    /* Interaction */
    else if (strcmp(tok, "look") == 0 || strcmp(tok, "l") == 0)
        game_handle_look(gs);
    else if (strncmp(tok, "take ", 5) == 0)
        game_handle_take(gs, tok + 5);
    else if (strncmp(tok, "use ", 4) == 0)
        game_handle_use(gs, tok + 4);
    else if (strcmp(tok, "talk") == 0 || strcmp(tok, "t") == 0)
        game_handle_talk(gs);

    /* Info */
    else if (strcmp(tok, "inventory") == 0 || strcmp(tok, "i") == 0)
        game_handle_inventory(gs);
    else if (strcmp(tok, "status") == 0 || strcmp(tok, "stats") == 0)
        game_handle_status(gs);
    else if (strcmp(tok, "help") == 0 || strcmp(tok, "h") == 0 ||
             strcmp(tok, "?") == 0)
        game_handle_help();

    /* System */
    else if (strcmp(tok, "quit") == 0 || strcmp(tok, "q") == 0)
        gs->status = GAME_STATUS_QUIT;
    else
        printf("Unknown command. Type 'help' for a list of commands.\n");
}

/* ── Individual command handlers ─────────────────────────────────────────── */

void game_handle_movement(GameState *gs, const char *direction) {
    int new_id = gs->player->current_location_id;
    if (world_move(gs->world, gs->player->current_location_id,
                   direction, &new_id)) {
        gs->player->current_location_id = new_id;
        gs->player->steps_taken++;
        /* Trigger chapter advances based on location transitions */
        if (gs->story->current_chapter == CHAPTER_PROLOGUE &&
            gs->player->steps_taken >= 3)
            story_advance_chapter(gs->story, gs->player, gs->world);
    }
}

void game_handle_look(GameState *gs) {
    Location *loc = world_get_location(gs->world,
                                        gs->player->current_location_id);
    if (!loc) return;
    /* Force full description on explicit look */
    printf("\n%s\n", loc->description);
    printf("\nExits:");
    for (int i = 0; i < loc->exit_count; i++)
        printf(" [%s]", loc->exits[i].direction);
    printf("\n");
}

void game_handle_take(GameState *gs, const char *item_name) {
    Location *loc = world_get_location(gs->world,
                                        gs->player->current_location_id);
    if (!loc || loc->item_id == 0 || loc->item_taken) {
        printf("There is nothing to take here.\n");
        return;
    }
    /* Match item by ID (simplified) */
    Item it;
    memset(&it, 0, sizeof(it));
    it.id = loc->item_id;
    strncpy(it.name, item_name, ITEM_NAME_MAX - 1);
    strncpy(it.description, "An item you found in the house.",
            ITEM_DESC_MAX - 1);
    it.usable = 1;

    if (player_add_item(gs->player, &it)) {
        loc->item_taken = 1;
        printf("You pick up: %s\n", it.name);
    }
}

void game_handle_use(GameState *gs, const char *item_name) {
    /* Find the item in inventory by name */
    for (int i = 0; i < gs->player->inventory_count; i++) {
        if (utils_strncasecmp(gs->player->inventory[i].name,
                              item_name, ITEM_NAME_MAX) == 0) {
            Item *it = &gs->player->inventory[i];
            if (!it->usable) {
                printf("You can't use %s here.\n", it->name);
                return;
            }
            /* Generic use – specific items handled by story events */
            printf("You use the %s.\n", it->name);

            /* Using the key on the basement */
            if (it->id == ITEM_ID_BASEMENT_KEY) {
                player_set_flag(gs->player, FLAG_KEY_OBTAINED);
                story_trigger_event(gs->story, gs->player,
                                    gs->world, "open_basement");
            }
            return;
        }
    }
    printf("You don't have '%s'.\n", item_name);
}

void game_handle_talk(GameState *gs) {
    Location *loc = world_get_location(gs->world,
                                        gs->player->current_location_id);
    if (!loc) return;

    /* Lily is in location LOC_ID_LIBRARY */
    if (loc->id == LOC_ID_LIBRARY &&
        !player_check_flag(gs->player, FLAG_MET_LILY)) {
        story_trigger_event(gs->story, gs->player, gs->world, "meet_lily");
        player_set_flag(gs->player, FLAG_MET_LILY);
    }

    /* In the ritual room, studying the carvings makes the player monster-aware */
    if (loc->id == LOC_ID_RITUAL_ROOM) {
        story_trigger_event(gs->story, gs->player, gs->world, "study_creature");
        story_trigger_event(gs->story, gs->player, gs->world, "solve_puzzle");
        story_trigger_event(gs->story, gs->player, gs->world, "learn_truth");
    }

    /* Run the dialogue tree starting at node 0 */
    int flag = dialogue_run(gs->dialogue, 0,
                            gs->player->courage,
                            gs->player->inventory_count > 0
                                ? gs->player->inventory[0].id : 0);
    if (flag) player_set_flag(gs->player, flag);
}

void game_handle_inventory(GameState *gs) {
    player_show_inventory(gs->player);
}

void game_handle_status(GameState *gs) {
    player_print_stats(gs->player);
}

void game_handle_help(void) {
    printf("\n── Commands ────────────────────────────────\n");
    printf("  north / south / east / west / up / down\n");
    printf("  look (l)        – describe current room\n");
    printf("  take <item>     – pick up an item\n");
    printf("  use  <item>     – use an item\n");
    printf("  talk (t)        – talk to someone nearby\n");
    printf("  inventory (i)   – show your items\n");
    printf("  status / stats  – show your stats\n");
    printf("  help (h / ?)    – this help text\n");
    printf("  quit (q)        – quit the game\n");
    printf("────────────────────────────────────────────\n");
}

/* ── Win / lose checks ──────────────────────────────────────────────────── */

void game_check_win_lose(GameState *gs) {
    if (gs->player->health <= 0) {
        gs->status = GAME_STATUS_LOSE;
        return;
    }
    if (gs->player->sanity <= 0) {
        gs->status = GAME_STATUS_LOSE;
        printf("The darkness has consumed your mind entirely.\n");
        return;
    }
    /* Win condition: reached the finale chapter */
    if (gs->story->current_chapter == CHAPTER_FINALE) {
        gs->status = GAME_STATUS_WIN;
    }
}

/* ── Save / load (stubs) ─────────────────────────────────────────────────── */

int game_save(const GameState *gs, const char *filepath) {
    (void)gs; (void)filepath;
    printf("(Save not yet implemented.)\n");
    return 0;
}

int game_load(GameState *gs, const char *filepath) {
    (void)gs; (void)filepath;
    printf("(Load not yet implemented.)\n");
    return 0;
}

/* ── Internal setup helpers ─────────────────────────────────────────────── */

static void game_setup_world(GameState *gs) {
    /* If world_load_locations produced 0 rooms, seed a minimal fallback. */
    if (gs->world->location_count > 0) return;

    World *w = gs->world;

    /* Room 0 – Entrance Hall */
    Location *hall = &w->locations[w->location_count++];
    memset(hall, 0, sizeof(Location));
    hall->id = LOC_ID_ENTRANCE_HALL;
    strncpy(hall->name, "Entrance Hall",        LOCATION_NAME_MAX - 1);
    strncpy(hall->description,
            "A grand entrance hall draped in cobwebs. A faded portrait\n"
            "watches you from the far wall. Dust motes hang in the air.",
            LOCATION_DESC_MAX - 1);
    strncpy(hall->atmosphere,
            "The entrance hall. The portrait still watches.",
            LOCATION_DESC_MAX - 1);
    world_add_exit(hall, "north", LOC_ID_CORRIDOR, 0, 0);
    world_add_exit(hall, "east",  LOC_ID_LIBRARY, 0, 0);

    /* Room 1 – Corridor */
    Location *corridor = &w->locations[w->location_count++];
    memset(corridor, 0, sizeof(Location));
    corridor->id = LOC_ID_CORRIDOR;
    strncpy(corridor->name, "Dark Corridor",       LOCATION_NAME_MAX - 1);
    strncpy(corridor->description,
            "A long corridor with peeling wallpaper. Doors line both sides,\n"
            "all locked except the one at the far end.",
            LOCATION_DESC_MAX - 1);
    strncpy(corridor->atmosphere,
            "The dark corridor stretches ahead.",
            LOCATION_DESC_MAX - 1);
    corridor->is_danger_zone = 1;
    world_add_exit(corridor, "south", LOC_ID_ENTRANCE_HALL, 0, 0);
    world_add_exit(corridor, "north", LOC_ID_BASEMENT, 1, ITEM_ID_BASEMENT_KEY);

    /* Room 2 – Library */
    Location *library = &w->locations[w->location_count++];
    memset(library, 0, sizeof(Location));
    library->id = LOC_ID_LIBRARY;
    strncpy(library->name, "The Library",         LOCATION_NAME_MAX - 1);
    strncpy(library->description,
            "Shelves of rotting books reach the ceiling. A single candle\n"
            "burns on the reading table – yet you lit none.",
            LOCATION_DESC_MAX - 1);
    strncpy(library->atmosphere,
            "The library, silent except for the candle flame.",
            LOCATION_DESC_MAX - 1);
    library->item_id = ITEM_ID_DIARY;
    world_add_exit(library, "west", LOC_ID_ENTRANCE_HALL, 0, 0);
    world_add_exit(library, "north", LOC_ID_CHILDS_ROOM, 0, 0);

    /* Room 3 – Basement */
    Location *basement = &w->locations[w->location_count++];
    memset(basement, 0, sizeof(Location));
    basement->id = LOC_ID_BASEMENT;
    strncpy(basement->name, "Basement",            LOCATION_NAME_MAX - 1);
    strncpy(basement->description,
            "Stone steps descend into impenetrable darkness.\n"
            "Something shifts below. The air tastes of iron.",
            LOCATION_DESC_MAX - 1);
    strncpy(basement->atmosphere,
            "The basement. Cold. Dark. Waiting.",
            LOCATION_DESC_MAX - 1);
    basement->is_danger_zone = 1;
    world_add_exit(basement, "south", LOC_ID_CORRIDOR, 0, 0);
    world_add_exit(basement, "east",  LOC_ID_RITUAL_ROOM, 0, 0);

    /* Room 4 – Child's Room */
    Location *child_room = &w->locations[w->location_count++];
    memset(child_room, 0, sizeof(Location));
    child_room->id = LOC_ID_CHILDS_ROOM;
    strncpy(child_room->name, "Child's Room",      LOCATION_NAME_MAX - 1);
    strncpy(child_room->description,
            "A small room with a single bed and a music box on the dresser.\n"
            "The music box plays a lullaby on its own.",
            LOCATION_DESC_MAX - 1);
    strncpy(child_room->atmosphere,
            "The child's room. The music box still plays.",
            LOCATION_DESC_MAX - 1);
    child_room->item_id = ITEM_ID_BASEMENT_KEY;
    world_add_exit(child_room, "south", LOC_ID_LIBRARY, 0, 0);

    /* Room 5 – The Ritual Room */
    Location *truth = &w->locations[w->location_count++];
    memset(truth, 0, sizeof(Location));
    truth->id = LOC_ID_RITUAL_ROOM;
    strncpy(truth->name, "The Ritual Room",        LOCATION_NAME_MAX - 1);
    strncpy(truth->description,
            "Candles arranged in a circle illuminate strange symbols on the\n"
            "floor. At the centre lies an open book written in a language\n"
            "you should not be able to read – yet you can.",
            LOCATION_DESC_MAX - 1);
    strncpy(truth->atmosphere,
            "The ritual room. The circle still glows.",
            LOCATION_DESC_MAX - 1);
    truth->is_danger_zone = 1;
    world_add_exit(truth, "west", LOC_ID_BASEMENT, 0, 0);
}

static void game_setup_dialogue(GameState *gs) {
    DialogueTree *t = gs->dialogue;

    /* Node 0 – Lily introduction */
    DialogueNode *n0 = dialogue_add_node(t, 0, "Lily",
        "\"You found this place too, didn't you? "
        "They always come when the candle is lit.\"", 0);

    DialogueChoice c1 = {
        .id = 1, .text = "\"Who are you?\"",
        .next_node_id = 1, .requires_courage = 0,
        .sanity_delta = 0, .courage_delta = 5, .story_flag = 0
    };
    DialogueChoice c2 = {
        .id = 2, .text = "\"I mean you no harm.\"",
        .next_node_id = 2, .requires_courage = 30,
        .sanity_delta = 0, .courage_delta = 10,
        .story_flag = FLAG_LILY_TRUSTS_PLAYER
    };
    DialogueChoice c3 = {
        .id = 3, .text = "[Stay silent]",
        .next_node_id = 3, .requires_courage = 0,
        .sanity_delta = -5, .courage_delta = 0, .story_flag = 0
    };
    dialogue_add_choice(n0, &c1);
    dialogue_add_choice(n0, &c2);
    dialogue_add_choice(n0, &c3);

    /* Node 1 – Who are you */
    dialogue_add_node(t, 1, "Lily",
        "\"I am Lily. I have lived here since the night of the ritual.\n"
        "I cannot leave. But you might be able to help.\"", 0);
    DialogueChoice c1b = {
        .id = 1, .text = "\"How can I help?\"",
        .next_node_id = 4, .requires_courage = 0,
        .sanity_delta = 0, .courage_delta = 5,
        .story_flag = FLAG_MET_LILY
    };
    dialogue_add_choice(
        dialogue_get_node(t, 1), &c1b);

    /* Node 2 – Trust branch */
    dialogue_add_node(t, 2, "Lily",
        "She studies you for a long moment, then nods slowly.\n"
        "\"Find the book in the ritual room. Read the last page aloud.\n"
        "Do not stop reading, no matter what you hear.\"", 1);

    /* Node 3 – Silence branch */
    dialogue_add_node(t, 3, "Lily",
        "Lily tilts her head. \"You are afraid. That is wise.\"\n"
        "She fades back into the shadows without another word.", 1);

    /* Node 4 – How to help */
    dialogue_add_node(t, 4, "Lily",
        "\"In the ritual room, beneath the floor, there is a book.\n"
        "Bring it here and I will show you what must be done.\"\n"
        "Her eyes are ancient despite her young face.", 1);
}

static void game_setup_items(GameState *gs) {
    /* Pre-place a candle in the player's starting inventory. */
    Item candle = {
        .id = ITEM_ID_CANDLE,
        .usable = 1,
    };
    strncpy(candle.name, "Candle", ITEM_NAME_MAX - 1);
    strncpy(candle.description,
            "A half-burned candle. It flickers even in still air.",
            ITEM_DESC_MAX - 1);
    player_add_item(gs->player, &candle);
}
