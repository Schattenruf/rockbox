/***************************************************************************
 * Rockbox Mahjongg Plugin - early skeleton
 *
 * Based structurally on Rockbox plugins such as Sokoban and Solitaire.
 * Game logic will later be filled from xmahjongg:
 *   - tile.c / tile.h
 *   - game.c / game.h
 *   - solvable.c / solvable.h
 *
 ****************************************************************************/

#include "plugin.h"
#include "mahjongg_game.h"
#include "mahjongg_game.c"
#include "pluginbitmaps/mahjongg_tiles.h"
/* #include "lib/playback_control.h" */

/* ------------------------------------------------------------------------ */
/* Basic metadata                                                            */
/* ------------------------------------------------------------------------ */

#define MAHJONGG_TITLE "Mahjongg"

#define MAHJONGG_SAVE_FILE PLUGIN_GAMES_DATA_DIR "/mahjongg.save"
#define MAHJONGG_SCORE_FILE PLUGIN_GAMES_DATA_DIR "/mahjongg.score"

/* ------------------------------------------------------------------------ */
/* Display constants                                                         */
/* ------------------------------------------------------------------------ */

/*
 * First test sizes.
 * Later these should be derived from actual bitmap dimensions.
 */
#define MJ_TILE_GAP_PX 1
#define MJ_TILE_W       28
#define MJ_TILE_H       36
#define MJ_TILE_XSTEP   11
#define MJ_TILE_YSTEP   14
#define MJ_LEVEL_DX      5
#define MJ_VISUAL_ROW_NUDGE_PX 8
#define MJ_VISUAL_RIGHT_NUDGE_X 5
#define MJ_VISUAL_RIGHT_NUDGE_Y 2
#define MJ_VISUAL_LEFT_PULL_X   -8
#define MJ_VISUAL_LEFT_PULL_Y    5
#define MJ_VISUAL_RIGHT_PULL_X  12
#define MJ_VISUAL_RIGHT_PULL_Y   5
#define MJ_VISUAL_OPEN_PULL_X   7
#define MJ_VISUAL_OPEN_PULL_Y   2
#define MJ_VISUAL_EXACT_RIGHT_PULL_X 16
#define MJ_VISUAL_EXACT_RIGHT_PULL_Y 4
#define MJ_LEVEL_DY     -5

#define MJ_MAX_TILES   144
#define MJ_ROWS         24
#define MJ_COLS         38
#define MJ_LEVS         12

#define MJ_FONT FONT_SYSFIXED

#if LCD_DEPTH > 1
#define MJ_TABLE_BG      LCD_RGBPACK(18, 78, 28)
#define MJ_TABLE_DARK    LCD_RGBPACK(12, 55, 22)
#define MJ_TABLE_LIGHT   LCD_RGBPACK(24, 92, 36)
#endif

/* ------------------------------------------------------------------------ */
/* iPod / target keymap                                                      */
/* ------------------------------------------------------------------------ */

#if (CONFIG_KEYPAD == IPOD_4G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_1G2G_PAD)

/*
 * iPod-style control proposal:
 *
 * Wheel back/fwd  : previous/next selectable tile
 * Select          : select tile / remove pair
 * Select hold     : menu
 * Menu short      : hint
 * Play            : undo
 * Left/Right      : optional alternate navigation
 */
#define MJ_PREV             BUTTON_SCROLL_BACK
#define MJ_NEXT             BUTTON_SCROLL_FWD

#define MJ_SELECT_PRE       BUTTON_SELECT
#define MJ_SELECT           (BUTTON_SELECT | BUTTON_REL)

#define MJ_MENU             (BUTTON_MENU | BUTTON_REL)

#define MJ_HINT_PRE         BUTTON_LEFT
#define MJ_HINT             (BUTTON_LEFT | BUTTON_REL)

#define MJ_UNDO             BUTTON_PLAY

