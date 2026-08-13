#include "mahjongg_game.h"

struct mj_move {
    int a;
    int b;
};

struct mj_game_state {
    struct mj_tile tiles[MJ_MAX_TILES];

    int grid[MJ_TILE_ROWS][MJ_TILE_COLS][MJ_TILE_LEVS];

    int tile_count;
    int remaining;
    int moves;

    int cursor_tile;
    int selected_tile;

    struct mj_move undo_stack[MJ_MAX_TILES / 2];
    int undo_count;
};

static struct mj_game_state game;

/* ------------------------------------------------------------------------ */
/* Utility                                                                   */
/* ------------------------------------------------------------------------ */

static void clear_grid(void)
{
    int r;
    int c;
    int l;

    for (r = 0; r < MJ_TILE_ROWS; r++) {
        for (c = 0; c < MJ_TILE_COLS; c++) {
            for (l = 0; l < MJ_TILE_LEVS; l++) {
                game.grid[r][c][l] = -1;
            }
        }
    }
}

static bool grid_in_bounds(int r, int c, int l)
{
    return r >= 0 && r < MJ_TILE_ROWS &&
           c >= 0 && c < MJ_TILE_COLS &&
           l >= 0 && l < MJ_TILE_LEVS;
}

static int grid_tile(int r, int c, int l)
{
    if (!grid_in_bounds(r, c, l)) {
        return -1;
    }

    return game.grid[r][c][l];
}

static bool cell_has_real_tile(int r, int c, int l)
{
    int idx = grid_tile(r, c, l);

    if (idx < 0) {
        return false;
    }

    return game.tiles[idx].real && !game.tiles[idx].removed;
}

bool mj_tile_open(const struct mj_tile *tile)
{
    if (tile == NULL) {
        return false;
    }

    return tile->real &&
           !tile->removed &&
           tile->blocked == 0 &&
           tile->coverage == 0;
}

bool mj_game_tile_open(int index)
{
    if (index < 0 || index >= game.tile_count) {
        return false;
    }

    return mj_tile_open(&game.tiles[index]);
}

/* ------------------------------------------------------------------------ */
/* Grid and blockage, based on xmahjongg Game::init_grid/init_blockage        */
/* ------------------------------------------------------------------------ */

static void build_grid(void)
{
    int i;
    int rr;
    int cc;

    clear_grid();

    for (i = 0; i < game.tile_count; i++) {
        struct mj_tile *tile = &game.tiles[i];

        if (!tile->real || tile->removed) {
            continue;
        }

        /*
         * xmahjongg represents each tile as occupying a 2x2 area in the
         * logical grid. This is important for half-step layouts.
         */
        for (rr = tile->row; rr < tile->row + 2; rr++) {
            for (cc = tile->col; cc < tile->col + 2; cc++) {
                if (grid_in_bounds(rr, cc, tile->lev)) {
                    game.grid[rr][cc][tile->lev] = i;
                }
            }
        }
    }
}

static void check_level_blockage(int r, int c, int l)
{
    int idx;
    bool left_blocked;
    bool right_blocked;
    struct mj_tile *tile;

    idx = grid_tile(r, c, l);

    if (idx < 0) {
        return;
    }

    tile = &game.tiles[idx];

    if (!tile->real || tile->removed) {
        return;
    }

    /*
     * xmahjongg normalizes to the tile's origin.
     */
    r = tile->row;
    c = tile->col;

    left_blocked =
        cell_has_real_tile(r,     c - 1, l) ||
        cell_has_real_tile(r + 1, c - 1, l);

    right_blocked =
        cell_has_real_tile(r,     c + 2, l) ||
        cell_has_real_tile(r + 1, c + 2, l);

    tile->blocked = left_blocked && right_blocked;
}

static void init_blockage(void)
{
    int i;
    int r;
    int c;

    build_grid();

    for (i = 0; i < game.tile_count; i++) {
        game.tiles[i].coverage = 0;
        game.tiles[i].blocked = 0;
        game.tiles[i].mark = 0;
    }

    /*
     * Coverage from tiles above:
     * xmahjongg increments coverage on the directly lower level.
     */
    for (i = 0; i < game.tile_count; i++) {
        struct mj_tile *tile = &game.tiles[i];

        if (!tile->real || tile->removed) {
            continue;
        }

        if (tile->lev > 0) {
            for (r = tile->row; r < tile->row + 2; r++) {
                for (c = tile->col; c < tile->col + 2; c++) {
                    int below = grid_tile(r, c, tile->lev - 1);

                    if (below >= 0) {
                        game.tiles[below].coverage++;
                    }
                }
            }
        }
    }

    /*
     * Side blockage on same level.
     */
    for (i = 0; i < game.tile_count; i++) {
        struct mj_tile *tile = &game.tiles[i];

        if (!tile->real || tile->removed) {
            continue;
        }

        check_level_blockage(tile->row, tile->col, tile->lev);
    }
}

