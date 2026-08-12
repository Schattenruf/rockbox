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
/*#include "lib/playback_control.h"

/* ------------------------------------------------------------------------ */
/* Basic metadata                                                            */
/* ------------------------------------------------------------------------ */

#define MAHJONGG_TITLE "Mahjongg"

#define MAHJONGG_SAVE_FILE PLUGIN_GAMES_DATA_DIR "/mahjongg.save"

/* ------------------------------------------------------------------------ */
/* Display constants                                                         */
/* ------------------------------------------------------------------------ */

/*
 * First test sizes.
 * Later these should be derived from actual bitmap dimensions.
 */
#define MJ_TILE_W       14
#define MJ_TILE_H       18
#define MJ_TILE_XSTEP    7
#define MJ_TILE_YSTEP    9
#define MJ_LEVEL_DX      2
#define MJ_LEVEL_DY     -2

#define MJ_MAX_TILES   144
#define MJ_ROWS         24
#define MJ_COLS         38
#define MJ_LEVS         12

#define MJ_FONT FONT_SYSFIXED

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

#define MJ_MENU             (BUTTON_SELECT | BUTTON_REPEAT)

#define MJ_HINT_PRE         BUTTON_MENU
#define MJ_HINT             (BUTTON_MENU | BUTTON_REL)

#define MJ_UNDO             BUTTON_PLAY

#define MJ_LEFT_PRE         BUTTON_LEFT
#define MJ_LEFT             (BUTTON_LEFT | BUTTON_REL)

#define MJ_RIGHT_PRE        BUTTON_RIGHT
#define MJ_RIGHT            (BUTTON_RIGHT | BUTTON_REL)

#define MJ_KEYS_PREV_NEXT   "SCROLL"
#define MJ_KEYS_SELECT      "SELECT"
#define MJ_KEYS_MENU        "SELECT.."
#define MJ_KEYS_HINT        "MENU"
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

/* ------------------------------------------------------------------------ */
/* Temporary tile/game model                                                 */
/* ------------------------------------------------------------------------ */

/*
 * This is deliberately simple.
 * Later this should be replaced by a cleaned C port of xmahjongg's:
 *
 *   class Tile  -> struct mj_tile
 *   class Game  -> struct mj_game
 */
struct mj_tile {
    bool real;
    bool removed;

    short number;
    short match;

    short row;
    short col;
    short lev;

    short coverage;
    short blocked;
};

struct mj_game {
    struct mj_tile tiles[MJ_MAX_TILES];

    int tile_count;
    int remaining;

    int cursor_tile;
    int selected_tile;

    int moves;
};

static struct mj_game game;

/* ------------------------------------------------------------------------ */
/* Utility                                                                   */
/* ------------------------------------------------------------------------ */

static bool tile_open(const struct mj_tile *t)
{
    return t->real && !t->removed && !t->blocked && !t->coverage;
}

static bool tiles_match(int a, int b)
{
    if (a < 0 || b < 0 || a >= game.tile_count || b >= game.tile_count)
        return false;

    if (a == b)
        return false;

    if (!tile_open(&game.tiles[a]) || !tile_open(&game.tiles[b]))
        return false;

    return game.tiles[a].match == game.tiles[b].match;
}

/* ------------------------------------------------------------------------ */
/* Temporary layout                                                          */
/* ------------------------------------------------------------------------ */

/*
 * This mirrors the default xmahjongg layout idea in a simplified way.
 * Later we should port layout_default() from xmahjongg game.cc more exactly.
 */
static void add_tile(short row, short col, short lev, short match)
{
    struct mj_tile *t;

    if (game.tile_count >= MJ_MAX_TILES)
        return;

    t = &game.tiles[game.tile_count];

    t->real = true;
    t->removed = false;

    t->number = game.tile_count;
    t->match = match;

    t->row = row;
    t->col = col;
    t->lev = lev;

    t->coverage = 0;
    t->blocked = 0;

    game.tile_count++;
    game.remaining++;
}

/*
 * Placeholder layout.
 * Not a full 144-tile turtle yet, but enough to test rendering and cursor.
 */
