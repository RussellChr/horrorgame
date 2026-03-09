#include "dialogue.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

DialogueTree *dialogue_tree_create(void) {
    return calloc(1, sizeof(DialogueTree));
}

void dialogue_tree_destroy(DialogueTree *tree) {
    free(tree);
}

/* ── Node management ───────────────────────────────────────────────────── */

DialogueNode *dialogue_get_node(DialogueTree *tree, int node_id) {
    if (!tree) return NULL;
    for (int i = 0; i < tree->node_count; i++) {
        if (tree->nodes[i].id == node_id)
            return &tree->nodes[i];
    }
    return NULL;
}

DialogueNode *dialogue_add_node(DialogueTree *tree, int id,
                                const char *speaker, const char *text,
                                int is_terminal) {
    if (!tree || tree->node_count >= MAX_DIALOGUE_NODES) return NULL;
    DialogueNode *node = &tree->nodes[tree->node_count++];
    memset(node, 0, sizeof(DialogueNode));
    node->id          = id;
    node->is_terminal = is_terminal;
    strncpy(node->speaker, speaker ? speaker : "", NPC_NAME_MAX - 1);
    strncpy(node->text,    text    ? text    : "", DIALOGUE_TEXT_MAX - 1);
    return node;
}

void dialogue_add_choice(DialogueNode *node, const DialogueChoice *choice) {
    if (!node || !choice || node->choice_count >= MAX_CHOICES) return;
    node->choices[node->choice_count++] = *choice;
}

/* ── Printing ──────────────────────────────────────────────────────────── */

void dialogue_print_node(const DialogueNode *node,
                         int player_courage, int player_item_id) {
    if (!node) return;
    printf("\n%s: \"%s\"\n", node->speaker, node->text);

    if (node->is_terminal || node->choice_count == 0) return;

    printf("\nWhat do you say?\n");
    int shown = 0;
    for (int i = 0; i < node->choice_count; i++) {
        const DialogueChoice *c = &node->choices[i];
        /* gate by courage and item requirements */
        if (c->requires_courage > player_courage) continue;
        if (c->requires_item_id && c->requires_item_id != player_item_id)
            continue;
        printf("  [%d] %s\n", ++shown, c->text);
    }
}

/* ── Interactive run ───────────────────────────────────────────────────── */

int dialogue_run(DialogueTree *tree, int start_node_id,
                 int player_courage, int player_item_id) {
    if (!tree) return 0;

    int current_id = start_node_id;
    int last_flag  = 0;

    while (1) {
        DialogueNode *node = dialogue_get_node(tree, current_id);
        if (!node) break;

        dialogue_print_node(node, player_courage, player_item_id);

        if (node->is_terminal || node->choice_count == 0) break;

        /* Build list of available choices */
        int available[MAX_CHOICES];
        int count = 0;
        for (int i = 0; i < node->choice_count; i++) {
            const DialogueChoice *c = &node->choices[i];
            if (c->requires_courage > player_courage) continue;
            if (c->requires_item_id && c->requires_item_id != player_item_id)
                continue;
            available[count++] = i;
        }

        if (count == 0) break;

        printf("\nChoice (1-%d): ", count);
        fflush(stdout);
        int sel = 0;
        if (scanf("%d", &sel) != 1) sel = 1;
        /* consume rest of line */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);

        sel = utils_clamp(sel, 1, count) - 1;
        const DialogueChoice *chosen = &node->choices[available[sel]];

        last_flag  = chosen->story_flag;
        current_id = chosen->next_node_id;
    }

    return last_flag;
}
