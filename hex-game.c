#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

#include "weighted-quick-union.h"


typedef enum cell_color {NEUTRAL = 0, RED = 1, BLUE = 2} cell_color;

typedef enum hexgame_scene 
{main_menu_scene = 0, board_size_menu_scene, grid_scene, result_scene} hexgame_scene;

struct hexgame {
	unsigned short redraw : 1;
	unsigned short reset : 1;
	unsigned short fullscreen : 1;
	size_t user_chosen_board_size;
	size_t hovered_button;
	size_t hovered_cell;
	cell_color current_player;
	cell_color winner;
	hexgame_scene scene;
};

#define HEXGAME_FLAG_ON(flags, member) ((flags).member = ~((flags).member ^ (flags).member))
#define HEXGAME_FLAG_OFF(flags, member) ((flags).member = ((flags).member ^ (flags).member))
#define HEXGAME_FLIP_FLAG(flags, member) ((flags).member = ~(flags).member)


#define AL_RED al_map_rgb(255, 0, 0)
#define AL_BLUE al_map_rgb(0, 0, 255)
#define AL_RED_HOVERED al_map_rgba(255, 0, 0, 51)
#define AL_BLUE_HOVERED al_map_rgba(0, 0, 255, 51)
#define AL_BLACK al_map_rgb(0,0,0)
#define AL_WHITE al_map_rgb(255,255,255)


struct hexgrid_cell {
	cell_color color;
	bool hovered;
};

typedef struct hex_grid {
	struct wqu_uf disjoint_set;
	struct hexgrid_cell *cells;
	size_t size;
	
} hex_grid;

#define HEXGAME_FIRST_PLAYER RED

#define RED_VIRTUAL_CELLS_START(g) ((g)->size*(g)->size)
#define BLUE_VIRTUAL_CELLS_START(g) (RED_VIRTUAL_CELLS_START((g)) + 2)

#define DEF_GRID_SIZE 19
static struct wqu_node def_grid_nodes[DEF_GRID_SIZE*DEF_GRID_SIZE+4];
static struct hexgrid_cell def_grid_cells[DEF_GRID_SIZE*DEF_GRID_SIZE];

static hex_grid def_grid = {
	.disjoint_set.nodes = def_grid_nodes,
	.disjoint_set.size = DEF_GRID_SIZE*DEF_GRID_SIZE+4, 
	.disjoint_set.count = DEF_GRID_SIZE*DEF_GRID_SIZE+4,
	.size = DEF_GRID_SIZE,
	.cells = def_grid_cells
};

static void hex_def_grid_init() {
	
	for(size_t i = 0; i < def_grid.size*def_grid.size+4; i++) {
		def_grid_nodes[i].id = i;
		def_grid_nodes[i].size = 1;
	}
	memset(def_grid.cells, 0, sizeof(struct hexgrid_cell) * def_grid.size * def_grid.size);
	
}

struct point {
	float x, y;
};

/* pointy_hex_corner() is stolen from https://www.redblobgames.com/grids/hexagons/#angles
   because I suck at geometry :/
*/
#define PI 3.141593f
static struct point pointy_hex_corner(const struct point *center, float size, unsigned i) {
	
	float angle_deg = 60.0f * i - 30.0f;
	float angle_rad = PI / 180.0f * angle_deg;
	return (struct point) {.x = center->x + size * cosf(angle_rad), 
						   .y = center->y + size * sinf(angle_rad)};
	
}

static void draw_filled_hexagon(float x, float y, float r, ALLEGRO_COLOR color) {
	
	float vertices[22];
	
	struct point center = {.x = x, .y = y};
	struct point corners[6];
	for(unsigned i = 0; i < 6; i++) {
		corners[5-i] = pointy_hex_corner(&center, r, i);
	}
	for(unsigned i = 0; i <= 16; i += 4) {
		vertices[i] = corners[i/4].x;
		vertices[i+1] = corners[i/4].y;
		vertices[i+2] = corners[i/4 + 1].x;
		vertices[i+3] = corners[i/4 + 1].y;
		
	}
	vertices[20] = corners[5].x;
	vertices[21] = corners[5].y;
	
	al_draw_filled_polygon(vertices, (sizeof(vertices)/sizeof(float))/2, color);
}

