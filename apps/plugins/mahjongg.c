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

/* ------------------------------------------------------------------------ */
/* Display constants                                                         */
/* ------------------------------------------------------------------------ */

/*
 * First test sizes.
 * Later these should be derived from actual bitmap dimensions.
 */
#define MJ_TILE_W       18
#define MJ_TILE_H       22
#define MJ_TILE_XSTEP    9
#define MJ_TILE_YSTEP   11
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

static void start_new_game(void)
{
    clear_hint();
    mj_game_init_default((unsigned int)*rb->current_tick);
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
    const int layout_y = 24;

    *x = layout_x
       + MJ_TILE_XSTEP * t->col
       + MJ_LEVEL_DX * t->lev;

    *y = layout_y
       + MJ_TILE_YSTEP * t->row
       + MJ_LEVEL_DY * t->lev;
}

static void draw_tile_box(int x, int y, int picture, int match,
                          bool selected, bool cursor, bool hint)
{
#if LCD_DEPTH > 1
    int oldfg = rb->lcd_get_foreground();
#endif
    int max_picture;

    max_picture = BMPWIDTH_mahjongg_tiles / MJ_TILE_W;

    if (max_picture <= 0) {
        max_picture = 1;
    }

    if (picture < 0) {
        picture = 0;
    }

    picture = picture % max_picture;

    rb->lcd_bitmap_part(mahjongg_tiles,
                        picture * MJ_TILE_W,
                        0,
                        STRIDE(SCREEN_MAIN,
                               BMPWIDTH_mahjongg_tiles,
                               BMPHEIGHT_mahjongg_tiles),
                        x,
                        y,
                        MJ_TILE_W,
                        MJ_TILE_H);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_BLACK);
#endif

    (void)match;

    if (selected) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(23, 119, 218));
#endif
        rb->lcd_drawrect(x + 1, y + 1, MJ_TILE_W - 2, MJ_TILE_H - 2);
    }

    if (hint) {
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_RGBPACK(255, 80, 180));
#endif
        rb->lcd_drawrect(x + 2, y + 2, MJ_TILE_W - 4, MJ_TILE_H - 4);
        rb->lcd_drawrect(x + 3, y + 3, MJ_TILE_W - 6, MJ_TILE_H - 6);
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
    int cursor;
    const struct mj_tile *tile;

    rb->lcd_putsxyf(2, 2, "Left:%d Moves:%d",
                    mj_game_remaining(),
                    mj_game_moves());

    rb->lcd_putsxyf(2, 12, "Open:%d Seed:%lu",
                    mj_game_possible_moves(),
                    (unsigned long)mj_game_seed());

    cursor = mj_game_cursor_tile();
    tile = mj_game_tile(cursor);

    if (tile != NULL) {
        rb->lcd_putsxyf(2, 22, "C:%d P:%d M:%d",
                        cursor,
                        tile->picture,
                        tile->match);
    } else if (mj_game_selected_tile() >= 0) {
        rb->lcd_putsxy(2, 22, "Selected");
    } else {
        rb->lcd_putsxy(2, 22, "Select pair");
    }
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
for (i = 0; i < mj_game_tile_count(); i++) {
    const struct mj_tile *t = mj_game_tile(i);

    if (t == NULL) {
        continue;
    }

    if (!t->real || t->removed) {
        continue;
    }

    tile_position(t, &x, &y);

    if (x + MJ_TILE_W < 0 || x >= LCD_WIDTH ||
        y + MJ_TILE_H < 0 || y >= LCD_HEIGHT) {
        continue;
    }

    draw_tile_box(x, y, t->picture, t->match,
                  i == mj_game_selected_tile(),
                  i == mj_game_cursor_tile(),
                  i == hint_tile_a || i == hint_tile_b);
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
                clear_hint();
                mj_game_select_current();
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
        start_new_game();
    }

    update_screen();

    return mahjongg_loop();
}
