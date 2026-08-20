#ifndef RENDER_H
#define RENDER_H

#include <math.h>
#include <stdio.h>

#include "raylib.h"
#include "stadium.h"

#define RCSS_FIELD_COLOR         (Color){ 31, 160,  31, 255 }
#define RCSS_LINE_COLOR          (Color){255, 255, 255, 255 }
#define RCSS_SCORE_PEN           (Color){255, 255, 255, 255 }
#define RCSS_SCORE_BRUSH         (Color){  0,   0,   0, 255 }
#define RCSS_BALL_COLOR          (Color){255, 255, 255, 255 }
#define RCSS_PLAYER_PEN          (Color){  0,   0,   0, 255 }
#define RCSS_LEFT_TEAM           (Color){255, 215,   0, 255 }
#define RCSS_LEFT_GOALIE         (Color){ 39, 231,  31, 255 }
#define RCSS_RIGHT_TEAM          (Color){  0, 191, 255, 255 }
#define RCSS_RIGHT_GOALIE        (Color){255, 153, 255, 255 }
#define RCSS_PLAYER_NUMBER       (Color){255, 255, 255, 255 }
#define RCSS_BALL_COLLIDE        (Color){255,   0,   0, 255 }
#define RCSS_PLAYER_COLLIDE      (Color){105, 155, 235, 255 }
#define RCSS_KICK_PEN            (Color){255, 255, 255, 255 }
#define RCSS_KICK_FAULT          (Color){255,   0,   0, 255 }
#define RCSS_CATCH_BRUSH         (Color){ 10,  80,  10, 255 }
#define RCSS_CATCH_FAULT         (Color){ 10,  80, 150, 255 }
#define RCSS_TACKLE              (Color){255, 136, 127, 255 }
#define RCSS_TACKLE_FAULT        (Color){ 79, 159, 159, 255 }
#define RCSS_FOUL_CHARGED        (Color){  0, 127,   0, 255 }
#define RCSS_EFFORT_DECAYED      (Color){255,   0,   0, 255 }
#define RCSS_RECOVERY_DECAYED    (Color){255, 231,  31, 255 }

#define RCSS_CENTER_CIRCLE_R     9.15f
#define RCSS_PENALTY_CIRCLE_R    9.15f
#define RCSS_PENALTY_SPOT_DIST   11.0f
#define RCSS_GOAL_DEPTH          2.44f
#define RCSS_CORNER_ARC_R        1.0f
#define RCSS_BALL_DRAW_SIZE      0.35f
#define RCSS_SCOREBOARD_FONT     16
#define RCSS_PLAYER_FONT         10

#define RCSS_PITCH_HALF_L        (PITCH_LENGTH * 0.5f)
#define RCSS_PITCH_HALF_W        (PITCH_WIDTH * 0.5f)

static const char *rcss_playmode_names[] = {
    "",
    "before_kick_off",
    "time_over",
    "play_on",
    "kick_off_l",
    "kick_off_r",
    "kick_in_l",
    "kick_in_r",
    "free_kick_l",
    "free_kick_r",
    "corner_kick_l",
    "corner_kick_r",
    "goal_kick_l",
    "goal_kick_r",
    "goal_l",
    "goal_r",
    "drop_ball",
    "offside_l",
    "offside_r",
    "penalty_kick_l",
    "penalty_kick_r",
    "first_half_over",
    "pause",
    "human_judge",
    "foul_charge_l",
    "foul_charge_r",
    "foul_push_l",
    "foul_push_r",
    "foul_multiple_attack_l",
    "foul_multiple_attack_r",
    "foul_ballout_l",
    "foul_ballout_r",
    "back_pass_l",
    "back_pass_r",
    "free_kick_fault_l",
    "free_kick_fault_r",
    "catch_fault_l",
    "catch_fault_r",
    "indirect_free_kick_l",
    "indirect_free_kick_r",
    "penalty_setup_l",
    "penalty_setup_r",
    "penalty_ready_l",
    "penalty_ready_r",
    "penalty_taken_l",
    "penalty_taken_r",
    "penalty_miss_l",
    "penalty_miss_r",
    "penalty_score_l",
    "penalty_score_r",
    "illegal_defense_l",
    "illegal_defense_r",
};