static void draw_hexagon(float x, float y, float r, ALLEGRO_COLOR color) {
	
	float vertices[24];
	
	struct point center = {.x = x, .y = y};
	struct point corners[6];
	for(unsigned i = 0; i < 6; i++) {
		corners[i] = pointy_hex_corner(&center, r, i);
	}
	for(unsigned i = 0; i <= 16; i += 4) {
		vertices[i] = corners[i/4].x;
		vertices[i+1] = corners[i/4].y;
		vertices[i+2] = corners[i/4 + 1].x;
		vertices[i+3] = corners[i/4 + 1].y;
		
	}
	vertices[20] = corners[5].x;
	vertices[21] = corners[5].y;
	vertices[22] = corners[0].x;
	vertices[23] = corners[0].y;

	al_draw_polygon(vertices, (sizeof(vertices)/sizeof(float))/2, ALLEGRO_LINE_JOIN_BEVEL, color, 1.5f, 1.0f);
}


#define GRID_REGION_WIDTH(display) (al_get_display_width(display))
#define GRID_REGION_HEIGHT(display) (al_get_display_height(display))
#define SQRT_3 1.732051f
#define H_OFFSET(grid_size, width) ((width) / (grid_size * 1.5f))
#define V_OFFSET(grid_size, height) ((height) / (grid_size))
#define CELL_WIDTH(grid_size, width, h_offset) (((width) - (h_offset) * 2) / (grid_size * 1.5f))
#define CELL_SIZE(cell_width) ((cell_width) / SQRT_3)
#define CELL_HEIGHT(cell_size) ((cell_size) * 2.0f)
#define CELL_X(h_offset, cell_width, i, j) ((h_offset) + (cell_width) * (j) + (cell_width)/2.0f * (i))
#define CELL_Y(v_offset, cell_height, i) ((v_offset) + 0.75f * (cell_height) * (i))

static void hex_grid_draw(struct hexgame *game, ALLEGRO_DISPLAY *display, const hex_grid *g) {
	
	const float width = GRID_REGION_WIDTH(display);
	const float height = GRID_REGION_HEIGHT(display);
	const float h_offset = H_OFFSET(g->size, width);
	const float v_offset = V_OFFSET(g->size, height);
	const float cell_width = CELL_WIDTH(g->size, width, h_offset);
	const float cell_size = CELL_SIZE(cell_width);
	const float cell_height = CELL_HEIGHT(cell_size);
	
	/* grid borders */
	for (size_t j = 0; j < g->size-1; j++) {
		
		/* red borders */
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, 0, j), 
		CELL_Y(v_offset, cell_height, 0) - cell_size, 
		CELL_X(h_offset, cell_width, 0, j+1), 
		CELL_Y(v_offset, cell_height, 0) - cell_size,
		CELL_X(h_offset, cell_width, 0, j) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, 0) - cell_size/2.0f,
		AL_RED);
		
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, g->size-1, j), 
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size, 
		CELL_X(h_offset, cell_width, g->size-1, j+1), 
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size,
		CELL_X(h_offset, cell_width, g->size-1, j) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size/2.0f,
		AL_RED);
		
		/* blue borders */
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, j, 0) - cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) + cell_height/4.0f, 
		CELL_X(h_offset, cell_width, j, 0), 
		CELL_Y(v_offset, cell_height, j) + cell_height/2.0f,
		CELL_X(h_offset, cell_width, j, 0),
		CELL_Y(v_offset, cell_height, j+1) + cell_height/4.0f,
		AL_BLUE);
		
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, j, g->size-1) + cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) - cell_height/4.0f, 
		CELL_X(h_offset, cell_width, j, g->size-1) + cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) + cell_height/4.0f,
		CELL_X(h_offset, cell_width, j+1, g->size-1) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, j+1) - cell_height/4.0f,
		AL_BLUE);
	}
	
	
	ALLEGRO_COLOR al_red = AL_RED;
	ALLEGRO_COLOR al_red_hovered = AL_RED_HOVERED;
	ALLEGRO_COLOR al_blue = AL_BLUE;
	ALLEGRO_COLOR al_blue_hovered = AL_BLUE_HOVERED;
	ALLEGRO_COLOR al_white = AL_WHITE;
	ALLEGRO_COLOR *color;
	
	/* grid cells */
	for (size_t i = 0; i < g->size; i++) {
		for (size_t j = 0; j < g->size; j++) {
			
			if (g->cells[i*g->size + j].color == RED) {
				color = &al_red;
			}
			else if (g->cells[i*g->size + j].color == BLUE) {
				color = &al_blue;
			}
			else if(g->cells[i*g->size + j].hovered) {	
					if(game->current_player == RED) color = &al_red_hovered;
					else color = &al_blue_hovered;
			}
			else color = &al_white;
			
			draw_filled_hexagon(CELL_X(h_offset, cell_width, i, j), 
						CELL_Y(v_offset, cell_height, i), 
						cell_size, *color);
			
			draw_hexagon(CELL_X(h_offset, cell_width, i, j), 
			CELL_Y(v_offset, cell_height, i), 
			cell_size, AL_BLACK);
		}
	}
}