/* ------------------------------------------------------------------------ */
/* Layout                                                                    */
/* ------------------------------------------------------------------------ */

static bool place_tile(int row, int col, int lev)
{
    struct mj_tile *tile;

    if (game.tile_count >= MJ_MAX_TILES) {
        return false;
    }

    if (!(row > 1 && col > 1 &&
          row < MJ_TILE_ROWS - 3 &&
          col < MJ_TILE_COLS - 3 &&
          lev >= 0 && lev < MJ_TILE_LEVS - 1)) {
        return false;
    }

    tile = &game.tiles[game.tile_count];

    tile->real = true;
    tile->removed = false;
    tile->number = game.tile_count;

    /*
     * Phase 1:
     * Keep original xmahjongg concept of 36 matching groups.
     * This is not yet the original image/picture mapping, but every group has
     * four matching tiles, which is correct for Mahjongg Solitaire.
     */
    tile->match = game.tile_count / 4;

    tile->row = row;
    tile->col = col;
    tile->lev = lev;

    tile->coverage = 0;
    tile->blocked = 0;
    tile->mark = 0;

    game.tile_count++;
    game.remaining++;

    return true;
}

static void layout_default(void)
{
    int i;
    int j;

    /*
     * This is a direct C port of xmahjongg Game::layout_default().
     */

    for (j = 2; j <= 13; j++) {
        place_tile(2,  j * 2, 0);
        place_tile(8,  j * 2, 0);
        place_tile(10, j * 2, 0);
        place_tile(16, j * 2, 0);
    }

    for (j = 3; j <= 12; j++) {
        place_tile(6,  j * 2, 0);
        place_tile(12, j * 2, 0);
    }

    for (j = 4; j <= 11; j++) {
        place_tile(4,  j * 2, 0);
        place_tile(14, j * 2, 0);
    }

    for (j = 5; j <= 10; j++) {
        for (i = 2; i <= 7; i++) {
            place_tile(i * 2, j * 2, 1);
        }
    }

    for (j = 6; j <= 9; j++) {
        for (i = 3; i <= 6; i++) {
            place_tile(i * 2, j * 2, 2);
        }
    }

    for (j = 7; j <= 8; j++) {
        for (i = 4; i <= 5; i++) {
            place_tile(i * 2, j * 2, 3);
        }
    }

    place_tile(9, 2, 0);
    place_tile(9, 28, 0);
    place_tile(9, 30, 0);
    place_tile(9, 15, 4);
}

/* ------------------------------------------------------------------------ */
/* Cursor                                                                    */
/* ------------------------------------------------------------------------ */

static int find_next_open_tile(int start, int direction)
{
    int i;
    int idx;

    if (game.tile_count <= 0) {
        return -1;
    }

    idx = start;

    for (i = 0; i < game.tile_count; i++) {
        idx += direction;

        if (idx < 0) {
            idx = game.tile_count - 1;
        } else if (idx >= game.tile_count) {
            idx = 0;
        }

        if (mj_game_tile_open(idx)) {
            return idx;
        }
    }

    return -1;
}

void mj_game_cursor_next(void)
{
    int next = find_next_open_tile(game.cursor_tile, 1);

    if (next >= 0) {
        game.cursor_tile = next;
    }
}

void mj_game_cursor_prev(void)
{
    int prev = find_next_open_tile(game.cursor_tile, -1);

    if (prev >= 0) {
        game.cursor_tile = prev;
    }
}

/* ------------------------------------------------------------------------ */
/* Game actions                                                              */
/* ------------------------------------------------------------------------ */

static bool tiles_match(int a, int b)
{
    if (a < 0 || b < 0 ||
        a >= game.tile_count || b >= game.tile_count ||
        a == b) {
        return false;
    }

    if (!mj_game_tile_open(a) || !mj_game_tile_open(b)) {
        return false;
    }

    return game.tiles[a].match == game.tiles[b].match;
}