/*#define MJ_LEFT_PRE         BUTTON_LEFT
#define MJ_LEFT             (BUTTON_LEFT | BUTTON_REL)

#define MJ_RIGHT_PRE        BUTTON_RIGHT
#define MJ_RIGHT            (BUTTON_RIGHT | BUTTON_REL)
*/
#define MJ_KEYS_PREV_NEXT   "SCROLL"
#define MJ_KEYS_SELECT      "SELECT"
#define MJ_KEYS_MENU        "MENU"
#define MJ_KEYS_HINT        "LEFT"
#define MJ_KEYS_UNDO        "PLAY"

#else

/*
 * Generic fallback for non-iPod targets.
 * This is intentionally simple and may need adjustment per target.
 */
#define MJ_PREV             BUTTON_LEFT
#define MJ_NEXT             BUTTON_RIGHT
#define MJ_SELECT           BUTTON_SELECT
#define MJ_MENU             BUTTON_POWER
#define MJ_HINT             BUTTON_UP
#define MJ_UNDO             BUTTON_DOWN

#define MJ_KEYS_PREV_NEXT   "LEFT/RIGHT"
#define MJ_KEYS_SELECT      "SELECT"
#define MJ_KEYS_MENU        "POWER"
#define MJ_KEYS_HINT        "UP"
#define MJ_KEYS_UNDO        "DOWN"

#endif

#if defined(MJ_SELECT_PRE) || defined(MJ_HINT_PRE) || \
    defined(MJ_LEFT_PRE) || defined(MJ_RIGHT_PRE)
#define MJ_NEED_LASTBUTTON
#endif

static int hint_tile_a = -1;
static int hint_tile_b = -1;

static void clear_hint(void)
{
    hint_tile_a = -1;
    hint_tile_b = -1;
}

#define MJ_STOPWATCH_SAVE_MAGIC 0x4d4a5731u
#define MJ_SCORE_MAGIC 0x4d4a5331u
#define MJ_CURRENT_LAYOUT_ID 0

static bool mj_stopwatch_enabled = true;
static long mj_stopwatch_start_tick = 0;
static long mj_best_elapsed_ticks = -1;

struct mj_stopwatch_save_state {
    unsigned int magic;
    unsigned int stopwatch_enabled;
    long elapsed_ticks;
};

struct mj_score_record {
    unsigned int magic;
    int layout_id;
    long best_ticks;
};

static void mj_stopwatch_reset(void)
{
    mj_stopwatch_start_tick = *rb->current_tick;
}

static long mj_stopwatch_elapsed_ticks(void)
{
    if (!mj_stopwatch_enabled) {
        return -1;
    }

    return *rb->current_tick - mj_stopwatch_start_tick;
}

static void mj_format_time(long ticks, char *buffer, int buffer_size)
{
    int total_seconds;
    int minutes;
    int seconds;

    if (ticks < 0) {
        rb->snprintf(buffer, buffer_size, "--:--");
        return;
    }

    total_seconds = ticks / HZ;
    minutes = total_seconds / 60;
    seconds = total_seconds % 60;

    rb->snprintf(buffer, buffer_size, "%d:%02d", minutes, seconds);
}

static bool mj_stopwatch_save(int fd)
{
    struct mj_stopwatch_save_state state;

    state.magic = MJ_STOPWATCH_SAVE_MAGIC;
    state.stopwatch_enabled = mj_stopwatch_enabled ? 1u : 0u;
    state.elapsed_ticks = mj_stopwatch_elapsed_ticks();

    if (state.elapsed_ticks < 0) {
        state.elapsed_ticks = 0;
    }

    return rb->write(fd, &state, sizeof(state)) == (ssize_t)sizeof(state);
}

static void mj_stopwatch_load(int fd)
{
    struct mj_stopwatch_save_state state;
    long elapsed;

    if (rb->read(fd, &state, sizeof(state)) != (ssize_t)sizeof(state)) {
        mj_stopwatch_enabled = true;
        mj_stopwatch_reset();
        return;
    }

    if (state.magic != MJ_STOPWATCH_SAVE_MAGIC) {
        mj_stopwatch_enabled = true;
        mj_stopwatch_reset();
        return;
    }

    mj_stopwatch_enabled = state.stopwatch_enabled != 0;
    elapsed = state.elapsed_ticks;

    if (elapsed < 0) {
        elapsed = 0;
    }

    mj_stopwatch_start_tick = *rb->current_tick - elapsed;
}