static size_t get_cell_index_from_mouse_coordinates(ALLEGRO_DISPLAY *display, 
													const hex_grid *g, 
													int x, int y) {
	
	const float width = GRID_REGION_WIDTH(display);
	const float height = GRID_REGION_HEIGHT(display);
	const float h_offset = H_OFFSET(g->size, width);
	const float v_offset = V_OFFSET(g->size, height);
	const float cell_width = CELL_WIDTH(g->size, width, h_offset);
	const float cell_size = CELL_SIZE(cell_width);
	const float cell_height = CELL_HEIGHT(cell_size);
	
	float i_f = ((float)y - v_offset) / CELL_Y(0, cell_height, 1);
	size_t i = (size_t) llroundf(i_f);
	float j_f = ((float)x - h_offset - (cell_width)/2.0f * i_f) / CELL_X(0, cell_width, 0, 1);
	size_t j = (size_t) llroundf(j_f);
	
	if(i < g->size && j < g->size) {
		return i * g->size + j;
	}
	return (size_t)-1;
}

static void open_cell(struct hexgame *game, hex_grid *g, size_t i) {
	
	assert(i < g->size * g->size);
	
	if(g->cells[i].color != NEUTRAL) {
		return;
	}
	
	size_t x = i % g->size;
	size_t y = i / g->size;
	g->cells[i].color = game->current_player;
	
	
	size_t neighbors[6];
	neighbors[0] = i - g->size;
	neighbors[1] = (x != (g->size - 1) ? (i - g->size + 1) : (size_t)-1);
	neighbors[2] = (x != (g->size-1) ? (i + 1) : (size_t)-1);
	neighbors[3] = i + g->size;
	neighbors[4] = (x != 0 ? (i + g->size - 1) : (size_t)-1);
	neighbors[5] = (x != 0 ? (i - 1) : (size_t)-1);
	
	for(size_t n = 0; n < 6; n++) {
		if(neighbors[n] < g->size * g->size && 
		   g->cells[neighbors[n]].color == g->cells[i].color)
		{
			w_quickunion_union(&g->disjoint_set, neighbors[n], i);
		}
	}
	
	size_t red_idx = RED_VIRTUAL_CELLS_START(g);
	size_t blue_idx = BLUE_VIRTUAL_CELLS_START(g);
	
	if(y == 0 && game->current_player == RED) { // upper
		w_quickunion_union(&g->disjoint_set, red_idx, i);
	}
	else if(y == (g->size - 1) && game->current_player == RED) { // lower
		w_quickunion_union(&g->disjoint_set, red_idx+1, i);
	}
	else if(x == 0 && game->current_player == BLUE) { // left
		w_quickunion_union(&g->disjoint_set, blue_idx, i);
	}
	else if(x == (g->size-1) && game->current_player == BLUE) { // right
		w_quickunion_union(&g->disjoint_set, blue_idx+1, i);
	}
	
	game->current_player = 1 + (game->current_player % 2);
	
}