typedef struct {
    float scale;
    float cx;
    float cy;
} RcssView;

static inline float rcss_sx(const RcssView *v, float x) {
    return v->cx + x * v->scale;
}

static inline float rcss_sy(const RcssView *v, float y) {
    return v->cy + y * v->scale;
}

static inline float rcss_sc(const RcssView *v, float len) {
    return len * v->scale;
}

static inline Color rcss_darker(Color c, int factor) {
    if (factor <= 100) return c;
    c.r = (unsigned char)((int)c.r * 100 / factor);
    c.g = (unsigned char)((int)c.g * 100 / factor);
    c.b = (unsigned char)((int)c.b * 100 / factor);
    return c;
}

static void rcss_draw_arc(const RcssView *v, float x, float y, float radius_m,
                          float start_deg, float end_deg, Color color) {
    float r = rcss_sc(v, radius_m);
    if (r < 1.0f) return;
    Vector2 c = { rcss_sx(v, x), rcss_sy(v, y) };
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

static void rcss_draw_h(const RcssView *v, float x0, float x1, float y, Color c) {
    DrawLineV((Vector2){ rcss_sx(v, x0), rcss_sy(v, y) },
              (Vector2){ rcss_sx(v, x1), rcss_sy(v, y) }, c);
}

static void rcss_draw_v(const RcssView *v, float x, float y0, float y1, Color c) {
    DrawLineV((Vector2){ rcss_sx(v, x), rcss_sy(v, y0) },
              (Vector2){ rcss_sx(v, x), rcss_sy(v, y1) }, c);
}

static RcssView rcss_make_view(int width, int height, int scoreboard_h) {
    float pitch_l = PITCH_LENGTH + PITCH_MARGIN * 2.0f + 1.0f;
    float pitch_w = PITCH_WIDTH + PITCH_MARGIN * 2.0f;
    int field_h = height - scoreboard_h;
    if (field_h < 1) field_h = 1;
    float scale = (float)width / pitch_l;
    if (pitch_w * scale > (float)field_h)
        scale = (float)field_h / pitch_w;
    if (scale < 1.0f) scale = 1.0f;
    scale = roundf(scale * 100.0f) / 100.0f;
    RcssView v;
    v.scale = scale;
    v.cx = (float)width * 0.5f;
    v.cy = (float)field_h * 0.5f;
    return v;
}

static void rcss_draw_field(const RcssView *v) {
    const Color line = RCSS_LINE_COLOR;
    const float l = -RCSS_PITCH_HALF_L;
    const float r = RCSS_PITCH_HALF_L;
    const float t = -RCSS_PITCH_HALF_W;
    const float b = RCSS_PITCH_HALF_W;

    rcss_draw_h(v, l, r, t, line);
    rcss_draw_h(v, l, r, b, line);
    rcss_draw_v(v, l, t, b, line);
    rcss_draw_v(v, r, t, b, line);
    rcss_draw_v(v, 0.0f, t, b, line);

    DrawCircleLinesV((Vector2){ rcss_sx(v, 0.0f), rcss_sy(v, 0.0f) },
                     rcss_sc(v, RCSS_CENTER_CIRCLE_R), line);

    rcss_draw_arc(v, l, t, RCSS_CORNER_ARC_R, 0.0f, 90.0f, line);
    rcss_draw_arc(v, l, b, RCSS_CORNER_ARC_R, 270.0f, 360.0f, line);
    rcss_draw_arc(v, r, b, RCSS_CORNER_ARC_R, 180.0f, 270.0f, line);
    rcss_draw_arc(v, r, t, RCSS_CORNER_ARC_R, 90.0f, 180.0f, line);

    float pen_y = PENALTY_AREA_WIDTH * 0.5f;
    float pen_x = RCSS_PITCH_HALF_L - PENALTY_AREA_LENGTH;
    rcss_draw_h(v, l, -pen_x, -pen_y, line);
    rcss_draw_h(v, l, -pen_x, pen_y, line);
    rcss_draw_v(v, -pen_x, -pen_y, pen_y, line);
    rcss_draw_h(v, r, pen_x, -pen_y, line);
    rcss_draw_h(v, r, pen_x, pen_y, line);
    rcss_draw_v(v, pen_x, -pen_y, pen_y, line);

    float spot = RCSS_PITCH_HALF_L - RCSS_PENALTY_SPOT_DIST;
    DrawCircleV((Vector2){ rcss_sx(v, -spot), rcss_sy(v, 0.0f) }, 1.5f, line);
    DrawCircleV((Vector2){ rcss_sx(v, spot), rcss_sy(v, 0.0f) }, 1.5f, line);

    float ratio = (PENALTY_AREA_LENGTH - RCSS_PENALTY_SPOT_DIST) / RCSS_PENALTY_CIRCLE_R;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < -1.0f) ratio = -1.0f;
    float alpha = acosf(ratio) * RAD2DEG;
    rcss_draw_arc(v, -spot, 0.0f, RCSS_PENALTY_CIRCLE_R, -alpha, alpha, line);
    rcss_draw_arc(v, spot, 0.0f, RCSS_PENALTY_CIRCLE_R, 180.0f - alpha, 180.0f + alpha, line);

    float ga_y = GOAL_AREA_WIDTH * 0.5f;
    float ga_x = RCSS_PITCH_HALF_L - GOAL_AREA_LENGTH;
    rcss_draw_h(v, l, -ga_x, -ga_y, line);
    rcss_draw_h(v, l, -ga_x, ga_y, line);
    rcss_draw_v(v, -ga_x, -ga_y, ga_y, line);
    rcss_draw_h(v, r, ga_x, -ga_y, line);
    rcss_draw_h(v, r, ga_x, ga_y, line);
    rcss_draw_v(v, ga_x, -ga_y, ga_y, line);

    float goal_top = rcss_sy(v, -GOAL_WIDTH * 0.5f);
    float goal_h = rcss_sc(v, GOAL_WIDTH);
    float goal_d = rcss_sc(v, RCSS_GOAL_DEPTH);
    DrawRectangleV((Vector2){ rcss_sx(v, l - RCSS_GOAL_DEPTH) - 1.0f, goal_top },
                   (Vector2){ goal_d, goal_h }, BLACK);
    DrawRectangleV((Vector2){ rcss_sx(v, r) + 1.0f, goal_top },
                   (Vector2){ goal_d, goal_h }, BLACK);

    float post_d = rcss_sc(v, GOAL_POST_RADIUS * 2.0f);
    if (post_d >= 1.0f) {
        float post_r = post_d * 0.5f;
        float gy = GOAL_WIDTH * 0.5f;
        DrawCircleV((Vector2){ rcss_sx(v, l), rcss_sy(v, -gy) }, post_r, BLACK);
        DrawCircleV((Vector2){ rcss_sx(v, l), rcss_sy(v, gy) }, post_r, BLACK);
        DrawCircleV((Vector2){ rcss_sx(v, r), rcss_sy(v, -gy) }, post_r, BLACK);
        DrawCircleV((Vector2){ rcss_sx(v, r), rcss_sy(v, gy) }, post_r, BLACK);
    }
}