#define MJ_SCORE_VERSION 1u
#define MJ_MAX_LAYOUT_RECORDS 72
#define MJ_SCORE_EMPTY 0xffffffffu

struct mj_score_file {
    unsigned int magic;
    unsigned int version;
    unsigned int count;
    unsigned int best_seconds[MJ_MAX_LAYOUT_RECORDS];
};

static void mj_score_file_init(struct mj_score_file *scores)
{
    int i;

    scores->magic = MJ_SCORE_MAGIC;
    scores->version = MJ_SCORE_VERSION;
    scores->count = MJ_MAX_LAYOUT_RECORDS;

    for (i = 0; i < MJ_MAX_LAYOUT_RECORDS; i++) {
        scores->best_seconds[i] = MJ_SCORE_EMPTY;
    }
}

static bool mj_score_file_read(struct mj_score_file *scores)
{
    int fd;
    bool ok;

    mj_score_file_init(scores);

    fd = rb->open(MAHJONGG_SCORE_FILE, O_RDONLY);

    if (fd < 0) {
        return false;
    }

    ok = rb->read(fd, scores, sizeof(*scores)) == (ssize_t)sizeof(*scores);
    rb->close(fd);

    if (!ok ||
        scores->magic != MJ_SCORE_MAGIC ||
        scores->version != MJ_SCORE_VERSION ||
        scores->count != MJ_MAX_LAYOUT_RECORDS) {
        mj_score_file_init(scores);
        return false;
    }

    return true;
}

static bool mj_score_file_write(const struct mj_score_file *scores)
{
    int fd;
    bool ok;

    rb->mkdir(PLUGIN_GAMES_DATA_DIR);

    fd = rb->open(MAHJONGG_SCORE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0) {
        return false;
    }

    ok = rb->write(fd, scores, sizeof(*scores)) == (ssize_t)sizeof(*scores);
    rb->close(fd);

    return ok;
}

static void mj_score_load_best(void)
{
    struct mj_score_file scores;
    unsigned int best_seconds;

    mj_best_elapsed_ticks = -1;

    mj_score_file_read(&scores);

    if (MJ_CURRENT_LAYOUT_ID < 0 ||
        MJ_CURRENT_LAYOUT_ID >= MJ_MAX_LAYOUT_RECORDS) {
        return;
    }

    best_seconds = scores.best_seconds[MJ_CURRENT_LAYOUT_ID];

    if (best_seconds == MJ_SCORE_EMPTY) {
        return;
    }

    mj_best_elapsed_ticks = (long)best_seconds * HZ;
}

static void mj_score_save_best(long elapsed_ticks)
{
    struct mj_score_file scores;
    unsigned int elapsed_seconds;
    unsigned int current_best;

    if (elapsed_ticks < 0) {
        return;
    }

    if (MJ_CURRENT_LAYOUT_ID < 0 ||
        MJ_CURRENT_LAYOUT_ID >= MJ_MAX_LAYOUT_RECORDS) {
        return;
    }

    elapsed_seconds = (unsigned int)((elapsed_ticks + HZ - 1) / HZ);

    mj_score_file_read(&scores);

    current_best = scores.best_seconds[MJ_CURRENT_LAYOUT_ID];

    if (current_best != MJ_SCORE_EMPTY &&
        elapsed_seconds >= current_best) {
        mj_best_elapsed_ticks = (long)current_best * HZ;
        return;
    }

    scores.best_seconds[MJ_CURRENT_LAYOUT_ID] = elapsed_seconds;

    if (mj_score_file_write(&scores)) {
        mj_best_elapsed_ticks = (long)elapsed_seconds * HZ;
    }
}

