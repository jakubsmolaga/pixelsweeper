/* --------------------------------- types ---------------------------------- */

typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int i32;
typedef float f32;
typedef _Bool bool;
typedef struct { f32 x, y; } Vec2;
typedef struct { f32 r, g, b, a; } Color;
typedef struct { Vec2 pos, uv; Color color; } Vertex;

typedef u8 CellFlags;
enum {
	CELLFLAG_IS_BOMB      = (1 << 0),
	CELLFLAG_IS_MARKED    = (1 << 1),
	CELLFLAG_IS_UNCOVERED = (1 << 2),
};

typedef u8 DrawFlags;
enum {
	DRAWFLAG_GRASS = (1 << 0),
	DRAWFLAG_DIRT  = (1 << 1),
	DRAWFLAG_DIGIT = (1 << 2),
	DRAWFLAG_BOMB  = (1 << 3),
	DRAWFLAG_MARK  = (1 << 4),
};


typedef u8 CellState;
enum {
	CELLSTATE_UNOPENED,
	CELLSTATE_OPENED,
	CELLSTATE_MARKED,
};

typedef struct Cell Cell;
struct Cell {
	CellState state;
	bool has_bomb;
};

typedef struct Particle Particle;
struct Particle {
	Vec2 pos, vel;
	Color color;
	bool removed;
};

/* ------------------------------- constants ------------------------------- */

#define false (0)
#define true (1)

#define CELL_W (7)
#define CELL_H (7)
#define BOARD_W (20)
#define BOARD_H (20)
#define SCREEN_W (BOARD_W * CELL_W)
#define SCREEN_H (BOARD_H * CELL_H)

#define rgb(r, g, b) ((Color){ r/255.0, g/255.0, b/255.0, 1.0 })
static const Color RED   = rgb(255, 0, 0);
static const Color GREEN = rgb(0, 255, 0);
static const Color BLUE  = rgb(0, 0, 255);
static const Color COLORS_GRASS[] = {
	[0] = {0.3, 0.9, 0.3, 1.0}, 
	[1] = {0.5, 1.0, 0.5, 1.0},
};
static const Color COLORS_DIRT[] = {
	[0] = {0.1, 0.1, 0.1, 1.0}, 
	[1] = {0.3, 0.3, 0.3, 1.0},
};
static const Color COLORS_DIGITS[] = {
	[0] = {1,1,1,1},
	[1] = {1,1,1,1},
	[2] = {1,1,1,1},
	[3] = {1,1,1,1},
	[4] = {1,1,1,1},
	[5] = {1,1,1,1},
	[6] = {1,1,1,1},
	[7] = {1,1,1,1},
	[8] = {1,1,1,1},
	[9] = {0.9,0.3,0.9,1},
};
#undef rgb

/* -------------------------------- globals --------------------------------- */

Vertex vertices[64 * 1024];
static i32 vertices_count = 0;

static Cell board[BOARD_H][BOARD_W];

static Particle particles[8 * 1024];
static i32 particles_len = 0;
static i32 particles_slots[8 * 1024];
static i32 particles_slots_len = 0;
static i32 rand_seed;

/* -------------------------------- gameplay -------------------------------- */

__attribute__((noreturn))
static void
abort(void)
{
	__builtin_unreachable();
}

static void
assert(bool cond)
{
	if (!cond) {
		abort();
	}
}

typedef struct NeighborIter NeighborIter;
struct NeighborIter {
	u8 x, y;
	u8 _cx, _cy, _i;
};

static bool
is_valid_coord(i32 cx, i32 cy)
{
	return (cx>=0 && cy>=0 && cx<BOARD_W && cy<BOARD_H);
}

static bool
neighboriter_next(NeighborIter *it)
{
	static const i32 offsets[8][2] = {
		{-1,-1}, { 0,-1}, { 1,-1},
		{-1, 0},          { 1, 0},
		{-1, 1}, { 0, 1}, { 1, 1},
	};
	while (it->_i < 8) {
		i32 nx = it->_cx + offsets[it->_i][0];
		i32 ny = it->_cy + offsets[it->_i][1];
		it->_i++;
		if (is_valid_coord(nx, ny)) {
			it->x = nx;
			it->y = ny;
			return true;
		}
	}
	return false;
}

