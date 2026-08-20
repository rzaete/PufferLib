#ifndef RENDER_H
#define RENDER_H

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "raylib.h"
#include "stadium.h"
#include "table.h"

#define RCSS_FIELD           (Color){ 31, 160,  31, 255 }
#define RCSS_LINE            (Color){255, 255, 255, 255 }
#define RCSS_SCORE_BG        (Color){  0,   0,   0, 255 }
#define RCSS_SCORE_FG        (Color){255, 255, 255, 255 }
#define RCSS_BALL            (Color){255, 255, 255, 255 }
#define RCSS_PLAYER_RING     (Color){  0,   0,   0, 255 }
#define RCSS_LEFT            (Color){255, 215,   0, 255 }
#define RCSS_LEFT_GOALIE     (Color){ 39, 231,  31, 255 }
#define RCSS_RIGHT           (Color){  0, 191, 255, 255 }
#define RCSS_RIGHT_GOALIE    (Color){255, 153, 255, 255 }
#define RCSS_NUMBER          (Color){255, 255, 255, 255 }
#define RCSS_BALL_COLLIDE    (Color){255,   0,   0, 255 }
#define RCSS_PLAYER_COLLIDE  (Color){105, 155, 235, 255 }
#define RCSS_KICK            (Color){255, 255, 255, 255 }
#define RCSS_KICK_FAULT      (Color){255,   0,   0, 255 }
#define RCSS_CATCH           (Color){ 10,  80,  10, 255 }
#define RCSS_CATCH_FAULT     (Color){ 10,  80, 150, 255 }
#define RCSS_TACKLE          (Color){255, 136, 127, 255 }
#define RCSS_TACKLE_FAULT    (Color){ 79, 159, 159, 255 }
#define RCSS_FOUL_CHARGED    (Color){  0, 127,   0, 255 }
#define RCSS_EFFORT_DECAYED  (Color){255,   0,   0, 255 }
#define RCSS_RECOVERY_DECAYED (Color){255, 231,  31, 255 }

#define RCSS_CENTER_CIRCLE_R   9.15f
#define RCSS_PENALTY_CIRCLE_R  9.15f
#define RCSS_PENALTY_SPOT_DIST 11.0f
#define RCSS_GOAL_DEPTH        2.44f
#define RCSS_CORNER_ARC_R      1.0f
#define RCSS_BALL_DRAW_SIZE    0.35f
#define RCSS_HALF_L            (PITCH_LENGTH * 0.5f)
#define RCSS_HALF_W            (PITCH_WIDTH * 0.5f)

#define RCSS_SCOREBOARD_FONT   20
#define RCSS_PLAYER_FONT       14
#define RCSS_FONT_PATH         "resources/shared/JetBrainsMono-Regular.ttf"
#define RCSS_FONT_BOLD         "resources/shared/JetBrainsMono-Bold.ttf"
#define RCSS_FONT_BASE         22

#define RCSS_DIAG_FONT         18
#define RCSS_DIAG_PAD          6
#define RCSS_DIAG_BG           (Color){  0,   0,   0, 160 }
#define RCSS_DIAG_HOVER        (Color){255, 255, 255,  32 }
#define RCSS_DIAG_LINE_LEN     256
#define RCSS_PLAYER_TABLE_ROWS 40

typedef struct {
    float scale;
    float cx;
    float cy;
} RcssView;