static Color rcss_player_brush(Stadium *stadium, int player_index) {
    Color brush = BLACK;
    if (stadium->players.side[player_index] == LEFT)
        brush = stadium->players.goalie[player_index] ? RCSS_LEFT_GOALIE : RCSS_LEFT_TEAM;
    else if (stadium->players.side[player_index] == RIGHT)
        brush = stadium->players.goalie[player_index] ? RCSS_RIGHT_GOALIE : RCSS_RIGHT_TEAM;

    if (stadium->players.state[player_index] & STATE_KICK_FAULT) brush = RCSS_KICK_FAULT;
    if (stadium->players.state[player_index] & STATE_CATCH) brush = RCSS_CATCH_BRUSH;
    if (stadium->players.state[player_index] & STATE_CATCH_FAULT) brush = RCSS_CATCH_FAULT;
    if (stadium->players.state[player_index] & STATE_TACKLE) brush = RCSS_TACKLE;
    if (stadium->players.state[player_index] & STATE_TACKLE_FAULT) brush = RCSS_TACKLE_FAULT;
    if (stadium->players.state[player_index] & STATE_FOUL_CHARGED) brush = RCSS_FOUL_CHARGED;
    if (stadium->players.state[player_index] & STATE_BALL_COLLIDE) brush = RCSS_BALL_COLLIDE;
    if (stadium->players.state[player_index] & STATE_PLAYER_COLLIDE) brush = RCSS_PLAYER_COLLIDE;
    return brush;
}