static void init_dummy_layout(void)
{
    int i;
    int r, c;

    game.tile_count = 0;
    game.remaining = 0;
    game.cursor_tile = -1;
    game.selected_tile = -1;
    game.moves = 0;

    /*
     * 8 x 6 base layer = 48 tiles.
     * Enough for early display testing.
     */
    i = 0;
    for (r = 0; r < 6; r++) {
        for (c = 0; c < 8; c++) {
            add_tile(4 + r * 2, 8 + c * 2, 0, i / 2);
            i++;
        }
    }

    /*
     * Small second layer.
     */
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 6; c++) {
            add_tile(6 + r * 2, 10 + c * 2, 1, i / 2);
            i++;
        }
    }

    /*
     * For the skeleton, fake every tile as open.
     * Later xmahjongg's init_blockage() replaces this.
     */
    for (i = 0; i < game.tile_count; i++) {
        game.tiles[i].coverage = 0;
        game.tiles[i].blocked = 0;
    }

    if (game.tile_count > 0)
        game.cursor_tile = 0;
}

/* ------------------------------------------------------------------------ */
/* Cursor logic                                                              */
/* ------------------------------------------------------------------------ */

static int find_next_open_tile(int start, int direction)
{
    int i;
    int idx;

    if (game.tile_count <= 0)
        return -1;

    idx = start;

    for (i = 0; i < game.tile_count; i++) {
        idx += direction;

        if (idx < 0)
            idx = game.tile_count - 1;
        else if (idx >= game.tile_count)
            idx = 0;

        if (tile_open(&game.tiles[idx]))
            return idx;
    }

    return -1;
}

static void cursor_next(void)
{
    int next = find_next_open_tile(game.cursor_tile, 1);

    if (next >= 0)
        game.cursor_tile = next;
}

static void cursor_prev(void)
{
    int prev = find_next_open_tile(game.cursor_tile, -1);

    if (prev >= 0)
        game.cursor_tile = prev;
}

/* ------------------------------------------------------------------------ */
/* Game actions                                                              */
/* ------------------------------------------------------------------------ */

static void deselect_tile(void)
{
    game.selected_tile = -1;
}

static void remove_pair(int a, int b)
{
    game.tiles[a].removed = true;
    game.tiles[b].removed = true;

    game.remaining -= 2;
    game.moves++;

    deselect_tile();

    /*
     * Later:
     *   - call mj_game_remove_pair()
     *   - update coverage/blockage
     *   - push undo record
     */
    game.cursor_tile = find_next_open_tile(game.cursor_tile, 1);
}

static void select_current_tile(void)
{
    int cur = game.cursor_tile;

    if (cur < 0 || cur >= game.tile_count)
        return;

    if (!tile_open(&game.tiles[cur]))
        return;

    if (game.selected_tile < 0) {
        game.selected_tile = cur;
        return;
    }

    if (game.selected_tile == cur) {
        deselect_tile();
        return;
    }

    if (tiles_match(game.selected_tile, cur)) {
        remove_pair(game.selected_tile, cur);
    } else {
        /*
         * Replace selection if it does not match.
         * This feels good on Click Wheel.
         */
        game.selected_tile = cur;
    }
}

static void undo_move(void)
{
    /*
     * Placeholder.
     * Later this should call the xmahjongg-derived undo implementation.
     */
    rb->splash(HZ / 2, "Undo not yet implemented");
}

static void show_hint(void)
{
    /*
     * Placeholder.
     * Later:
     *   - scan for two open tiles with same match id
     *   - select/highlight them temporarily
     */
    rb->splash(HZ / 2, "Hint not yet implemented");
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
    const int layout_y = 24;

    *x = layout_x
       + MJ_TILE_XSTEP * t->col
       + MJ_LEVEL_DX * t->lev;

    *y = layout_y
       + MJ_TILE_YSTEP * t->row
       + MJ_LEVEL_DY * t->lev;
}

