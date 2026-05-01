#include "savegame.h"

#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

/* ── Slot paths ────────────────────────────────────────────────────────── */

static const char *slot_paths[SAVE_SLOT_COUNT] = {
    "saves/save_1.bin",
    "saves/save_2.bin",
    "saves/save_3.bin"
};

/* ── Public API ─────────────────────────────────────────────────────────── */

const char *savegame_slot_path(int slot)
{
    if (slot < 1 || slot > SAVE_SLOT_COUNT) return NULL;
    return slot_paths[slot - 1];
}

int savegame_exists(int slot)
{
    const char *path = savegame_slot_path(slot);
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int savegame_read_label(int slot, char *label_out, int label_max)
{
    SaveData data;
    if (!savegame_read(slot, &data)) return 0;
    if (label_out && label_max > 0) {
        strncpy(label_out, data.save_label, (size_t)(label_max - 1));
        label_out[label_max - 1] = '\0';
    }
    return 1;
}

int savegame_write(int slot, const SaveData *data)
{
    if (!data) return 0;

    /* Ensure the saves/ directory exists */
    SDL_CreateDirectory("saves");

    const char *path = savegame_slot_path(slot);
    if (!path) return 0;

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = (fwrite(data, sizeof(SaveData), 1, f) == 1);
    fclose(f);
    return ok;
}

int savegame_read(int slot, SaveData *data)
{
    if (!data) return 0;

    const char *path = savegame_slot_path(slot);
    if (!path) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int ok = (fread(data, sizeof(SaveData), 1, f) == 1);
    fclose(f);

    if (!ok) return 0;
    /* Validate magic bytes */
    if (memcmp(data->magic, SAVE_MAGIC, 7) != 0) return 0;
    return 1;
}