static void draw_table_background(void)
{
#if LCD_DEPTH > 1
    int x;
    int y;

    /*
     * Subtle felt texture.
     * Keep the status area readable and avoid strong horizontal lines.
     */
    rb->lcd_set_foreground(LCD_RGBPACK(13, 65, 24));

    for (y = 38; y < LCD_HEIGHT; y += 18) {
        for (x = ((y / 18) & 1) ? 9 : 0; x < LCD_WIDTH; x += 18) {
            rb->lcd_fillrect(x, y, 1, 1);
        }
    }

    rb->lcd_set_foreground(LCD_RGBPACK(22, 90, 34));

    for (y = 46; y < LCD_HEIGHT; y += 28) {
        for (x = ((y / 28) & 1) ? 12 : 4; x < LCD_WIDTH; x += 30) {
            rb->lcd_fillrect(x, y, 1, 1);
        }
    }
#endif
}

static void start_new_game(void)
{
    clear_hint();
    mj_game_init_default((unsigned int)*rb->current_tick);
    mj_score_load_best();
    mj_stopwatch_reset();
}

static void undo_move(void)
{
    clear_hint();

    if (!mj_game_undo()) {
        rb->splash(HZ / 2, "Nothing to undo");
    }
}

static void show_hint(void)
{
    int a;
    int b;

    if (mj_game_find_hint(&a, &b)) {
        hint_tile_a = a;
        hint_tile_b = b;
    } else {
        clear_hint();
        rb->splash(HZ, "No moves");
    }
}

/* ------------------------------------------------------------------------ */
/* Positioning and rendering                                                 */
/* ------------------------------------------------------------------------ */

static void tile_position(const struct mj_tile *t, int *x, int *y)
{
    /*
     * Simplified xmahjongg Board::position():
     *
     * x = layout_x + tile_w * (col / 2)
     *             + half tile if odd col
     *             + level shadow offset
     *
     * y = layout_y + tile_h * (row / 2)
     *             + half tile if odd row
     *             + level shadow offset
     */

    const int layout_x = 16;
    const int layout_y = 4;

    *x = layout_x
       + MJ_TILE_XSTEP * t->col
       + MJ_LEVEL_DX * t->lev;

    *y = layout_y
       + MJ_TILE_YSTEP * t->row
       + MJ_LEVEL_DY * t->lev;




}

static void draw_tile_box(int x, int y, int picture, int match, int level,
                          bool open, bool selected, bool cursor, bool hint)
{
#if LCD_DEPTH > 1
    int oldfg = rb->lcd_get_foreground();
#endif
    int max_picture;
    int normal_picture_count;
    int sprite_picture;
    int shadow;
    int gap;
    int tx;
    int ty;
    int tw;
    int th;

    (void)match;

    gap = MJ_TILE_GAP_PX;
    tx = x + gap;
    ty = y + gap;
    tw = MJ_TILE_W - gap * 2;
    th = MJ_TILE_H - gap * 2;

    if (tw < 4) {
        tw = MJ_TILE_W;
        tx = x;
    }

    if (th < 4) {
        th = MJ_TILE_H;
        ty = y;
    }

    max_picture = BMPWIDTH_mahjongg_tiles / MJ_TILE_W;

    if (max_picture <= 0) {
        max_picture = 1;
    }

    normal_picture_count = max_picture;

    if (max_picture >= 84) {
        normal_picture_count = max_picture / 2;
    }

    if (picture < 0) {
        picture = 0;
    }

    picture = picture % normal_picture_count;
    sprite_picture = picture;

    if (!open && !selected && !cursor && !hint &&
        max_picture >= normal_picture_count * 2) {
        sprite_picture = picture + normal_picture_count;
    }

    shadow = 2 + level;

    if (shadow > 5) {
        shadow = 5;
    }

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(28, 70, 26));
#endif
    rb->lcd_fillrect(tx + shadow, ty + shadow, tw, th);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(105, 94, 70));
#endif
    rb->lcd_fillrect(tx + tw - 1, ty + 2, 1, th + shadow - 2);
    rb->lcd_fillrect(tx + 2, ty + th - 1, tw + shadow - 2, 1);

    rb->lcd_bitmap_part(mahjongg_tiles,
                        sprite_picture * MJ_TILE_W + gap,
                        gap,
                        STRIDE(SCREEN_MAIN,
                               BMPWIDTH_mahjongg_tiles,
                               BMPHEIGHT_mahjongg_tiles),
                        tx,
                        ty,
                        tw,
                        th);

    /*
     * Extra right/bottom side faces.
     */
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(105, 94, 70));
#endif
    rb->lcd_fillrect(tx + tw, ty + 3, 1, th + shadow - 3);
    rb->lcd_fillrect(tx + 3, ty + th, tw + shadow - 3, 1);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(45, 38, 28));