static void deselect(void)
{
    game.selected_tile = -1;
}

static void remove_pair(int a, int b)
{
    if (game.undo_count < (MJ_MAX_TILES / 2)) {
        game.undo_stack[game.undo_count].a = a;
        game.undo_stack[game.undo_count].b = b;
        game.undo_count++;
    }

    game.tiles[a].removed = true;
    game.tiles[b].removed = true;

    game.remaining -= 2;
    game.moves++;

    deselect();

    init_blockage();

    if (!mj_game_tile_open(game.cursor_tile)) {
        game.cursor_tile = find_next_open_tile(game.cursor_tile, 1);
    }
}

enum mj_select_result mj_game_select_current(void)
{
    int cur = game.cursor_tile;

    if (cur < 0 || cur >= game.tile_count) {
        return MJ_SELECT_NONE;
    }

    if (!mj_game_tile_open(cur)) {
        return MJ_SELECT_NONE;
    }

    if (game.selected_tile < 0) {
        game.selected_tile = cur;
        return MJ_SELECT_CHANGED;
    }

    if (game.selected_tile == cur) {
        deselect();
        return MJ_SELECT_CHANGED;
    }

    if (tiles_match(game.selected_tile, cur)) {
        remove_pair(game.selected_tile, cur);
        return MJ_SELECT_REMOVED_PAIR;
    }

    game.selected_tile = cur;
    return MJ_SELECT_CHANGED;
}

bool mj_game_undo(void)
{
    struct mj_move move;

    if (game.undo_count <= 0) {
        return false;
    }

    game.undo_count--;

    move = game.undo_stack[game.undo_count];

    if (move.a < 0 || move.b < 0 ||
        move.a >= game.tile_count || move.b >= game.tile_count) {
        return false;
    }

    game.tiles[move.a].removed = false;
    game.tiles[move.b].removed = false;

    game.remaining += 2;

    if (game.moves > 0) {
        game.moves--;
    }

    deselect();

    init_blockage();

    game.cursor_tile = move.a;

    if (!mj_game_tile_open(game.cursor_tile)) {
        game.cursor_tile = find_next_open_tile(-1, 1);
    }

    return true;
}

bool mj_game_find_hint(int *a, int *b)
{
    int i;
    int j;

    for (i = 0; i < game.tile_count; i++) {
        if (!mj_game_tile_open(i)) {
            continue;
        }

        for (j = i + 1; j < game.tile_count; j++) {
            if (!mj_game_tile_open(j)) {
                continue;
            }

            if (game.tiles[i].match == game.tiles[j].match) {
                if (a != NULL) {
                    *a = i;
                }

                if (b != NULL) {
                    *b = j;
                }

                return true;
            }
        }
    }

    return false;
}

int mj_game_possible_moves(void)
{
    int count = 0;
    int i;
    int j;

    for (i = 0; i < game.tile_count; i++) {
        if (!mj_game_tile_open(i)) {
            continue;
        }

        for (j = i + 1; j < game.tile_count; j++) {
            if (!mj_game_tile_open(j)) {
                continue;
            }

            if (game.tiles[i].match == game.tiles[j].match) {
                count++;
            }
        }
    }

    return count;
}

/* ------------------------------------------------------------------------ */
/* Public accessors                                                          */
/* ------------------------------------------------------------------------ */

void mj_game_init_default(unsigned int seed)
{
    (void)seed;

    game.tile_count = 0;
    game.remaining = 0;
    game.moves = 0;
    game.cursor_tile = -1;
    game.selected_tile = -1;
    game.undo_count = 0;

    layout_default();
    init_blockage();

    game.cursor_tile = find_next_open_tile(-1, 1);
}

int mj_game_tile_count(void)
{
    return game.tile_count;
}

const struct mj_tile *mj_game_tile(int index)
{
    if (index < 0 || index >= game.tile_count) {
        return NULL;
    }

    return &game.tiles[index];
}

int mj_game_remaining(void)
{
    return game.remaining;
}

int mj_game_moves(void)
{
    return game.moves;
}

int mj_game_cursor_tile(void)
{
    return game.cursor_tile;
}

int mj_game_selected_tile(void)
{
    return game.selected_tile;
}