static inline int rcss_clampi(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float rcss_clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float rcss_x(const RcssView *v, float x) { return v->cx + x * v->scale; }
static inline float rcss_y(const RcssView *v, float y) { return v->cy + y * v->scale; }
static inline float rcss_px(const RcssView *v, float meters) { return meters * v->scale; }
static inline Vector2 rcss_xy(const RcssView *v, float x, float y) {
    return (Vector2){ rcss_x(v, x), rcss_y(v, y) };
}

static void rcss_clip(Rectangle pane) {
    int w = (int)pane.width;
    int h = (int)pane.height;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    BeginScissorMode((int)pane.x, (int)pane.y, w, h);
}

static inline Color rcss_dim(Color c, int percent) {
    if (percent <= 100) return c;
    c.r = (unsigned char)((int)c.r * 100 / percent);
    c.g = (unsigned char)((int)c.g * 100 / percent);
    c.b = (unsigned char)((int)c.b * 100 / percent);
    return c;
}

static inline Color rcss_side_color(Side side) {
    if (side == LEFT) return RCSS_LEFT;
    if (side == RIGHT) return RCSS_RIGHT;
    return RCSS_SCORE_FG;
}

static Font rcss_load_font(const char *path) {
    Font font = LoadFontEx(path, RCSS_FONT_BASE, NULL, 0);
    if (font.texture.id != 0)
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

static Font rcss_font(void) {
    static Font font;
    if (font.texture.id == 0)
        font = rcss_load_font(RCSS_FONT_PATH);
    return font;
}

static Font rcss_bold(void) {
    static Font font;
    if (font.texture.id == 0)
        font = rcss_load_font(RCSS_FONT_BOLD);
    return font;
}

static void rcss_text(Font font, const char *s, int x, int y, int size, Color color) {
    if (font.texture.id == 0) {
        DrawText(s, x, y, size, color);
        return;
    }
    DrawTextEx(font, s, (Vector2){(float)x, (float)y}, (float)size, 0.0f, color);
}

static int rcss_width(Font font, const char *s, int size) {
    if (font.texture.id == 0)
        return MeasureText(s, size);
    return (int)MeasureTextEx(font, s, (float)size, 0.0f).x;
}

static void rcss_draw_text(const char *s, int x, int y, int size, Color color) {
    rcss_text(rcss_font(), s, x, y, size, color);
}

static int rcss_text_width(const char *s, int size) {
    return rcss_width(rcss_font(), s, size);
}

static RcssView rcss_make_view(Rectangle pane, int scoreboard_h) {
    float pitch_l = PITCH_LENGTH + PITCH_MARGIN * 2.0f + 1.0f;
    float pitch_w = PITCH_WIDTH + PITCH_MARGIN * 2.0f;
    float field_h = pane.height - (float)scoreboard_h;
    if (field_h < 1.0f) field_h = 1.0f;
    float scale = pane.width / pitch_l;
    if (pitch_w * scale > field_h)
        scale = field_h / pitch_w;
    if (scale < 1.0f) scale = 1.0f;
    scale = roundf(scale * 100.0f) / 100.0f;
    RcssView v;
    v.scale = scale;
    v.cx = pane.x + pane.width * 0.5f;
    v.cy = pane.y + field_h * 0.5f;
    return v;
}

static void rcss_hline(const RcssView *v, float x0, float x1, float y, Color c) {
    DrawLineV(rcss_xy(v, x0, y), rcss_xy(v, x1, y), c);
}

static void rcss_vline(const RcssView *v, float x, float y0, float y1, Color c) {
    DrawLineV(rcss_xy(v, x, y0), rcss_xy(v, x, y1), c);
}

static void rcss_arc(const RcssView *v, float x, float y, float radius_m,
                     float start_deg, float end_deg, Color color) {
    float r = rcss_px(v, radius_m);
    if (r < 1.0f) return;
    Vector2 c = rcss_xy(v, x, y);
    int n = (int)(fabsf(end_deg - start_deg) / 3.0f);
    if (n < 8) n = 8;
    float step = (end_deg - start_deg) / (float)n;
    float a0 = start_deg * DEG2RAD;
    Vector2 prev = { c.x + cosf(a0) * r, c.y + sinf(a0) * r };
    for (int i = 1; i <= n; i++) {
        float a = (start_deg + step * (float)i) * DEG2RAD;
        Vector2 p = { c.x + cosf(a) * r, c.y + sinf(a) * r };
        DrawLineV(prev, p, color);
        prev = p;
    }
}

static void rcss_draw_area(const RcssView *v, float length, float width, Color line) {
    float half_w = width * 0.5f;
    float inner = RCSS_HALF_L - length;
    rcss_hline(v, -RCSS_HALF_L, -inner, -half_w, line);
    rcss_hline(v, -RCSS_HALF_L, -inner,  half_w, line);
    rcss_vline(v, -inner, -half_w, half_w, line);
    rcss_hline(v,  RCSS_HALF_L,  inner, -half_w, line);
    rcss_hline(v,  RCSS_HALF_L,  inner,  half_w, line);
    rcss_vline(v,  inner, -half_w, half_w, line);
}

static void rcss_draw_goals(const RcssView *v) {
    float top = rcss_y(v, -GOAL_WIDTH * 0.5f);
    float h = rcss_px(v, GOAL_WIDTH);
    float d = rcss_px(v, RCSS_GOAL_DEPTH);
    DrawRectangleV((Vector2){ rcss_x(v, -RCSS_HALF_L - RCSS_GOAL_DEPTH) - 1.0f, top },
                   (Vector2){ d, h }, BLACK);
    DrawRectangleV((Vector2){ rcss_x(v, RCSS_HALF_L) + 1.0f, top },
                   (Vector2){ d, h }, BLACK);

    float post_d = rcss_px(v, GOAL_POST_RADIUS * 2.0f);
    if (post_d < 1.0f) return;
    float r = post_d * 0.5f;
    float gy = GOAL_WIDTH * 0.5f;
    DrawCircleV(rcss_xy(v, -RCSS_HALF_L, -gy), r, BLACK);
    DrawCircleV(rcss_xy(v, -RCSS_HALF_L,  gy), r, BLACK);
    DrawCircleV(rcss_xy(v,  RCSS_HALF_L, -gy), r, BLACK);
    DrawCircleV(rcss_xy(v,  RCSS_HALF_L,  gy), r, BLACK);
}

static void rcss_draw_field(const RcssView *v) {
    const Color line = RCSS_LINE;
    const float l = -RCSS_HALF_L;
    const float r =  RCSS_HALF_L;
    const float t = -RCSS_HALF_W;
    const float b =  RCSS_HALF_W;

    rcss_hline(v, l, r, t, line);
    rcss_hline(v, l, r, b, line);
    rcss_vline(v, l, t, b, line);
    rcss_vline(v, r, t, b, line);
    rcss_vline(v, 0.0f, t, b, line);
    DrawCircleLinesV(rcss_xy(v, 0.0f, 0.0f), rcss_px(v, RCSS_CENTER_CIRCLE_R), line);

    rcss_arc(v, l, t, RCSS_CORNER_ARC_R,   0.0f,  90.0f, line);
    rcss_arc(v, l, b, RCSS_CORNER_ARC_R, 270.0f, 360.0f, line);
    rcss_arc(v, r, b, RCSS_CORNER_ARC_R, 180.0f, 270.0f, line);
    rcss_arc(v, r, t, RCSS_CORNER_ARC_R,  90.0f, 180.0f, line);

    rcss_draw_area(v, PENALTY_AREA_LENGTH, PENALTY_AREA_WIDTH, line);

    float spot = RCSS_HALF_L - RCSS_PENALTY_SPOT_DIST;
    DrawCircleV(rcss_xy(v, -spot, 0.0f), 1.5f, line);
    DrawCircleV(rcss_xy(v,  spot, 0.0f), 1.5f, line);

    float ratio = rcss_clampf(
        (PENALTY_AREA_LENGTH - RCSS_PENALTY_SPOT_DIST) / RCSS_PENALTY_CIRCLE_R, -1.0f, 1.0f);
    float alpha = acosf(ratio) * RAD2DEG;
    rcss_arc(v, -spot, 0.0f, RCSS_PENALTY_CIRCLE_R, -alpha, alpha, line);
    rcss_arc(v,  spot, 0.0f, RCSS_PENALTY_CIRCLE_R, 180.0f - alpha, 180.0f + alpha, line);

    rcss_draw_area(v, GOAL_AREA_LENGTH, GOAL_AREA_WIDTH, line);
    rcss_draw_goals(v);
}

static Color rcss_player_fill(const Stadium *stadium, int i) {
    const Players *p = &stadium->players;
    Color fill = BLACK;
    if (p->side[i] == LEFT)
        fill = p->goalie[i] ? RCSS_LEFT_GOALIE : RCSS_LEFT;
    else if (p->side[i] == RIGHT)
        fill = p->goalie[i] ? RCSS_RIGHT_GOALIE : RCSS_RIGHT;

    int32_t st = p->state[i];
    if (st & STATE_KICK_FAULT)     fill = RCSS_KICK_FAULT;
    if (st & STATE_CATCH)          fill = RCSS_CATCH;
    if (st & STATE_CATCH_FAULT)    fill = RCSS_CATCH_FAULT;
    if (st & STATE_TACKLE)         fill = RCSS_TACKLE;
    if (st & STATE_TACKLE_FAULT)   fill = RCSS_TACKLE_FAULT;
    if (st & STATE_FOUL_CHARGED)   fill = RCSS_FOUL_CHARGED;
    if (st & STATE_BALL_COLLIDE)   fill = RCSS_BALL_COLLIDE;
    if (st & STATE_PLAYER_COLLIDE) fill = RCSS_PLAYER_COLLIDE;
    return fill;
}

static Color rcss_player_outline(const Stadium *stadium, int i) {
    int32_t st = stadium->players.state[i];
    if (st & (STATE_TACKLE | STATE_TACKLE_FAULT)) return RCSS_TACKLE;
    if (st & STATE_KICK) return RCSS_KICK;
    return RCSS_PLAYER_RING;
}

static Color rcss_stamina_fill(Color fill, float stamina) {
    float rate = rcss_clampf(stamina / (float)STAMINA_MAX, 0.0f, 1.0f);
    float quantized = roundf(rate * 8.0f) / 8.0f;
    int darkness = (int)roundf((1.0f - quantized) * 200.0f) - 50;
    if (darkness <= 0) return fill;
    return rcss_dim(fill, 100 + darkness);
}

static void rcss_draw_player(const RcssView *v, const Stadium *stadium, int i) {
    const Players *p = &stadium->players;
    float size = p->player_type[i].player_size;
    float kickable = p->player_type[i].kickable_margin;
    if (size < 0.01f) size = PLAYER_SIZE;
    if (kickable < 0.01f) kickable = KICKABLE_MARGIN;

    float reach = size + kickable + BALL_SIZE;
    float body_r = rcss_px(v, size);
    float kick_r = rcss_px(v, reach);
    if (body_r < 1.0f) body_r = 1.0f;
    if (kick_r < 5.0f) kick_r = 5.0f;

    Vector2 c = rcss_xy(v, p->pos_x[i], p->pos_y[i]);
    Color fill = rcss_player_fill(stadium, i);
    Color outline = rcss_player_outline(stadium, i);

    DrawCircleV(c, kick_r, fill);
    DrawCircleLinesV(c, kick_r, outline);
    if (p->state[i] & STATE_KICK)
        DrawCircleLinesV(c, kick_r + 1.0f, outline);

    DrawCircleV(c, body_r, rcss_stamina_fill(fill, p->stamina[i]));
    DrawCircleLinesV(c, body_r, RCSS_PLAYER_RING);

    if (p->effort[i] < p->player_type[i].effort_max - 1.0e-3f)
        DrawCircleLinesV(c, kick_r + 2.0f, RCSS_EFFORT_DECAYED);
    else if (p->recovery[i] < 1.0f - 1.0e-3f)
        DrawCircleLinesV(c, kick_r + 2.0f, RCSS_RECOVERY_DECAYED);

    float body = p->angle_body_committed[i];
    DrawLineV(c, rcss_xy(v, p->pos_x[i] + reach * cosf(body),
                            p->pos_y[i] + reach * sinf(body)), RCSS_PLAYER_RING);

    char label[32];
    snprintf(label, sizeof(label), "%d", p->unum[i]);
    int text_r = (int)kick_r;
    if (text_r > 40) text_r = 40;
    int tx = (int)c.x + text_r;
    int ty = (int)c.y - RCSS_PLAYER_FONT / 2;
    if (p->state[i] & (STATE_RED_CARD | STATE_YELLOW_CARD)) {
        int card_w = 6;
        DrawRectangle(tx, ty, card_w, RCSS_PLAYER_FONT,
                      (p->state[i] & STATE_RED_CARD) ? RED : YELLOW);
        tx += card_w + 2;
    }
    rcss_draw_text(label, tx, ty, RCSS_PLAYER_FONT, RCSS_NUMBER);
}

static void rcss_draw_ball(const RcssView *v, float x, float y) {
    Vector2 c = rcss_xy(v, x, y);
    float ball_r = rcss_px(v, RCSS_BALL_DRAW_SIZE);
    if (ball_r < 1.0f) ball_r = 1.0f;
    float kick_r = rcss_px(v, PLAYER_SIZE + KICKABLE_MARGIN + BALL_SIZE);
    if (kick_r < 1.0f) kick_r = 1.0f;
    DrawCircleV(c, ball_r, RCSS_BALL);
    DrawCircleLinesV(c, kick_r, RCSS_BALL);
}

static const char *const rcss_playmode_names[] = {
    "",
    "before_kick_off",
    "time_over",
    "play_on",
    "kick_off",
    "kick_in",
    "free_kick",
    "corner_kick",
    "goal_kick",
    "goal",
    "drop_ball",
    "offside",
    "penalty_kick",
    "first_half_over",
    "pause",
    "human_judge",
    "foul_charge",
    "foul_push",
    "foul_multiple_attack",
    "foul_ballout",
    "back_pass",
    "free_kick_fault",
    "catch_fault",
    "indirect_free_kick",
    "penalty_setup",
    "penalty_ready",
    "penalty_taken",
    "penalty_miss",
    "penalty_score",
    "illegal_defense",
};

static void rcss_playmode_label(char *out, int n, PlayMode pm, Side side) {
    int i = (int)pm;
    int nnames = (int)(sizeof(rcss_playmode_names) / sizeof(rcss_playmode_names[0]));
    const char *name = (i >= 0 && i < nnames) ? rcss_playmode_names[i] : "";
    if (name[0] != '\0' && side == LEFT)
        snprintf(out, n, "%s_l", name);
    else if (name[0] != '\0' && side == RIGHT)
        snprintf(out, n, "%s_r", name);
    else
        snprintf(out, n, "%s", name);
}

static void rcss_draw_scoreboard(Rectangle pane, int bar_h, const Stadium *stadium) {
    Font font = rcss_bold();
    int size = RCSS_SCOREBOARD_FONT;
    int x = (int)pane.x;
    int width = (int)pane.width;
    int top = (int)pane.y + (int)pane.height - bar_h;
    DrawRectangle(x, top, width, bar_h, RCSS_SCORE_BG);
    int ty = top + (bar_h - size) / 2;

    char score[64];
    snprintf(score, sizeof(score), "Left  %d : %d  Right",
             stadium->team_left_points, stadium->team_right_points);
    rcss_text(font, score, x + 10, ty, size, RCSS_SCORE_FG);

    char pm[64];
    rcss_playmode_label(pm, sizeof(pm), stadium->playmode, stadium->playmode_side);
    rcss_text(font, pm, x + width / 2 - rcss_width(font, pm, size) / 2, ty, size, RCSS_SCORE_FG);

    char clock[16];
    snprintf(clock, sizeof(clock), "%d", stadium->time);
    rcss_text(font, clock, x + width - rcss_width(font, clock, size) - 12, ty, size, RCSS_SCORE_FG);
}

static void rcss_draw_diag(Rectangle pane, Stadium *stadium);

static void draw_rcss(Stadium *stadium, Rectangle pane) {
    if (pane.width <= 0.0f || pane.height <= 0.0f) return;

    rcss_clip(pane);
    DrawRectangle((int)pane.x, (int)pane.y, (int)pane.width, (int)pane.height, RCSS_FIELD);

    int bar_h = RCSS_SCOREBOARD_FONT + 8;
    RcssView view = rcss_make_view(pane, bar_h);
    rcss_draw_field(&view);
    if (stadium) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            rcss_draw_player(&view, stadium, i);
        rcss_draw_ball(&view, stadium->ball_pos_x, stadium->ball_pos_y);
        rcss_draw_scoreboard(pane, bar_h, stadium);
    }
    rcss_draw_diag(pane, stadium);
    EndScissorMode();
}

typedef struct {
    bool ball;
    bool players;
} RcssDiagOpen;

static const char *rcss_diag_cmd(CommandType t) {
    static const char *const names[] = { "MOV", "DAS", "TRN", "KCK", "TKL", "---" };
    unsigned i = (unsigned)t;
    if (i >= sizeof(names) / sizeof(names[0])) return "---";
    return names[i];
}

static int rcss_diag_add_floats(TableRow *rows, int n, int max, const char *label,
                                const char *fmt, const float *vals) {
    if (n >= max) return n;
    table_row_floats(&rows[n], label, RCSS_SCORE_FG, fmt, vals, NUM_PLAYERS);
    return n + 1;
}

static int rcss_diag_add_ints(TableRow *rows, int n, int max, const char *label, const int *vals) {
    if (n >= max) return n;
    table_row_ints(&rows[n], label, RCSS_SCORE_FG, vals, NUM_PLAYERS);
    return n + 1;
}

static int rcss_diag_add_bools(TableRow *rows, int n, int max, const char *label,
                               const bool *vals, Color on) {
    if (n >= max) return n;
    table_row_bools(&rows[n], label, RCSS_SCORE_FG, on, vals, NUM_PLAYERS);
    return n + 1;
}

static int rcss_diag_add_cmds(TableRow *rows, int n, int max, const char *label,
                              const CommandType *cmds) {
    if (n >= max) return n;
    table_row_init(&rows[n], label, RCSS_SCORE_FG, NUM_PLAYERS);
    for (int i = 0; i < NUM_PLAYERS; i++)
        snprintf(rows[n].cells[i].text, sizeof(rows[n].cells[i].text), "%s", rcss_diag_cmd(cmds[i]));
    return n + 1;
}

static int rcss_fill_player_table(const Stadium *stadium, TableRow *header,
                                  TableRow *rows, int max_rows) {
    const Players *p = &stadium->players;
    Color fg = RCSS_SCORE_FG;

    table_row_init(header, "player", fg, NUM_PLAYERS);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        snprintf(header->cells[i].text, sizeof(header->cells[i].text), "%d", i);
        header->cells[i].color = rcss_side_color(p->side[i]);
    }

    int n = 0;
    if (n >= max_rows) return n;
    table_row_init(&rows[n], "id", fg, NUM_PLAYERS);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (p->side[i] == LEFT)
            snprintf(rows[n].cells[i].text, sizeof(rows[n].cells[i].text), "L%d", p->unum[i]);
        else if (p->side[i] == RIGHT)
            snprintf(rows[n].cells[i].text, sizeof(rows[n].cells[i].text), "R%d", p->unum[i]);
        else
            snprintf(rows[n].cells[i].text, sizeof(rows[n].cells[i].text), "%d", p->unum[i]);
        rows[n].cells[i].color = rcss_side_color(p->side[i]);
    }
    n++;

    n = rcss_diag_add_floats(rows, n, max_rows, "pos_x", "%.2f", p->pos_x);
    n = rcss_diag_add_floats(rows, n, max_rows, "pos_y", "%.2f", p->pos_y);
    n = rcss_diag_add_floats(rows, n, max_rows, "vel_x", "%.2f", p->vel_x);
    n = rcss_diag_add_floats(rows, n, max_rows, "vel_y", "%.2f", p->vel_y);
    n = rcss_diag_add_floats(rows, n, max_rows, "accel_x", "%.2f", p->accel_x);
    n = rcss_diag_add_floats(rows, n, max_rows, "accel_y", "%.2f", p->accel_y);
    n = rcss_diag_add_ints(rows, n, max_rows, "collision_count", p->collision_count);
    n = rcss_diag_add_bools(rows, n, max_rows, "ball_collide", p->ball_collide, RCSS_BALL_COLLIDE);
    n = rcss_diag_add_bools(rows, n, max_rows, "player_collide", p->player_collide, RCSS_PLAYER_COLLIDE);
    n = rcss_diag_add_bools(rows, n, max_rows, "post_collide", p->post_collide, RED);
    n = rcss_diag_add_floats(rows, n, max_rows, "post_col_pos_x", "%.2f", p->post_collision_pos_x);
    n = rcss_diag_add_floats(rows, n, max_rows, "post_col_pos_y", "%.2f", p->post_collision_pos_y);
    n = rcss_diag_add_floats(rows, n, max_rows, "angle_body", "%.2f", p->angle_body_committed);
    n = rcss_diag_add_floats(rows, n, max_rows, "angle_neck", "%.2f", p->angle_neck_committed);

    n = rcss_diag_add_floats(rows, n, max_rows, "stamina", "%.1f", p->stamina);
    if (n > 0) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            if (p->stamina[i] < 2000.0f) rows[n - 1].cells[i].color = RED;
    }

    n = rcss_diag_add_floats(rows, n, max_rows, "effort", "%.2f", p->effort);
    if (n > 0) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            if (p->effort[i] < p->player_type[i].effort_max - 1.0e-3f)
                rows[n - 1].cells[i].color = RCSS_EFFORT_DECAYED;
    }

    n = rcss_diag_add_floats(rows, n, max_rows, "recovery", "%.2f", p->recovery);
    if (n > 0) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            if (p->recovery[i] < 1.0f - 1.0e-3f)
                rows[n - 1].cells[i].color = RCSS_RECOVERY_DECAYED;
    }

    n = rcss_diag_add_floats(rows, n, max_rows, "stamina_capacity", "%.0f", p->stamina_capacity);
    n = rcss_diag_add_floats(rows, n, max_rows, "consumed_stamina", "%.1f", p->consumed_stamina);
    n = rcss_diag_add_cmds(rows, n, max_rows, "left_cmd", p->left_leg_command_type);
    n = rcss_diag_add_floats(rows, n, max_rows, "left_power", "%.1f", p->left_leg_dash_power);
    n = rcss_diag_add_floats(rows, n, max_rows, "left_dir", "%.2f", p->left_leg_dash_dir);
    n = rcss_diag_add_cmds(rows, n, max_rows, "right_cmd", p->right_leg_command_type);
    n = rcss_diag_add_floats(rows, n, max_rows, "right_power", "%.1f", p->right_leg_dash_power);
    n = rcss_diag_add_floats(rows, n, max_rows, "right_dir", "%.2f", p->right_leg_dash_dir);
    n = rcss_diag_add_ints(rows, n, max_rows, "kick_count", p->kick_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "dash_count", p->dash_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "tackle_count", p->tackle_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "foul_count", p->foul_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "turn_count", p->turn_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "turn_neck_count", p->turn_neck_count);
    n = rcss_diag_add_ints(rows, n, max_rows, "card_count", p->card_count);
    if (n > 0) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            if (p->card_count[i] > 0) rows[n - 1].cells[i].color = YELLOW;
    }
    return n;
}