#endif
    rb->lcd_vline(tx + tw, ty + 4, ty + th + shadow - 2);
    rb->lcd_hline(tx + 4, tx + tw + shadow - 2, ty + th);

    /*
     * Rounded visible tile outline using the inset tile bounds.
     */
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(MJ_TABLE_BG);
#endif
    rb->lcd_fillrect(tx, ty, 2, 2);
    rb->lcd_fillrect(tx + tw - 2, ty, 2, 2);
    rb->lcd_fillrect(tx, ty + th - 2, 2, 2);
    rb->lcd_fillrect(tx + tw - 2, ty + th - 2, 2, 2);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_hline(tx + 2, tx + tw - 3, ty);
    rb->lcd_hline(tx + 2, tx + tw - 3, ty + th - 1);
    rb->lcd_vline(tx, ty + 2, ty + th - 3);
    rb->lcd_vline(tx + tw - 1, ty + 2, ty + th - 3);

    rb->lcd_fillrect(tx + 1, ty + 1, 1, 1);
    rb->lcd_fillrect(tx + tw - 2, ty + 1, 1, 1);
    rb->lcd_fillrect(tx + 1, ty + th - 2, 1, 1);
    rb->lcd_fillrect(tx + tw - 2, ty + th - 2, 1, 1);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    rb->lcd_hline(tx + 2, tx + tw - 4, ty + 1);
    rb->lcd_vline(tx + 1, ty + 2, ty + th - 4);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(55, 46, 34));
#endif
    rb->lcd_hline(tx + 3, tx + tw - 3, ty + th - 2);
    rb->lcd_vline(tx + tw - 2, ty + 3, ty + th - 3);

    /*
     * Cast shadow outside the visible tile edge.
     */
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(28, 24, 18));
#endif
    rb->lcd_vline(tx - 1, ty + 5, ty + th - 4);
    rb->lcd_hline(tx + 5, tx + tw - 5, ty + th);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(80, 66, 45));
#endif
    rb->lcd_vline(tx - 2, ty + 7, ty + th - 6);
    rb->lcd_hline(tx + 7, tx + tw - 7, ty + th + 1);

    if (open && !selected && !cursor && !hint) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(255, 214, 90));
#endif
        rb->lcd_drawrect(tx + 2, ty + 2, tw - 4, th - 4);

#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(120, 90, 35));
#endif
        rb->lcd_hline(tx + 4, tx + tw - 5, ty + th - 4);
    }

    if (selected) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(23, 119, 218));
#endif
        rb->lcd_drawrect(tx + 1, ty + 1, tw - 2, th - 2);
        rb->lcd_drawrect(tx + 2, ty + 2, tw - 4, th - 4);
    }

    if (hint) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(255, 80, 180));
#endif
        rb->lcd_drawrect(tx + 2, ty + 2, tw - 4, th - 4);
        rb->lcd_drawrect(tx + 3, ty + 3, tw - 6, th - 6);
    }

    if (cursor) {
        rb->lcd_set_drawmode(DRMODE_COMPLEMENT);
        rb->lcd_fillrect(tx + 3, ty + 3, tw - 6, th - 6);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(oldfg);
#endif
}

static void draw_status_bar(void)
{
#if LCD_DEPTH > 1
    int oldfg = rb->lcd_get_foreground();
#endif
    char time_text[8];
    char best_text[8];
    int w;
    int h;

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_WHITE);
#endif

    rb->lcd_putsxyf(2, 2, "Left:%d Moves:%d",
                    mj_game_remaining(),
                    mj_game_moves());

    mj_format_time(mj_best_elapsed_ticks, best_text, sizeof(best_text));

    rb->lcd_putsxyf(2, 12, "Open:%d Best:%s",
                    mj_game_possible_moves(),
                    best_text);

    if (mj_stopwatch_enabled) {
        mj_format_time(mj_stopwatch_elapsed_ticks(), time_text, sizeof(time_text));

        rb->lcd_setfont(FONT_UI);
        rb->lcd_getstringsize(time_text, &w, &h);
        rb->lcd_putsxy(LCD_WIDTH - w - 4, 2, time_text);
        rb->lcd_setfont(MJ_FONT);
    }

    if (mj_game_selected_tile() >= 0) {
        rb->lcd_putsxy(2, 22, "Selected");
    } else {
        rb->lcd_putsxy(2, 22, "Select pair");
    }

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(oldfg);
#endif
}