static void
neighboriter_reset(NeighborIter *it)
{
	it->_i = 0;
}

static NeighborIter
cell_neighbors(i32 cx, i32 cy)
{
	return (NeighborIter){ ._cx=cx, ._cy=cy };
}

/* ------------------------------- rendering -------------------------------- */

static void
push_vert(f32 x, f32 y, f32 u, f32 v, Color color)
{
	assert(vertices_count < (sizeof(vertices)/sizeof(vertices[0])));
	Vec2 pos = { x, y };
	Vec2 uv = { u, v };
	Vertex vert = { pos, uv, color };
	vertices[vertices_count++] = vert;
}

static void
draw_rect(f32 x, f32 y, f32 w, f32 h, Color color)
{
	f32 x0 = x;
	f32 y0 = y;
	f32 x1 = x+w;
	f32 y1 = y+h;
	push_vert(x0, y0, 0.4, 0, color);
	push_vert(x0, y1, 0.4, 0, color);
	push_vert(x1, y1, 0.4, 0, color);
	push_vert(x0, y0, 0.4, 0, color);
	push_vert(x1, y0, 0.4, 0, color);
	push_vert(x1, y1, 0.4, 0, color);
}

static u8
rand_byte(void)
{
	rand_seed = (rand_seed*13 + 17) % 269;
	return rand_seed;
}

static void
particle_spawn(f32 x, f32 y, Color color)
{
	f32 vx = (rand_byte() / 255.0 - 0.5) * 2.0;
	f32 vy = (rand_byte() / 255.0) * -2.0;
	Particle p = {
		.pos = { x, y },
		.vel = { vx, vy },
		.color = color,
		.removed = false,
	};
	if (particles_slots_len > 0) {
		i32 slot = particles_slots[--particles_slots_len];
		particles[slot] = p;
	} else {
		particles[particles_len++] = p;
	}
}

static void
particles_update_and_draw(void)
{
	for (i32 i=0; i<particles_len; i++) {
		Particle *p = &particles[i];
		if (p->removed) {
			continue;
		}
		p->pos.x += p->vel.x;
		p->pos.y += p->vel.y;
		p->vel.y += 0.1;
		if (p->pos.y > SCREEN_H + 10) {
			particles_slots[particles_slots_len++] = i;
			p->removed = true;
		} else {
			draw_rect(p->pos.x, p->pos.y, 2, 2, p->color);
		}
	}
}

static void
place_bombs(u8 n)
{
	for (i32 i=0; i<n; i++) {
		u8 cx, cy;
		do {
			cx = rand_byte() % BOARD_W;
			cy = rand_byte() % BOARD_H;
		} while (board[cy][cx].has_bomb);
		board[cy][cx].has_bomb = true;
	}
}

static void
draw_digit(f32 cx, f32 cy, u8 digit)
{
	f32 w = 3;
	f32 h = 5;
	f32 x = (cx*7) + (7-w)/2;
	f32 y = (cy*7) + (7-h)/2;
	f32 x0 = x;
	f32 y0 = y;
	f32 x1 = x+w;
	f32 y1 = y+h;
	Color color = COLORS_DIGITS[digit];
	f32 u = 0.1 * digit;
	f32 v = 0;
	f32 u0 = u;
	f32 v0 = v;
	f32 u1 = u+0.1;
	f32 v1 = v+1.0;
	push_vert(x0, y0, u0, v0, color);
	push_vert(x0, y1, u0, v1, color);
	push_vert(x1, y1, u1, v1, color);
	push_vert(x0, y0, u0, v0, color);
	push_vert(x1, y0, u1, v0, color);
	push_vert(x1, y1, u1, v1, color);
}

static void
draw_grid(void)
{
	Color color0 = {0.3, 0.9, 0.3, 1.0};
	Color color1 = {0.5, 1.0, 0.5, 1.0};
	for (int cx=0; cx<20; cx++) {
		for (int cy=0; cy<20; cy++) {
			f32 x = cx * 7;
			f32 y = cy * 7;
			Color color = (cx+cy)%2 == 0 ? color0 : color1;
			draw_rect(x, y, 7, 7, color);
		}
	}
}

static i32
count_surrounding_bombs(i32 cx, i32 cy)
{
	i32 bombs = 0;
	NeighborIter it = cell_neighbors(cx, cy);
	while (neighboriter_next(&it)) {
		if (board[it.y][it.x].has_bomb)
			bombs++;
	}
	return bombs;
}