static Color rcss_player_outline(Stadium *stadium, int player_index) {
    if ((stadium->players.state[player_index] & STATE_TACKLE) || (stadium->players.state[player_index] & STATE_TACKLE_FAULT))
        return RCSS_TACKLE;
    if (stadium->players.state[player_index] & STATE_KICK)
        return RCSS_KICK_PEN;
    return RCSS_PLAYER_PEN;
}

static void rcss_draw_player(const RcssView *v, Stadium *stadium, int player_index) {
    float size = stadium->players.player_type[player_index].player_size;
    float kickable = stadium->players.player_type[player_index].kickable_margin;
    if (size < 0.01f) size = PLAYER_SIZE;
    if (kickable < 0.01f) kickable = KICKABLE_MARGIN;

    float body_r = rcss_sc(v, size);
    float kick_r = rcss_sc(v, size + kickable + BALL_SIZE);
    if (body_r < 1.0f) body_r = 1.0f;
    if (kick_r < 5.0f) kick_r = 5.0f;

    Vector2 c = { rcss_sx(v, stadium->players.pos_x[player_index]), rcss_sy(v, stadium->players.pos_y[player_index]) };
    Color brush = rcss_player_brush(stadium, player_index);
    Color outline = rcss_player_outline(stadium, player_index);

    DrawCircleV(c, kick_r, brush);
    DrawCircleLinesV(c, kick_r, outline);
    if (stadium->players.state[player_index] & STATE_KICK)
        DrawCircleLinesV(c, kick_r + 1.0f, outline);

    float stamina_rate = stadium->players.stamina[player_index] / (float)STAMINA_MAX;
    if (stamina_rate < 0.0f) stamina_rate = 0.0f;
    if (stamina_rate > 1.0f) stamina_rate = 1.0f;
    int dark_rate = 200 - (int)roundf(200.0f * roundf(stamina_rate / 0.125f) * 0.125f);
    dark_rate -= 50;
    if (dark_rate < 0) dark_rate = 0;
    DrawCircleV(c, body_r, dark_rate == 0 ? brush : rcss_darker(brush, 100 + dark_rate));
    DrawCircleLinesV(c, body_r, RCSS_PLAYER_PEN);

    if (stadium->players.effort[player_index] < stadium->players.player_type[player_index].effort_max - 1.0e-3f)
        DrawCircleLinesV(c, kick_r + 2.0f, RCSS_EFFORT_DECAYED);
    else if (stadium->players.recovery[player_index] < 1.0f - 1.0e-3f)
        DrawCircleLinesV(c, kick_r + 2.0f, RCSS_RECOVERY_DECAYED);

    float dir_r = size + kickable + BALL_SIZE;
    float body = stadium->players.angle_body_committed[player_index];
    DrawLineV(c, (Vector2){
        rcss_sx(v, stadium->players.pos_x[player_index] + dir_r * cosf(body)),
        rcss_sy(v, stadium->players.pos_y[player_index] + dir_r * sinf(body))
    }, RCSS_PLAYER_PEN);

    char label[32];
    snprintf(label, sizeof(label), "%d", stadium->players.unum[player_index]);
    int text_r = (int)kick_r;
    if (text_r > 40) text_r = 40;
    int tx = (int)c.x + text_r;
    int ty = (int)c.y - RCSS_PLAYER_FONT / 2;
    if ((stadium->players.state[player_index] & STATE_RED_CARD) || (stadium->players.state[player_index] & STATE_YELLOW_CARD)) {
        int card_w = 6;
        int card_h = RCSS_PLAYER_FONT;
        DrawRectangle(tx, ty, card_w, card_h,
                      (stadium->players.state[player_index] & STATE_RED_CARD) ? RED : YELLOW);
        tx += card_w + 2;
    }
    DrawText(label, tx, ty, RCSS_PLAYER_FONT, RCSS_PLAYER_NUMBER);
}