static void update_screen(void)
{
    int i;
    int n = 0;
    int pass;
    int idx[MJ_MAX_TILES];
    int sx[MJ_MAX_TILES];
    int sy[MJ_MAX_TILES];
    int key[MJ_MAX_TILES];

#if LCD_DEPTH > 1
    rb->lcd_set_background(MJ_TABLE_BG);
    rb->lcd_set_foreground(LCD_BLACK);
#endif

    rb->lcd_clear_display();

    draw_table_background();

    draw_status_bar();

    /*
     * Screen-space drawing order.
     *
     * Earlier versions drew by logical lev/row/col. That works mostly,
     * but dense overlaps can still look ambiguous. This sort draws:
     *   - lower levels first
     *   - then visually higher rows first
     *   - then left-to-right
     *
     * Later-drawn tiles are visually "in front".
     */
    for (i = 0; i < mj_game_tile_count(); i++) {
        const struct mj_tile *t = mj_game_tile(i);
        int x;
        int y;

        if (t == NULL) {
            continue;
        }

        if (!t->real || t->removed) {
            continue;
        }

        tile_position(t, &x, &y);

        if (t->lev <= 1 &&
            x >= 95 && x <= 245 &&
            y >= 118 && y <= 178) {
            y += MJ_VISUAL_ROW_NUDGE_PX;
        }

        if (t->lev <= 1 &&
            x >= 190 && x <= 285 &&
            y >= 70 && y <= 205) {
            x += MJ_VISUAL_RIGHT_NUDGE_X;
            y += MJ_VISUAL_RIGHT_NUDGE_Y;
        }


        if (t->lev <= 1 &&
            x >= 195 && x <= 265 &&
            y >= 120 && y <= 190) {
            x += MJ_VISUAL_RIGHT_PULL_X;
            y += MJ_VISUAL_RIGHT_PULL_Y;
        }

        /*
         * Visual-only pullout for open/selectable tiles.
         * The tile remains at the same logical row/col/level, but the visual
         * sprite is pulled slightly outward so reachable tiles look reachable.
         */
        if (mj_game_tile_open(i)) {
            int center_x = LCD_WIDTH / 2;
            int tile_center_x = x + MJ_TILE_W / 2;

            if (tile_center_x < center_x - 4) {
                x -= MJ_VISUAL_OPEN_PULL_X;
            } else if (tile_center_x > center_x + 4) {
                x += MJ_VISUAL_OPEN_PULL_X;
            }

            y += MJ_VISUAL_OPEN_PULL_Y;
        }

        if (t->lev == 1 && t->col == 16 &&
            (t->row == 6 || t->row == 8)) {
            x += MJ_VISUAL_EXACT_RIGHT_PULL_X;
            y += MJ_VISUAL_EXACT_RIGHT_PULL_Y;
        }

        if (x + MJ_TILE_W < 0 || x >= LCD_WIDTH ||
            y + MJ_TILE_H < 0 || y >= LCD_HEIGHT) {
            continue;
        }

        idx[n] = i;
        sx[n] = x;
        sy[n] = y;

        /*
         * Big level weight guarantees upper levels are drawn over lower ones.
         * y decides front/back within a level. x breaks ties.
         */
        key[n] = t->lev * 1000000 + (y + MJ_TILE_H) * 1000 + x;

        n++;
    }

    for (pass = 1; pass < n; pass++) {
        int j = pass;
        int ti = idx[j];
        int tx = sx[j];
        int ty = sy[j];
        int tk = key[j];

        while (j > 0 && key[j - 1] > tk) {
            idx[j] = idx[j - 1];
            sx[j] = sx[j - 1];
            sy[j] = sy[j - 1];
            key[j] = key[j - 1];
            j--;
        }

        idx[j] = ti;
        sx[j] = tx;
        sy[j] = ty;
        key[j] = tk;
    }

    for (i = 0; i < n; i++) {
        const struct mj_tile *t = mj_game_tile(idx[i]);

        draw_tile_box(sx[i], sy[i], t->picture, t->match, t->lev,
                      mj_game_tile_open(idx[i]),
                      idx[i] == mj_game_selected_tile(),
                      idx[i] == mj_game_cursor_tile(),
                      idx[i] == hint_tile_a || idx[i] == hint_tile_b);
    }

    rb->lcd_update();
}