static cell_color get_winner(hex_grid *g) {
	
	size_t red_idx = RED_VIRTUAL_CELLS_START(g);
	if(w_quickunion_is_connected(&g->disjoint_set, red_idx, red_idx+1)) return RED;
	size_t blue_idx = BLUE_VIRTUAL_CELLS_START(g);
	if(w_quickunion_is_connected(&g->disjoint_set, blue_idx, blue_idx+1)) return BLUE;
	return NEUTRAL;
}

static void show_winner(struct hexgame *game, ALLEGRO_DISPLAY *display, 
						ALLEGRO_FONT *font, 
						ALLEGRO_FONT *font_big) {
	
	static char *msg[3] = {[RED] = "Red Wins!", [BLUE] = "Blue Wins!"};
	if(game->winner != NEUTRAL) {
		al_draw_text(font_big, AL_BLUE, 
		50, 
		al_get_display_height(display) - 100,
		0, msg[game->winner]);
	}
	else return;
	
	al_draw_multiline_text(font, AL_BLACK, 
	50, al_get_display_height(display) - 50, 250, 0,
     ALLEGRO_ALIGN_LEFT, "Press R to play again,\nM to return to main menu");
	
}


struct menu_button {
	const char *title;
	bool hovered;
};

#define MAIN_MENU_BUTTON_NUM 2
static struct menu_button main_menu[MAIN_MENU_BUTTON_NUM] = {
	{.title = "Start", .hovered = false},
	{.title = "Quit", .hovered = false}
};

#define BOARD_SIZE_MENU_BUTTON_NUM 4
static struct menu_button board_size_menu[BOARD_SIZE_MENU_BUTTON_NUM] = {
	{.title = "11x11", .hovered = false},
	{.title = "13x13", .hovered = false},
	{.title = "14x14", .hovered = false},
	{.title = "19x19", .hovered = false}
};
static size_t board_sizes[BOARD_SIZE_MENU_BUTTON_NUM] = {11, 13, 14, 19};


#define MENU_BUTTON_WIDTH 250
#define MENU_BUTTON_HEIGHT 50
#define MENU_BUTTON_MARGIN 50
#define Y1_OFFSET(height, buttons_num) \
((height) - ((MENU_BUTTON_MARGIN + MENU_BUTTON_HEIGHT) * (buttons_num + 1)))

#define MENU_BUTTON_X1(width) ((width) / 2 - MENU_BUTTON_WIDTH/2) 
#define MENU_BUTTON_Y1(y1_offset) ((y1_offset) + MENU_BUTTON_MARGIN)
#define MENU_BUTTON_X2(width) ((width) / 2 + MENU_BUTTON_WIDTH/2) 
#define MENU_BUTTON_Y2(y1_offset) (MENU_BUTTON_Y1(y1_offset) + MENU_BUTTON_HEIGHT) 

static void menu_show(ALLEGRO_DISPLAY *display, ALLEGRO_FONT *font,
					  struct menu_button *menu, size_t buttons_num) {
	
	const size_t width = (size_t)al_get_display_width(display);
	const size_t height = (size_t)al_get_display_height(display);
	float y1_offset = Y1_OFFSET(height, buttons_num);
	for(size_t i = 0; i < buttons_num; i++) {
		
		if(menu[i].hovered) {
			al_draw_rectangle(MENU_BUTTON_X1(width), MENU_BUTTON_Y1(y1_offset), 
			MENU_BUTTON_X2(width), MENU_BUTTON_Y2(y1_offset),
			AL_BLUE, 1);
		}
		else {
			al_draw_rectangle(MENU_BUTTON_X1(width), MENU_BUTTON_Y1(y1_offset), 
			MENU_BUTTON_X2(width), MENU_BUTTON_Y2(y1_offset),
			AL_BLACK, 1);
		}
		
		al_draw_text(font,
		AL_BLACK, width/2, MENU_BUTTON_Y1(y1_offset) + 12.5f, ALLEGRO_ALIGN_CENTRE,
		menu[i].title);
		y1_offset = MENU_BUTTON_Y2(y1_offset);
	}
}