static void rcss_draw_ball(const RcssView *v, const float ball_pos_x, const float ball_pos_y) {
    Vector2 c = { rcss_sx(v, ball_pos_x), rcss_sy(v, ball_pos_y) };
    float ball_r = rcss_sc(v, RCSS_BALL_DRAW_SIZE);
    if (ball_r < 1.0f) ball_r = 1.0f;
    float kick_r = rcss_sc(v, PLAYER_SIZE + KICKABLE_MARGIN + BALL_SIZE);
    if (kick_r < 1.0f) kick_r = 1.0f;
    DrawCircleV(c, ball_r, RCSS_BALL_COLOR);
    DrawCircleLinesV(c, kick_r, RCSS_BALL_COLOR);
}

static const char *rcss_playmode_name(PlayMode pm) {
    int n = (int)(sizeof(rcss_playmode_names) / sizeof(rcss_playmode_names[0]));
    int i = (int)pm;
    if (i < 0 || i >= n) return "";
    return rcss_playmode_names[i];
}

static int rcss_text_width(const char *s, int size) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n * size * 2 / 3;
}

static void rcss_draw_scoreboard(int width, int height, int bar_h, const Stadium *stadium) {
    DrawRectangle(0, height - bar_h, width, bar_h, RCSS_SCORE_BRUSH);
    int ty = height - bar_h + (bar_h - RCSS_SCOREBOARD_FONT) / 2;
    char left[64];
    snprintf(left, sizeof(left), "Left  %d : %d  Right",
             stadium->team_left_points, stadium->team_right_points);
    DrawText(left, 10, ty, RCSS_SCOREBOARD_FONT, RCSS_SCORE_PEN);

    const char *pm = rcss_playmode_name(stadium->playmode);
    int pm_w = rcss_text_width(pm, RCSS_SCOREBOARD_FONT);
    DrawText(pm, width / 2 - pm_w / 2, ty, RCSS_SCOREBOARD_FONT, RCSS_SCORE_PEN);

    char clock[16];
    snprintf(clock, sizeof(clock), "%d", stadium->time);
    int clock_w = rcss_text_width(clock, RCSS_SCOREBOARD_FONT);
    DrawText(clock, width - clock_w - 12, ty, RCSS_SCOREBOARD_FONT, RCSS_SCORE_PEN);
}

void render(Stadium *stadium) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    if (width <= 0 || height <= 0) return;

    int bar_h = RCSS_SCOREBOARD_FONT + 8;
    RcssView view = rcss_make_view(width, height, bar_h);

    BeginDrawing();
    ClearBackground(RCSS_FIELD_COLOR);
    rcss_draw_field(&view);
    if (stadium) {
        for (int i = 0; i < NUM_PLAYERS; i++)
            rcss_draw_player(&view, stadium, i);
        rcss_draw_ball(&view, stadium->ball_pos_x, stadium->ball_pos_y);
        rcss_draw_scoreboard(width, height, bar_h, stadium);
    }
    EndDrawing();
}

#endif