/* ------------------------------------------------------------------------ */
/* Save/load placeholders                                                    */
/* ------------------------------------------------------------------------ */

static int save_game(void)
{
    int fd;
    bool ok;

    rb->mkdir(PLUGIN_GAMES_DATA_DIR);

    fd = rb->open(MAHJONGG_SAVE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0) {
        rb->splash(HZ, "Save failed");
        return -1;
    }

    ok = mj_game_save(fd);

    if (ok) {
        ok = mj_stopwatch_save(fd);
    }

    rb->close(fd);

    if (!ok) {
        rb->splash(HZ, "Save failed");
        return -1;
    }

    rb->splash(HZ / 2, "Game saved");
    return 0;
}


static int load_game(void)
{
    int fd;
    bool ok;

    fd = rb->open(MAHJONGG_SAVE_FILE, O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    ok = mj_game_load(fd);

    if (ok) {
        mj_stopwatch_load(fd);
    }

    rb->close(fd);

    if (!ok) {
        return -1;
    }

    rb->splash(HZ / 2, "Game loaded");
    return 0;
}


/* ------------------------------------------------------------------------ */
/* Menu                                                                      */
/* ------------------------------------------------------------------------ */

enum {
    MJ_MENU_RESUME = 0,
    MJ_MENU_NEW_GAME,
    MJ_MENU_TOGGLE_TIMER,
    MJ_MENU_HELP,
    MJ_MENU_SAVE_QUIT,
    MJ_MENU_QUIT
};

static void show_help(void)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(2, 2, MAHJONGG_TITLE);
    rb->lcd_putsxy(2, 16, MJ_KEYS_PREV_NEXT ": Prev/Next tile");
    rb->lcd_putsxy(2, 26, MJ_KEYS_SELECT ": Select");
    rb->lcd_putsxy(2, 36, MJ_KEYS_HINT ": Hint");
    rb->lcd_putsxy(2, 46, MJ_KEYS_UNDO ": Undo");
    rb->lcd_putsxy(2, 56, MJ_KEYS_MENU ": Menu");
    rb->lcd_update();

    rb->button_clear_queue();
    rb->button_get(true);
}