static int rcss_diag_draw_kv(int x, int y, int font, const char *key, const char *value) {
    char buf[RCSS_DIAG_LINE_LEN];
    snprintf(buf, sizeof(buf), "  %s: %s", key, value);
    rcss_draw_text(buf, x, y, font, RCSS_SCORE_FG);
    return y + font + 2;
}

static int rcss_diag_draw_float(int x, int y, int font, const char *key, const char *fmt, float v) {
    char val[64];
    snprintf(val, sizeof(val), fmt, v);
    return rcss_diag_draw_kv(x, y, font, key, val);
}

static int rcss_diag_draw_int(int x, int y, int font, const char *key, int v) {
    char val[32];
    snprintf(val, sizeof(val), "%d", v);
    return rcss_diag_draw_kv(x, y, font, key, val);
}

static int rcss_diag_draw_ball_fields(int x, int y, int font, const Stadium *s) {
    y = rcss_diag_draw_float(x, y, font, "pos_x", "%.2f", s->ball_pos_x);
    y = rcss_diag_draw_float(x, y, font, "pos_y", "%.2f", s->ball_pos_y);
    y = rcss_diag_draw_float(x, y, font, "vel_x", "%.2f", s->ball_vel_x);
    y = rcss_diag_draw_float(x, y, font, "vel_y", "%.2f", s->ball_vel_y);
    y = rcss_diag_draw_float(x, y, font, "accel_x", "%.2f", s->ball_accel_x);
    y = rcss_diag_draw_float(x, y, font, "accel_y", "%.2f", s->ball_accel_y);
    y = rcss_diag_draw_int(x, y, font, "collision_count", s->ball_collision_count);
    y = rcss_diag_draw_kv(x, y, font, "collided", s->ball_collided ? "true" : "false");
    y = rcss_diag_draw_float(x, y, font, "post_collision_pos_x", "%.2f", s->ball_post_collision_pos_x);
    y = rcss_diag_draw_float(x, y, font, "post_collision_pos_y", "%.2f", s->ball_post_collision_pos_y);
    return y;
}