static DrawFlags
get_draw_flags(Cell cell)
{
	if (cell.state == CELLSTATE_UNOPENED)
		return DRAWFLAG_GRASS;
	if (cell.state == CELLSTATE_MARKED)
		return DRAWFLAG_GRASS | DRAWFLAG_MARK;
	if (cell.has_bomb)
		return DRAWFLAG_DIRT | DRAWFLAG_BOMB;
	return DRAWFLAG_DIRT | DRAWFLAG_DIGIT;
}

static void
draw_cell(i32 cx, i32 cy)
{
	DrawFlags f = get_draw_flags(board[cy][cx]);
	i32 parity = (cx+cy)%2;
	i32 x = cx*7, y = cy*7;
	if (f & DRAWFLAG_GRASS)
		draw_rect(x, y, 7, 7, COLORS_GRASS[parity]);
	if (f & DRAWFLAG_DIRT)
		draw_rect(x, y, 7, 7, COLORS_DIRT[parity]);
	if (f & DRAWFLAG_DIGIT)
		draw_digit(cx, cy, count_surrounding_bombs(cx, cy));
	if (f & DRAWFLAG_BOMB)
		draw_rect(x+1, y+1, 5, 5, RED);
	if (f & DRAWFLAG_MARK)
		draw_digit(cx, cy, 9);
}

static void
uncover(i32 cx, i32 cy)
{
	if (board[cy][cx].state == CELLSTATE_OPENED)
		return;
	board[cy][cx].state = CELLSTATE_OPENED;
	for (i32 i=0; i<10; i++)
		particle_spawn(cx*CELL_W, cy*CELL_H, (Color){1,1,1,1});
	if (count_surrounding_bombs(cx, cy) > 0)
		return;
	NeighborIter it = cell_neighbors(cx, cy);
	while (neighboriter_next(&it)) {
		if (!board[it.y][it.x].has_bomb)
			uncover(it.x, it.y);
	}
}

static CellState
cellstate_after_mark(CellState state)
{
	switch (state) {
	case CELLSTATE_UNOPENED: return CELLSTATE_MARKED;
	case CELLSTATE_OPENED:   return CELLSTATE_OPENED;
	case CELLSTATE_MARKED:   return CELLSTATE_UNOPENED;
	default: 		 abort();
	}
}

static void
chord(u8 cx, u8 cy)
{
	i32 marks = 0;
	NeighborIter it = cell_neighbors(cx, cy);
	while (neighboriter_next(&it)) {
		if (board[it.y][it.x].state == CELLSTATE_MARKED)
			marks++;
	}
	if (marks != count_surrounding_bombs(cx, cy))
		return;
	neighboriter_reset(&it);
	while (neighboriter_next(&it)) {
		if (board[it.y][it.x].state == CELLSTATE_UNOPENED)
			uncover(it.x, it.y);
	}
}

static void
handle_lmb(u8 cx, u8 cy)
{
	if (board[cy][cx].state == CELLSTATE_OPENED)
		chord(cx, cy);
	else
		uncover(cx, cy);
}

static void
handle_rmb(u8 cx, u8 cy)
{
	board[cy][cx].state = cellstate_after_mark(board[cy][cx].state);
}

/* ------------------------------- public api ------------------------------- */

void
on_mouse_click(f32 x, f32 y, bool rmb)
{
	u8 cx = (u8)(x/7.0);
	u8 cy = (u8)(y/7.0);
	if (rmb) handle_rmb(cx, cy);
	else     handle_lmb(cx, cy);
}

void
initialize(i32 seed)
{
	rand_seed = seed;
	for (i32 cy=0; cy<BOARD_H; cy++) {
		for (i32 cx=0; cx<BOARD_W; cx++) {
			board[cy][cx] = (Cell){0};
		}
	}
	place_bombs(40);
}

i32
next_frame(f32 timestamp)
{
	vertices_count = 0;
	for (i32 cy=0; cy<BOARD_H; cy++) {
		for (i32 cx=0; cx<BOARD_W; cx++) {
			draw_cell(cx, cy);
		}
	}
	particles_update_and_draw();

	return vertices_count;
}