static size_t get_menu_button_index_from_mouse_coordinates(ALLEGRO_DISPLAY *display,
														   size_t buttons_num,
														   int x, int y) {
	
	const size_t width = (size_t)al_get_display_width(display);
	if((size_t)x < MENU_BUTTON_X1(width) || (size_t)x > MENU_BUTTON_X2(width)) return (size_t)-1;
	const size_t height = (size_t)al_get_display_height(display);
	float y1_offset = Y1_OFFSET(height, buttons_num);
	
	for(size_t i = 0; i < buttons_num; i++) {
		if((size_t)y >= MENU_BUTTON_Y1(y1_offset) && (size_t)y <= MENU_BUTTON_Y2(y1_offset)) return i;
		y1_offset = MENU_BUTTON_Y2(y1_offset);
	}
	return (size_t)-1;
}

static void menu_reset(struct menu_button *menu, size_t buttons_num) {
	
	for(size_t i = 0; i < buttons_num; i++) {
		menu[i].hovered = false;
	}
}


static void main_menu_button_clicked(struct hexgame *game, size_t i) {
	
	if(i == 0) {
		game->scene = board_size_menu_scene;
		menu_reset(board_size_menu, BOARD_SIZE_MENU_BUTTON_NUM);
	}
	else if(i == 1) exit(0);
}

static void board_size_menu_button_clicked(struct hexgame *game, size_t i) {
	
	game->user_chosen_board_size = board_sizes[i];
	game->scene = grid_scene;
	HEXGAME_FLAG_ON(*game, reset);
}

static void menu_button_hovered(struct menu_button *menu, size_t buttons_num, size_t idx) {
	
	for(size_t i = 0; i < buttons_num; i++) {
		if(i == idx) {
			menu[i].hovered = true;
		}
		else {
			menu[i].hovered = false;
		}
	}
}

static void hexgame_init(struct hexgame *game) {
	
	memset(game, 0, sizeof(struct hexgame));
	HEXGAME_FLAG_ON(*game, redraw);
	HEXGAME_FLAG_ON(*game, reset);
	game->scene = main_menu_scene;
	game->user_chosen_board_size = 11;
}