static int mahjongg_menu(void)
{
    int selected = 0;

    MENUITEM_STRINGLIST(menu, "Mahjongg", NULL,
                        "Resume",
                        "New Game",
                        "Toggle Stopwatch",
                        "Help",
                        /*"Audio Playback",*/
                        "Save and Quit",
                        "Quit without Saving");

    while (true) {
        switch (rb->do_menu(&menu, &selected, NULL, false)) {
            case MJ_MENU_RESUME:
                return MJ_MENU_RESUME;

            case MJ_MENU_NEW_GAME:
                start_new_game();
                return MJ_MENU_RESUME;

            case MJ_MENU_TOGGLE_TIMER:
                mj_stopwatch_enabled = !mj_stopwatch_enabled;
                mj_stopwatch_reset();

                if (mj_stopwatch_enabled) {
                    rb->splash(HZ, "Stopwatch on");
                } else {
                    rb->splash(HZ, "Stopwatch off");
                }

                return MJ_MENU_RESUME;

            case MJ_MENU_HELP:
                show_help();
                break;

            /*case MJ_MENU_PLAYBACK:
                playback_control(NULL);
                break;
*/
            case MJ_MENU_SAVE_QUIT:
                save_game();
                return MJ_MENU_SAVE_QUIT;

            case MJ_MENU_QUIT:
            default:
                return MJ_MENU_QUIT;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Main loop                                                                 */
/* ------------------------------------------------------------------------ */

static enum plugin_status mahjongg_loop(void)
{
    int button;

#ifdef MJ_NEED_LASTBUTTON
    int lastbutton = BUTTON_NONE;
#endif

    while (true) {
        button = rb->button_get_w_tmo(mj_stopwatch_enabled ? HZ / 4 : HZ);

        if (button == BUTTON_NONE) {
            update_screen();
            rb->yield();
            continue;
        }

        switch (button) {
            case MJ_PREV:
            case MJ_PREV | BUTTON_REPEAT:
                clear_hint();
                mj_game_cursor_prev();
                update_screen();
                break;

            case MJ_NEXT:
            case MJ_NEXT | BUTTON_REPEAT:
                clear_hint();
                mj_game_cursor_next();
                update_screen();
                break;

#ifdef MJ_LEFT
            case MJ_LEFT:
#ifdef MJ_LEFT_PRE
                if (lastbutton != MJ_LEFT_PRE)
                    break;
#endif
                /*
                 * Optional navigation.
                 * For now, same as previous tile.
                 */
                mj_game_cursor_prev();
                update_screen();
                break;
#endif

#ifdef MJ_RIGHT
            case MJ_RIGHT:
#ifdef MJ_RIGHT_PRE
                if (lastbutton != MJ_RIGHT_PRE)
                    break;
#endif
                /*
                 * Optional navigation.
                 * For now, same as next tile.
                 */
                mj_game_cursor_next();
                update_screen();
                break;
#endif

            case MJ_SELECT:
#ifdef MJ_SELECT_PRE
                if (lastbutton != MJ_SELECT_PRE)
                    break;
#endif
                {
                    enum mj_select_result result;

                    clear_hint();
                    result = mj_game_select_current();

                    if (result == MJ_SELECT_REMOVED_PAIR &&
                        mj_game_remaining() == 0) {
                        long elapsed = mj_stopwatch_elapsed_ticks();

                        mj_score_save_best(elapsed);
                        update_screen();
                        rb->splash(HZ * 2, "Solved!");
                        return PLUGIN_OK;
                    }

                    update_screen();
                }
                break;

            case MJ_HINT:
#ifdef MJ_HINT_PRE
                if (lastbutton != MJ_HINT_PRE)
                    break;
#endif
                show_hint();
                update_screen();
                break;

            case MJ_UNDO:
            case MJ_UNDO | BUTTON_REPEAT:
                undo_move();
                update_screen();
                break;

            case MJ_MENU:
                switch (mahjongg_menu()) {
                    case MJ_MENU_SAVE_QUIT:
                    case MJ_MENU_QUIT:
                        return PLUGIN_OK;

                    case MJ_MENU_RESUME:
                    default:
                        update_screen();
                        break;
                }
                break;

            case SYS_POWEROFF:
            case SYS_REBOOT:
                save_game();
                return PLUGIN_OK;

            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }

#ifdef MJ_NEED_LASTBUTTON
        if (button != BUTTON_NONE)
            lastbutton = button;
#endif

        rb->yield();
    }

    return PLUGIN_OK;
}

/* ------------------------------------------------------------------------ */
/* Plugin entry point                                                        */
/* ------------------------------------------------------------------------ */

enum plugin_status plugin_start(const void *parameter)
{
    int w, h;
    int loaded;

    (void)parameter;

    rb->lcd_setfont(MJ_FONT);

    rb->lcd_clear_display();
    rb->lcd_getstringsize(MAHJONGG_TITLE, &w, &h);
    rb->lcd_putsxy(LCD_WIDTH / 2 - w / 2,
                   LCD_HEIGHT / 2 - h / 2,
                   MAHJONGG_TITLE);
    rb->lcd_update();
    rb->sleep(HZ);

    loaded = load_game();

    if (loaded != 0) {
        start_new_game();
    }

    update_screen();

    return mahjongg_loop();
}