static int rcss_diag_bump_w(int w, int font, const char *fmt, ...) {
    char buf[RCSS_DIAG_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    int tw = rcss_text_width(buf, font);
    return tw > w ? tw : w;
}

static int rcss_diag_section_width(int font, bool open, const char *key, const char *summary) {
    if (!open && summary && summary[0] != '\0')
        return rcss_diag_bump_w(0, font, "+ %s: %s", key, summary);
    return rcss_diag_bump_w(0, font, "%c %s:", open ? '-' : '+', key);
}

static int rcss_diag_draw_section(int x, int y, int font, int line_h, int x0, int panel_w,
                                  bool open, const char *key, const char *summary,
                                  Vector2 mouse) {
    char buf[RCSS_DIAG_LINE_LEN];
    if (!open && summary && summary[0] != '\0')
        snprintf(buf, sizeof(buf), "+ %s: %s", key, summary);
    else
        snprintf(buf, sizeof(buf), "%c %s:", open ? '-' : '+', key);

    Rectangle hit = { (float)x0, (float)y, (float)panel_w, (float)line_h };
    if (CheckCollisionPointRec(mouse, hit))
        DrawRectangle(x0, y, panel_w, line_h, RCSS_DIAG_HOVER);

    rcss_draw_text(buf, x, y, font, RCSS_SCORE_FG);
    return y + line_h;
}

static int rcss_diag_kv_block_width(int font, const Stadium *s) {
    int w = 0;
    w = rcss_diag_bump_w(w, font, "  pos_x: %.2f", s->ball_pos_x);
    w = rcss_diag_bump_w(w, font, "  pos_y: %.2f", s->ball_pos_y);
    w = rcss_diag_bump_w(w, font, "  vel_x: %.2f", s->ball_vel_x);
    w = rcss_diag_bump_w(w, font, "  vel_y: %.2f", s->ball_vel_y);
    w = rcss_diag_bump_w(w, font, "  accel_x: %.2f", s->ball_accel_x);
    w = rcss_diag_bump_w(w, font, "  accel_y: %.2f", s->ball_accel_y);
    w = rcss_diag_bump_w(w, font, "  collision_count: %d", s->ball_collision_count);
    w = rcss_diag_bump_w(w, font, "  collided: %s", s->ball_collided ? "true" : "false");
    w = rcss_diag_bump_w(w, font, "  post_collision_pos_x: %.2f", s->ball_post_collision_pos_x);
    w = rcss_diag_bump_w(w, font, "  post_collision_pos_y: %.2f", s->ball_post_collision_pos_y);
    return w;
}

static void rcss_draw_diag(Rectangle pane, Stadium *stadium) {
    static bool on = false;
    static int scroll = 0;
    static RcssDiagOpen open = { true, true };

    if (IsKeyPressed(KEY_F1)) on = !on;
    if (!on || !stadium) return;

    int font = RCSS_DIAG_FONT;
    int pad = RCSS_DIAG_PAD;
    int line_h = font + 2;
    int x0 = (int)pane.x + 4;
    int y0 = (int)pane.y + 4;
    Vector2 mouse = GetMousePosition();

    static int last_panel_w = 200;
    int ball_hy = y0 + pad - scroll;
    int players_hy = ball_hy + line_h * (open.ball ? 11 : 1);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        mouse.x >= (float)x0 && mouse.x < (float)(x0 + last_panel_w)) {
        if (mouse.y >= (float)ball_hy && mouse.y < (float)(ball_hy + line_h))
            open.ball = !open.ball;
        else if (mouse.y >= (float)players_hy && mouse.y < (float)(players_hy + line_h))
            open.players = !open.players;
    }

    char ball_summary[RCSS_DIAG_LINE_LEN];
    snprintf(ball_summary, sizeof(ball_summary), "{pos: [%.2f, %.2f], vel: [%.2f, %.2f]}",
             stadium->ball_pos_x, stadium->ball_pos_y,
             stadium->ball_vel_x, stadium->ball_vel_y);
    char players_summary[64];
    snprintf(players_summary, sizeof(players_summary), "{n: %d}", NUM_PLAYERS);

    TableRow header;
    TableRow rows[RCSS_PLAYER_TABLE_ROWS];
    Table players = {0};
    if (open.players) {
        int n = rcss_fill_player_table(stadium, &header, rows, RCSS_PLAYER_TABLE_ROWS);
        players.font = rcss_font();
        players.font_size = font;
        players.ncols = NUM_PLAYERS;
        players.header = &header;
        players.rows = rows;
        players.nrows = n;
    }

    int content_w = rcss_diag_section_width(font, open.ball, "ball", ball_summary);
    if (open.ball) {
        int kv_w = rcss_diag_kv_block_width(font, stadium);
        if (kv_w > content_w) content_w = kv_w;
    }
    int pw = rcss_diag_section_width(font, open.players, "players", players_summary);
    if (pw > content_w) content_w = pw;
    if (open.players) {
        int tw = table_width(&players);
        if (tw > content_w) content_w = tw;
    }

    int max_panel_w = (int)pane.width - 8;
    if (max_panel_w < 1) max_panel_w = 1;
    int panel_w = rcss_clampi(content_w + pad * 2, 1, max_panel_w);

    int kv_h = line_h * ((open.ball ? 11 : 1) + 1);
    int table_h = open.players ? table_height(&players) : 0;
    int content_h = kv_h + table_h;
    int max_panel_h = (int)pane.height - 32;
    if (max_panel_h < line_h + pad * 2) max_panel_h = line_h + pad * 2;
    int panel_h = content_h + pad * 2;
    if (panel_h > max_panel_h) panel_h = max_panel_h;

    Rectangle panel = { (float)x0, (float)y0, (float)panel_w, (float)panel_h };
    bool hover = CheckCollisionPointRec(mouse, panel);
    int max_scroll = content_h - (panel_h - pad * 2);
    if (max_scroll < 0) max_scroll = 0;
    if (hover)
        scroll -= (int)GetMouseWheelMove() * line_h;
    scroll = rcss_clampi(scroll, 0, max_scroll);
    last_panel_w = panel_w;

    DrawRectangle(x0, y0, panel_w, panel_h, RCSS_DIAG_BG);
    rcss_clip(panel);

    int x = x0 + pad;
    int y = y0 + pad - scroll;
    y = rcss_diag_draw_section(x, y, font, line_h, x0, panel_w, open.ball, "ball",
                               ball_summary, mouse);
    if (open.ball)
        y = rcss_diag_draw_ball_fields(x, y, font, stadium);
    y = rcss_diag_draw_section(x, y, font, line_h, x0, panel_w, open.players, "players",
                               players_summary, mouse);
    if (open.players)
        table_draw(x, y, &players);

    EndScissorMode();
}

#endif