int main(void) {
	
	al_init();
	al_init_primitives_addon();
	al_install_keyboard();
	al_install_mouse();
	al_init_font_addon();
	al_init_ttf_addon();
	al_init_image_addon();
	
	ALLEGRO_DISPLAY* display = al_create_display(600, 500);
	ALLEGRO_TIMER* timer = al_create_timer(1.0 / 30.0);
	ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
	ALLEGRO_FONT *font = al_load_ttf_font("VCR_OSD_MONO_1.001.ttf", 16, 0);
	if(!font) {
		font = al_create_builtin_font();
	}
	ALLEGRO_FONT *font_big = al_load_ttf_font("VCR_OSD_MONO_1.001.ttf", 32, 0);
	if(!font_big) {
		font_big = font;
	}
	ALLEGRO_BITMAP *icon = al_load_bitmap("icon.png");
	if(icon) {
		al_set_display_icon(display, icon);
	}
	
	
	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_register_event_source(queue, al_get_mouse_event_source());
	ALLEGRO_EVENT event;
	
	struct hexgame game;
	hexgame_init(&game);
	
	
	al_start_timer(timer);
	
	while (1)
	{
		al_wait_for_event(queue, &event);

		if(event.type == ALLEGRO_EVENT_TIMER)
		{
			HEXGAME_FLAG_ON(game, redraw);
		}
		else if((event.type == ALLEGRO_EVENT_KEY_DOWN && 
				event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) || 
				(event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)) 
		{
			break;
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_F)
		{
			HEXGAME_FLIP_FLAG(game, fullscreen);
			al_set_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, game.fullscreen);
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_R && 
				(game.scene == grid_scene || game.scene == result_scene))
		{
			HEXGAME_FLAG_ON(game, reset);
			game.scene = grid_scene;
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_M)
		{
			game.scene = main_menu_scene;
			menu_reset(main_menu, MAIN_MENU_BUTTON_NUM);
			
		}
		else if(event.type == ALLEGRO_EVENT_MOUSE_AXES) {
			
			if(game.scene == main_menu_scene) {
				
				size_t i = get_menu_button_index_from_mouse_coordinates(display,
						  MAIN_MENU_BUTTON_NUM,
						  event.mouse.x, event.mouse.y);
				if(i != (size_t)-1) {	
					menu_button_hovered(main_menu, MAIN_MENU_BUTTON_NUM, i);
					game.hovered_button = i;
				}
				else {
					main_menu[game.hovered_button].hovered = false;
				}
			}
			else if(game.scene == board_size_menu_scene) {
				
				size_t i = get_menu_button_index_from_mouse_coordinates(display,
						  BOARD_SIZE_MENU_BUTTON_NUM,
						  event.mouse.x, event.mouse.y);
				if(i != (size_t)-1) {
					menu_button_hovered(board_size_menu, BOARD_SIZE_MENU_BUTTON_NUM, i);
					game.hovered_button = i;
				}
				else {
					board_size_menu[game.hovered_button].hovered = false;
				}
			}
			else if(game.scene == grid_scene) {
				
				size_t i = get_cell_index_from_mouse_coordinates(display, &def_grid, 
						event.mouse.x, event.mouse.y);			
				if(i != (size_t)-1) {
					if(game.hovered_cell != (size_t)-1) {
						def_grid.cells[game.hovered_cell].hovered = false;
					}
					def_grid.cells[i].hovered = true;
					game.hovered_cell = i;
				}
				else {
					def_grid.cells[game.hovered_cell].hovered = false;
					game.hovered_cell = (size_t)-1;
				}
			}
		}
		
		else if(event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN &&
				event.mouse.button == 1)
		{
			if(game.scene == main_menu_scene) {	
				size_t i = get_menu_button_index_from_mouse_coordinates(display,
						  MAIN_MENU_BUTTON_NUM,
						  event.mouse.x, event.mouse.y);
				if(i != (size_t)-1) {	
					main_menu_button_clicked(&game, i);
				}
			}
			else if(game.scene == board_size_menu_scene) {
				size_t i = get_menu_button_index_from_mouse_coordinates(display,
						  BOARD_SIZE_MENU_BUTTON_NUM,
						  event.mouse.x, event.mouse.y);
				if(i != (size_t)-1) {
					board_size_menu_button_clicked(&game, i);
				}
			}
			else if(game.scene == grid_scene) {
				size_t i = get_cell_index_from_mouse_coordinates(display, &def_grid, 
						   event.mouse.x, event.mouse.y);			
				if(i != (size_t)-1) {
					open_cell(&game, &def_grid, i);
					game.winner = get_winner(&def_grid);
					if(game.winner != NEUTRAL) {
						game.scene = result_scene;
					}
				}
			}
		}

		if(game.redraw && al_is_event_queue_empty(queue))
		{
			if (game.reset) {
				def_grid.size = game.user_chosen_board_size;
				hex_def_grid_init();
				game.winner = NEUTRAL;
				game.current_player = HEXGAME_FIRST_PLAYER;
				HEXGAME_FLAG_OFF(game, reset);
				game.hovered_cell = (size_t)-1;
			}
			
			al_clear_to_color(AL_WHITE);
			
			if(game.scene == main_menu_scene) {
				menu_show(display, font_big, main_menu, MAIN_MENU_BUTTON_NUM);
			}
			else if(game.scene == board_size_menu_scene) {
				menu_show(display, font_big, board_size_menu, BOARD_SIZE_MENU_BUTTON_NUM);
			}
			else {
				hex_grid_draw(&game, display, &def_grid);
				if(game.scene == result_scene) {
					show_winner(&game, display, font, font_big);
				}
			}
			
			al_flip_display();
			HEXGAME_FLAG_OFF(game, redraw);
		}
	}
	
	return 0;
}
