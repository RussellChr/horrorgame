#ifndef DIALOGUE_H
#define DIALOGUE_H

#define DIALOGUE_TEXT_MAX   512
#define CHOICE_TEXT_MAX     128
#define MAX_CHOICES         8
#define NPC_NAME_MAX        64

/* ── DialogueChoice ────────────────────────────────────────────────────── */

typedef struct {
    int  id;
    char text[CHOICE_TEXT_MAX];
    int  next_node_id;          /* ID of the DialogueNode reached        */
    int  requires_courage;      /* minimum courage needed (0 = always)   */
    int  requires_item_id;      /* item required to show this choice (0) */
    int  sanity_delta;          /* sanity change on selection            */
    int  courage_delta;         /* courage change on selection           */
    int  story_flag;            /* flag set when this choice is made     */
} DialogueChoice;

/* ── DialogueNode ──────────────────────────────────────────────────────── */

typedef struct {
    int            id;
    char           speaker[NPC_NAME_MAX];
    char           text[DIALOGUE_TEXT_MAX];
    int            choice_count;
    DialogueChoice choices[MAX_CHOICES];
    int            is_terminal;    /* 1 = conversation ends here         */
} DialogueNode;

/* ── Dialogue tree ─────────────────────────────────────────────────────── */

#define MAX_DIALOGUE_NODES 128

typedef struct {
    DialogueNode nodes[MAX_DIALOGUE_NODES];
    int          node_count;
} DialogueTree;

/* ── API ───────────────────────────────────────────────────────────────── */

DialogueTree *dialogue_tree_create(void);
void          dialogue_tree_destroy(DialogueTree *tree);

DialogueNode *dialogue_get_node(DialogueTree *tree, int node_id);
DialogueNode *dialogue_add_node(DialogueTree *tree, int id,
                                const char *speaker, const char *text,
                                int is_terminal);
void          dialogue_add_choice(DialogueNode *node,
                                  const DialogueChoice *choice);

/* Run an interactive conversation; returns the story_flag of the last
   chosen option (0 if no flag was set).                                 */
int dialogue_run(DialogueTree *tree, int start_node_id,
                 int player_courage, int player_item_id);

/* Print a single node's text and choices (for testing / replay).        */
void dialogue_print_node(const DialogueNode *node,
                         int player_courage, int player_item_id);

#endif /* DIALOGUE_H */
