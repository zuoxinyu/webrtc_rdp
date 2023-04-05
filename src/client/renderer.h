#ifndef RENDERER_H
#define RENDERER_H

#include "microui.h"

typedef struct SDL_Window SDL_Window;

void r_init(void);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
 int r_get_text_width(const char *text, int len);
 int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
void r_clear(mu_Color color);
void r_present(void);

extern const char button_map[256];
extern const char key_map[256];
extern float bg[3];

SDL_Window *r_get_window();

#endif