static void draw_tile_box(int x, int y, int match, bool selected, bool cursor)
{
#if LCD_DEPTH > 1
    int oldfg = rb->lcd_get_foreground();
#endif

    /*
     * Dummy tile body.
     * Later replace with:
     *
     * rb->lcd_bitmap_part(mahjong_tiles, ...);
     */
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    rb->lcd_fillrect(x, y, MJ_TILE_W, MJ_TILE_H);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_drawrect(x, y, MJ_TILE_W, MJ_TILE_H);

    /*
     * Very small match number for debugging.
     */
    rb->lcd_putsxyf(x + 2, y + 5, "%02d", match);

    if (selected) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(23, 119, 218));
#endif
        rb->lcd_drawrect(x + 1, y + 1, MJ_TILE_W - 2, MJ_TILE_H - 2);
    }

    if (cursor) {
        rb->lcd_set_drawmode(DRMODE_COMPLEMENT);
        rb->lcd_fillrect(x + 2, y + 2, MJ_TILE_W - 4, MJ_TILE_H - 4);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(oldfg);
#endif
}

static void draw_status_bar(void)
{
    rb->lcd_putsxyf(2, 2, "Left:%d Moves:%d", game.remaining, game.moves);

    if (game.selected_tile >= 0)
        rb->lcd_putsxy(2, 12, "Selected");
    else
        rb->lcd_putsxy(2, 12, "Select pair");
}

static void update_screen(void)
{
    int i;
    int x, y;

#if LCD_DEPTH > 1
    rb->lcd_set_background(LCD_RGBPACK(0, 100, 0));
    rb->lcd_set_foreground(LCD_BLACK);
#endif

    rb->lcd_clear_display();

    draw_status_bar();

    /*
     * Early draw order:
     *   lower levels first, higher later.
     *
     * Later we should port xmahjongg's display_order_dfs()
     * from board.cc for correct overlap handling.
     */
    for (i = 0; i < game.tile_count; i++) {
        struct mj_tile *t = &game.tiles[i];

        if (!t->real || t->removed)
            continue;

        tile_position(t, &x, &y);

        /*
         * Skip tiles outside display.
         */
        if (x + MJ_TILE_W < 0 || x >= LCD_WIDTH ||
            y + MJ_TILE_H < 0 || y >= LCD_HEIGHT)
            continue;

        draw_tile_box(x, y, t->match,
                      i == game.selected_tile,
                      i == game.cursor_tile);
    }

    rb->lcd_update();
}

/* ------------------------------------------------------------------------ */
/* Save/load placeholders                                                    */
/* ------------------------------------------------------------------------ */

static int save_game(void)
{
    /*
     * Placeholder.
     * Later save:
     *   - board number / seed
     *   - removed flags
     *   - undo stack
     *   - selected/cursor tile
     */
    rb->splash(HZ / 2, "Save not yet implemented");
    return 0;
}

static int load_game(void)
{
    /*
     * Placeholder.
     * Return non-zero for "no save loaded".
     */
    return -1;
}

/* ------------------------------------------------------------------------ */
/* Menu                                                                      */
/* ------------------------------------------------------------------------ */

enum {
    MJ_MENU_RESUME = 0,
    MJ_MENU_NEW_GAME,
    MJ_MENU_HELP,
    MJ_MENU_PLAYBACK,
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
                        "Help",
                        /*"Audio Playback",*/
                        "Save and Quit",
                        "Quit without Saving");

    while (true) {
        switch (rb->do_menu(&menu, &selected, NULL, false)) {
            case MJ_MENU_RESUME:
                return MJ_MENU_RESUME;

            case MJ_MENU_NEW_GAME:
                init_dummy_layout();
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
        button = rb->button_get(true);

        switch (button) {
            case MJ_PREV:
            case MJ_PREV | BUTTON_REPEAT:
                cursor_prev();
                update_screen();
                break;

            case MJ_NEXT:
            case MJ_NEXT | BUTTON_REPEAT:
                cursor_next();
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
                cursor_prev();
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
                cursor_next();
                update_screen();
                break;
#endif

            case MJ_SELECT:
#ifdef MJ_SELECT_PRE
                if (lastbutton != MJ_SELECT_PRE)
                    break;
#endif
                select_current_tile();
                update_screen();
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
        init_dummy_layout();
    }

    update_screen();

    return mahjongg_loop();
}
