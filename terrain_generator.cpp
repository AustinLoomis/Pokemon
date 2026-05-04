#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <climits>
#include <ncurses.h>
#include <string>
#include <stdarg.h>
#include <vector>
#include "pokedex.hpp"
#include "pokemon_gen.hpp"
#include "battle.hpp"

/* ─── Dimensions ──────────────────────────────────────────────────────────── */
#define MAP_WIDTH   80
#define MAP_HEIGHT  21
#define WORLD_SIZE  401
#define WORLD_CTR   200

/* ncurses row layout */
#define MSG_ROW     0
#define MAP_ROW_OFF 1

/* ─── Terrain types ───────────────────────────────────────────────────────── */
enum terrain_type_t {
    TERRAIN_BOULDER,
    TERRAIN_TREE,
    TERRAIN_PATH,
    TERRAIN_POKECENTER,
    TERRAIN_POKEMART,
    TERRAIN_GRASS,
    TERRAIN_CLEARING,
    TERRAIN_WATER,
    TERRAIN_MOUNTAIN,
    TERRAIN_FOREST,
    NUM_TERRAIN_TYPES
};

/* ─── Trainer types ───────────────────────────────────────────────────────── */
enum trainer_type_t {
    TRAINER_PC,
    TRAINER_HIKER,
    TRAINER_RIVAL
};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Class hierarchy: character <- pc, npc                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

class character {
public:
    int x, y;
    trainer_type_t type;

    character() : x(0), y(0), type(TRAINER_PC) {}
    virtual ~character() {}
};

class pc : public character {
public:
    std::vector<GamePokemon> party;
    pc() { type = TRAINER_PC; }
};

class npc : public character {
public:
    int active;       /* 1 = present on this map */
    int defeated;     /* 1 = battle lost, stops pathing */
    int speed;        /* move every N turns */
    int next_move;    /* countdown to next move */
    int wander_dx, wander_dy;
    std::vector<GamePokemon> party;

    npc() : active(0), defeated(0), speed(1), next_move(1),
            wander_dx(0), wander_dy(0) {
        type = TRAINER_HIKER;
    }
};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Map class                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */
#define MAX_NPCS 10

class map_t {
public:
    char         cells[MAP_HEIGHT][MAP_WIDTH];
    terrain_type_t terrain[MAP_HEIGHT][MAP_WIDTH];
    int north_gate, south_gate, east_gate, west_gate;
    pc           player;
    npc          npcs[MAX_NPCS];
    int          num_npcs;

    map_t() : north_gate(0), south_gate(0), east_gate(0), west_gate(0),
              num_npcs(0) {}
};

/* ─── Priority queue for Dijkstra ─────────────────────────────────────────── */
struct heap_node_t { int x, y, dist; };

struct heap_t {
    heap_node_t *nodes;
    int size, capacity;
};

/* ─── World grid ──────────────────────────────────────────────────────────── */
static map_t *world[WORLD_SIZE][WORLD_SIZE];
static int cur_ix = WORLD_CTR;
static int cur_iy = WORLD_CTR;
static int turn   = 0;
static char msg_buf[256] = "";
/* Pointer to active PC party for wild encounters */
static std::vector<GamePokemon> *m_player_party_ptr = nullptr;

/* ─── Helpers ─────────────────────────────────────────────────────────────── */
static inline int logical_x(int ix) { return ix - WORLD_CTR; }
static inline int logical_y(int iy) { return iy - WORLD_CTR; }
static bool is_valid_pos(int x, int y) { return x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT; }
static int manhattan(int ix, int iy) { return abs(ix - WORLD_CTR) + abs(iy - WORLD_CTR); }

static int building_prob(int ix, int iy)
{
    int d = manhattan(ix, iy);
    if (d == 0)   return 100;
    if (d >= 200) return 5;
    int p = -45 * d / 200 + 50;
    return (p < 5) ? 5 : p;
}

struct seed_t { int x, y; terrain_type_t type; };

/* ─── Forward declarations ──────────────────────────────────────────────── */
static map_t *generate_map(int ix, int iy, int fn, int fs, int fe, int fw);
static map_t *get_or_generate(int ix, int iy);
static void   render_map(map_t *m, int ix, int iy);
static void   dijkstra(map_t *m, trainer_type_t trainer, int dist_map[MAP_HEIGHT][MAP_WIDTH]);
static void   move_npcs(map_t *m);
static void   game_loop();
static void   set_msg(const char *fmt, ...);
static void   show_msg();

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Color pairs                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */
#define CP_DEFAULT   1
#define CP_BOULDER   2
#define CP_TREE      3
#define CP_PATH      4
#define CP_CENTER    5
#define CP_MART      6
#define CP_GRASS     7
#define CP_WATER     8
#define CP_PC        9
#define CP_NPC_HIKER 10
#define CP_NPC_RIVAL 11
#define CP_MSG       12

static void init_colors()
{
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT,   COLOR_WHITE,   -1);
    init_pair(CP_BOULDER,   COLOR_WHITE,   -1);
    init_pair(CP_TREE,      COLOR_GREEN,   -1);
    init_pair(CP_PATH,      COLOR_YELLOW,  -1);
    init_pair(CP_CENTER,    COLOR_CYAN,    -1);
    init_pair(CP_MART,      COLOR_MAGENTA, -1);
    init_pair(CP_GRASS,     COLOR_GREEN,   -1);
    init_pair(CP_WATER,     COLOR_BLUE,    -1);
    init_pair(CP_PC,        COLOR_WHITE,   COLOR_RED);
    init_pair(CP_NPC_HIKER, COLOR_YELLOW,  -1);
    init_pair(CP_NPC_RIVAL, COLOR_MAGENTA, -1);
    init_pair(CP_MSG,       COLOR_WHITE,   COLOR_BLUE);
}

static int terrain_color(terrain_type_t t)
{
    switch (t) {
        case TERRAIN_BOULDER:    return CP_BOULDER;
        case TERRAIN_TREE:       return CP_TREE;
        case TERRAIN_PATH:       return CP_PATH;
        case TERRAIN_POKECENTER: return CP_CENTER;
        case TERRAIN_POKEMART:   return CP_MART;
        case TERRAIN_GRASS:      return CP_GRASS;
        case TERRAIN_WATER:      return CP_WATER;
        case TERRAIN_MOUNTAIN:   return CP_BOULDER;
        case TERRAIN_FOREST:     return CP_TREE;
        default:                 return CP_DEFAULT;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Movement cost table                                                        */
/* ═══════════════════════════════════════════════════════════════════════════ */
static int get_move_cost(trainer_type_t trainer, terrain_type_t terrain)
{
    switch (trainer) {
        case TRAINER_PC:
            switch (terrain) {
                case TERRAIN_PATH:       return 10;
                case TERRAIN_POKEMART:   return 10;
                case TERRAIN_POKECENTER: return 10;
                case TERRAIN_GRASS:      return 20;
                case TERRAIN_CLEARING:   return 10;
                default:                 return INT_MAX;
            }
        case TRAINER_HIKER:
            switch (terrain) {
                case TERRAIN_PATH:       return 10;
                case TERRAIN_POKEMART:   return 50;
                case TERRAIN_POKECENTER: return 50;
                case TERRAIN_GRASS:      return 15;
                case TERRAIN_CLEARING:   return 10;
                case TERRAIN_MOUNTAIN:   return 15;
                case TERRAIN_FOREST:     return 15;
                default:                 return INT_MAX;
            }
        case TRAINER_RIVAL:
            switch (terrain) {
                case TERRAIN_PATH:       return 10;
                case TERRAIN_POKEMART:   return 50;
                case TERRAIN_POKECENTER: return 50;
                case TERRAIN_GRASS:      return 20;
                case TERRAIN_CLEARING:   return 10;
                default:                 return INT_MAX;
            }
        default:
            return INT_MAX;
    }
}

/* Check if (x,y) is a gate cell */
static bool is_gate(map_t *m, int x, int y)
{
    if (y == 0            && x == m->north_gate) return true;
    if (y == MAP_HEIGHT-1 && x == m->south_gate) return true;
    if (x == 0            && y == m->west_gate)  return true;
    if (x == MAP_WIDTH-1  && y == m->east_gate)  return true;
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Priority Queue (Min Heap)                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */
static heap_t *heap_create(int capacity)
{
    heap_t *h = (heap_t *)malloc(sizeof(heap_t));
    h->nodes = (heap_node_t *)malloc(sizeof(heap_node_t) * capacity);
    h->size = 0; h->capacity = capacity;
    return h;
}
static void heap_destroy(heap_t *h) { free(h->nodes); free(h); }
static void heap_swap(heap_node_t *a, heap_node_t *b) { heap_node_t t = *a; *a = *b; *b = t; }

static void heap_push(heap_t *h, int x, int y, int dist)
{
    if (h->size >= h->capacity) return;
    int i = h->size++;
    h->nodes[i] = {x, y, dist};
    while (i > 0) {
        int p = (i-1)/2;
        if (h->nodes[i].dist >= h->nodes[p].dist) break;
        heap_swap(&h->nodes[i], &h->nodes[p]); i = p;
    }
}

static bool heap_pop(heap_t *h, int *x, int *y, int *dist)
{
    if (!h->size) return false;
    *x = h->nodes[0].x; *y = h->nodes[0].y; *dist = h->nodes[0].dist;
    h->nodes[0] = h->nodes[--h->size];
    int i = 0;
    while (true) {
        int s = i, l = 2*i+1, r = 2*i+2;
        if (l < h->size && h->nodes[l].dist < h->nodes[s].dist) s = l;
        if (r < h->size && h->nodes[r].dist < h->nodes[s].dist) s = r;
        if (s == i) break;
        heap_swap(&h->nodes[i], &h->nodes[s]); i = s;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Dijkstra's Algorithm                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void dijkstra(map_t *m, trainer_type_t trainer, int dist_map[MAP_HEIGHT][MAP_WIDTH])
{
    heap_t *heap = heap_create(MAP_WIDTH * MAP_HEIGHT);
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            dist_map[y][x] = INT_MAX;

    dist_map[m->player.y][m->player.x] = 0;
    heap_push(heap, m->player.x, m->player.y, 0);

    const int dx[] = {0,1,1,1,0,-1,-1,-1};
    const int dy[] = {-1,-1,0,1,1,1,0,-1};

    while (true) {
        int x, y, dist;
        if (!heap_pop(heap, &x, &y, &dist)) break;
        if (dist > dist_map[y][x]) continue;
        for (int i = 0; i < 8; i++) {
            int nx = x+dx[i], ny = y+dy[i];
            if (!is_valid_pos(nx, ny)) continue;
            int cost = get_move_cost(trainer, m->terrain[ny][nx]);
            if (cost == INT_MAX) continue;
            int nd = dist + cost;
            if (nd < dist_map[ny][nx]) {
                dist_map[ny][nx] = nd;
                heap_push(heap, nx, ny, nd);
            }
        }
    }
    heap_destroy(heap);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Generation helpers                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */
static char terrain_char(terrain_type_t t)
{
    switch (t) {
        case TERRAIN_BOULDER:    return '%';
        case TERRAIN_TREE:       return '^';
        case TERRAIN_PATH:       return '#';
        case TERRAIN_POKECENTER: return 'C';
        case TERRAIN_POKEMART:   return 'M';
        case TERRAIN_GRASS:      return ':';
        case TERRAIN_CLEARING:   return '.';
        case TERRAIN_WATER:      return '~';
        case TERRAIN_MOUNTAIN:   return '%';
        case TERRAIN_FOREST:     return '^';
        default:                 return '.';
    }
}

static void init_cells(map_t *m)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++) {
            m->cells[y][x] = '.';
            m->terrain[y][x] = TERRAIN_CLEARING;
        }
}

static void set_terrain(map_t *m, int x, int y, terrain_type_t type)
{
    m->terrain[y][x] = type;
    m->cells[y][x] = terrain_char(type);
}

static void place_border(map_t *m, int ix, int iy)
{
    bool an = (iy == 0), as_ = (iy == WORLD_SIZE-1);
    bool aw = (ix == 0), ae  = (ix == WORLD_SIZE-1);

    for (int x = 0; x < MAP_WIDTH; x++) {
        if (an  || x != m->north_gate) set_terrain(m, x, 0, TERRAIN_BOULDER);
        if (as_ || x != m->south_gate) set_terrain(m, x, MAP_HEIGHT-1, TERRAIN_BOULDER);
    }
    for (int y = 0; y < MAP_HEIGHT; y++) {
        if (aw || y != m->west_gate) set_terrain(m, 0, y, TERRAIN_BOULDER);
        if (ae || y != m->east_gate) set_terrain(m, MAP_WIDTH-1, y, TERRAIN_BOULDER);
    }
}

static void grow_regions(map_t *m, seed_t *seeds, int ns)
{
    for (int i = 0; i < ns; i++)
        if (m->terrain[seeds[i].y][seeds[i].x] != TERRAIN_BOULDER)
            set_terrain(m, seeds[i].x, seeds[i].y, seeds[i].type);

    bool changed = true;
    int itr = 0;
    while (changed && itr < 150) {
        changed = false; itr++;
        for (int y = 1; y < MAP_HEIGHT-1; y++) {
            for (int x = 1; x < MAP_WIDTH-1; x++) {
                if (m->terrain[y][x] != TERRAIN_CLEARING) continue;
                terrain_type_t nbrs[8]; int cnt = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (!dx && !dy) continue;
                        terrain_type_t t = m->terrain[y+dy][x+dx];
                        if (t == TERRAIN_GRASS || t == TERRAIN_WATER ||
                            t == TERRAIN_CLEARING || t == TERRAIN_MOUNTAIN ||
                            t == TERRAIN_FOREST)
                            nbrs[cnt++] = t;
                    }
                if (cnt > 0 && rand() % 100 < 40) {
                    set_terrain(m, x, y, nbrs[rand()%cnt]);
                    changed = true;
                }
            }
        }
    }
}

static void generate_paths(map_t *m)
{
    int cx = m->north_gate, tx = m->south_gate;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        set_terrain(m, cx, y, TERRAIN_PATH);
        if (y < MAP_HEIGHT-1 && cx != tx) {
            int rows_left = (MAP_HEIGHT-1) - y;
            int dist = abs(cx-tx);
            bool must = (dist >= rows_left);
            bool want = (rand()%3 == 0);
            if (must || want) { if (cx < tx) cx++; else cx--; }
        }
    }
    int cy = m->west_gate, ty = m->east_gate;
    for (int x = 0; x < MAP_WIDTH; x++) {
        set_terrain(m, x, cy, TERRAIN_PATH);
        if (x < MAP_WIDTH-1 && cy != ty) {
            int cols_left = (MAP_WIDTH-1) - x;
            int dist = abs(cy-ty);
            bool must = (dist >= cols_left);
            bool want = (rand()%3 == 0);
            if (must || want) { if (cy < ty) cy++; else cy--; }
        }
    }
}

static void place_buildings(map_t *m, int ix, int iy)
{
    int prob = building_prob(ix, iy);
    bool place_center = (rand()%100 < prob);
    bool place_mart   = (rand()%100 < prob);
    if (ix == WORLD_CTR && iy == WORLD_CTR) { place_center = true; place_mart = true; }

    bool near[MAP_HEIGHT][MAP_WIDTH] = {};
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            if (m->terrain[y][x] == TERRAIN_PATH)
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y+dy, nx = x+dx;
                        if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT)
                            near[ny][nx] = true;
                    }

    int tries = 0;
    if (place_center) {
        while (tries++ < 500) {
            int x = 1+rand()%(MAP_WIDTH-3), y = 1+rand()%(MAP_HEIGHT-3);
            if (near[y][x] &&
                m->terrain[y][x]   != TERRAIN_PATH && m->terrain[y][x+1]   != TERRAIN_PATH &&
                m->terrain[y+1][x] != TERRAIN_PATH && m->terrain[y+1][x+1] != TERRAIN_PATH) {
                set_terrain(m, x,   y,   TERRAIN_POKECENTER);
                set_terrain(m, x+1, y,   TERRAIN_POKECENTER);
                set_terrain(m, x,   y+1, TERRAIN_POKECENTER);
                set_terrain(m, x+1, y+1, TERRAIN_POKECENTER);
                break;
            }
        }
    }
    tries = 0;
    if (place_mart) {
        while (tries++ < 500) {
            int x = 1+rand()%(MAP_WIDTH-3), y = 1+rand()%(MAP_HEIGHT-3);
            if (near[y][x] &&
                m->terrain[y][x]   != TERRAIN_PATH && m->terrain[y][x+1]   != TERRAIN_PATH &&
                m->terrain[y+1][x] != TERRAIN_PATH && m->terrain[y+1][x+1] != TERRAIN_PATH &&
                m->terrain[y][x]   != TERRAIN_POKECENTER && m->terrain[y][x+1]   != TERRAIN_POKECENTER &&
                m->terrain[y+1][x] != TERRAIN_POKECENTER && m->terrain[y+1][x+1] != TERRAIN_POKECENTER) {
                set_terrain(m, x,   y,   TERRAIN_POKEMART);
                set_terrain(m, x+1, y,   TERRAIN_POKEMART);
                set_terrain(m, x,   y+1, TERRAIN_POKEMART);
                set_terrain(m, x+1, y+1, TERRAIN_POKEMART);
                break;
            }
        }
    }
}

static void add_decorations(map_t *m)
{
    int n = 30 + rand()%30;
    for (int i = 0; i < n; i++) {
        int x = 1+rand()%(MAP_WIDTH-2), y = 1+rand()%(MAP_HEIGHT-2);
        terrain_type_t t = m->terrain[y][x];
        if (t == TERRAIN_CLEARING || t == TERRAIN_GRASS)
            set_terrain(m, x, y, (rand()%2) ? TERRAIN_TREE : TERRAIN_BOULDER);
    }
}

static int npc_at(map_t *m, int x, int y)
{
    for (int i = 0; i < m->num_npcs; i++)
        if (m->npcs[i].active && m->npcs[i].x == x && m->npcs[i].y == y)
            return i;
    return -1;
}

static void place_npcs(map_t *m, int ix, int iy)
{
    m->num_npcs = 2 + rand()%(MAX_NPCS-1);
    for (int i = 0; i < m->num_npcs; i++) {
        npc &n = m->npcs[i];
        n.type = (i % 2 == 0) ? TRAINER_HIKER : TRAINER_RIVAL;
        n.active = 1; n.defeated = 0;
        n.speed = 1 + rand()%3;
        n.next_move = n.speed;
        n.wander_dx = (rand()%3)-1;
        n.wander_dy = (rand()%3)-1;

        /* Generate trainer party */
        if (g_db.loaded)
            n.party = generate_party(ix, iy);

        int tries = 0;
        while (tries++ < 1000) {
            int x = 1+rand()%(MAP_WIDTH-2), y = 1+rand()%(MAP_HEIGHT-2);
            if (is_gate(m, x, y)) continue;
            if (get_move_cost(n.type, m->terrain[y][x]) == INT_MAX) continue;
            if (x == m->player.x && y == m->player.y) continue;
            if (npc_at(m, x, y) != -1) continue;
            n.x = x; n.y = y;
            break;
        }
    }
}

static void place_pc_on_road(map_t *m)
{
    for (int tries = 0; tries < 1000; tries++) {
        int x = 1+rand()%(MAP_WIDTH-2), y = 1+rand()%(MAP_HEIGHT-2);
        if (m->terrain[y][x] == TERRAIN_PATH && !is_gate(m, x, y)) {
            m->player.x = x; m->player.y = y;
            return;
        }
    }
    m->player.x = MAP_WIDTH/2; m->player.y = MAP_HEIGHT/2;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Core generator                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */
static map_t *generate_map(int ix, int iy, int fn, int fs, int fe, int fw)
{
    map_t *m = new map_t();

    m->north_gate = (fn >= 0) ? fn : 30+rand()%(MAP_WIDTH-60);
    m->south_gate = (fs >= 0) ? fs : 30+rand()%(MAP_WIDTH-60);
    m->west_gate  = (fw >= 0) ? fw : 3+rand()%(MAP_HEIGHT-6);
    m->east_gate  = (fe >= 0) ? fe : 3+rand()%(MAP_HEIGHT-6);

    init_cells(m);

    seed_t seeds[20]; int ns = 0;
    for (int i = 0; i < 3; i++) seeds[ns++] = {10+rand()%60, 3+rand()%15, TERRAIN_GRASS};
    for (int i = 0; i < 2; i++) seeds[ns++] = {10+rand()%60, 3+rand()%15, TERRAIN_WATER};
    for (int i = 0; i < 2; i++) seeds[ns++] = {10+rand()%60, 3+rand()%15, TERRAIN_CLEARING};
    seeds[ns++] = {10+rand()%60, 3+rand()%15, TERRAIN_MOUNTAIN};
    seeds[ns++] = {10+rand()%60, 3+rand()%15, TERRAIN_FOREST};

    grow_regions(m, seeds, ns);
    generate_paths(m);
    place_border(m, ix, iy);
    place_buildings(m, ix, iy);
    add_decorations(m);
    place_pc_on_road(m);
    place_npcs(m, ix, iy);

    return m;
}

static map_t *get_or_generate(int ix, int iy)
{
    if (world[iy][ix]) return world[iy][ix];

    int fn = -1, fs = -1, fe = -1, fw = -1;
    if (iy > 0            && world[iy-1][ix]) fn = world[iy-1][ix]->south_gate;
    if (iy < WORLD_SIZE-1 && world[iy+1][ix]) fs = world[iy+1][ix]->north_gate;
    if (ix > 0            && world[iy][ix-1]) fw = world[iy][ix-1]->east_gate;
    if (ix < WORLD_SIZE-1 && world[iy][ix+1]) fe = world[iy][ix+1]->west_gate;

    world[iy][ix] = generate_map(ix, iy, fn, fs, fe, fw);
    return world[iy][ix];
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Message helpers                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void set_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, ap);
    va_end(ap);
}

static void show_msg()
{
    attron(COLOR_PAIR(CP_MSG));
    mvhline(MSG_ROW, 0, ' ', MAP_WIDTH);
    mvprintw(MSG_ROW, 0, "%.*s", MAP_WIDTH-1, msg_buf);
    attroff(COLOR_PAIR(CP_MSG));
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Render                                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void render_map(map_t *m, int ix, int iy)
{
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int pair = terrain_color(m->terrain[y][x]);
            attron(COLOR_PAIR(pair));
            mvaddch(y+MAP_ROW_OFF, x, m->cells[y][x]);
            attroff(COLOR_PAIR(pair));
        }
    }

    for (int i = 0; i < m->num_npcs; i++) {
        npc &n = m->npcs[i];
        if (!n.active) continue;
        int pair = (n.type == TRAINER_HIKER) ? CP_NPC_HIKER : CP_NPC_RIVAL;
        char sym = (n.type == TRAINER_HIKER) ? 'h' : 'r';
        attron(COLOR_PAIR(pair) | A_BOLD);
        mvaddch(n.y+MAP_ROW_OFF, n.x, sym);
        attroff(COLOR_PAIR(pair) | A_BOLD);
    }

    attron(COLOR_PAIR(CP_PC) | A_BOLD);
    mvaddch(m->player.y+MAP_ROW_OFF, m->player.x, '@');
    attroff(COLOR_PAIR(CP_PC) | A_BOLD);

    attron(COLOR_PAIR(CP_DEFAULT));
    mvprintw(22, 0, "Map(%d,%d) T:%d  Bag: %dP %dR %dPB  Party:%d  [move|f=fly|B=bag|t=list|Q]",
             logical_x(ix), logical_y(iy), turn,
             g_bag.potions, g_bag.revives, g_bag.pokeballs,
             (int)(m->player.party.size()));
    clrtoeol();
    attroff(COLOR_PAIR(CP_DEFAULT));

    show_msg();
    refresh();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Battle placeholder                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */
/* Print one pokemon's details to the ncurses screen starting at row r */
static int print_pokemon_details(const GamePokemon &gp, int row, int col)
{
    attron(COLOR_PAIR(CP_PATH) | A_BOLD);
    mvprintw(row++, col, "  %s  Lv.%d%s%s",
             gp.name.c_str(), gp.level,
             gp.is_shiny  ? " [SHINY]"  : "",
             gp.is_female ? " (F)" : " (M)");
    attroff(COLOR_PAIR(CP_PATH) | A_BOLD);
    mvprintw(row++, col, "  HP:%d/%d  Atk:%d  Def:%d  SpAtk:%d  SpDef:%d  Spd:%d",
             gp.hp_cur, gp.hp_max, gp.atk, gp.def, gp.spatk, gp.spdef, gp.spd);
    mvprintw(row++, col, "  IVs: %d/%d/%d/%d/%d/%d",
             gp.iv[0], gp.iv[1], gp.iv[2], gp.iv[3], gp.iv[4], gp.iv[5]);
    if (gp.num_moves == 0) {
        mvprintw(row++, col, "  Moves: (none)");
    } else {
        char mbuf[128] = "  Moves: ";
        for (int i = 0; i < gp.num_moves; i++) {
            if (i) strcat(mbuf, ", ");
            strcat(mbuf, gp.moves[i].name.c_str());
        }
        mvprintw(row++, col, "%s", mbuf);
    }
    return row;
}

static void do_battle(map_t *m, int npc_idx)
{
    npc &n = m->npcs[npc_idx];
    const char *tname = (n.type == TRAINER_HIKER) ? "Hiker" : "Rival";

    if (n.party.empty()) {
        set_msg("%s has no Pokemon and runs away!", tname);
        n.defeated = 1;
        return;
    }

    bool won = battle_trainer(m->player.party, n.party, tname, g_bag);
    n.defeated = 1;
    if (won)
        set_msg("You defeated %s!", tname);
    else
        set_msg("You lost to %s...", tname);
    clear();
}

/* Wild pokemon encounter: 10% chance when stepping on grass */
static void maybe_wild_encounter(int ix, int iy)
{
    if (!g_db.loaded) return;
    if (rand() % 10 != 0) return;

    if (m_player_party_ptr && m_player_party_ptr->empty()) {
        set_msg("No pokemon -- can't encounter wild pokemon!");
        return;
    }

    GamePokemon gp = generate_pokemon_for_map(ix, iy);

    set_msg("A wild %s (Lv.%d) appeared!", gp.name.c_str(), gp.level);

    /* We need access to the PC party -- passed via pointer set before call */
    battle_wild(*m_player_party_ptr, gp, g_bag);
    clear();
    set_msg("Back to the map.");
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Buildings                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void do_building(terrain_type_t type)
{
    clear();
    if (type == TERRAIN_POKECENTER) {
        attron(COLOR_PAIR(CP_CENTER) | A_BOLD);
        mvprintw(5, 28, "=== Pokemon Center ===");
        attroff(COLOR_PAIR(CP_CENTER) | A_BOLD);
        mvprintw(7, 20, "Nurse Joy heals all your Pokemon to full health!");
        /* Heal all party pokemon */
        if (m_player_party_ptr) {
            for (auto &gp : *m_player_party_ptr)
                gp.hp_cur = gp.hp_max;
        }
        mvprintw(9, 20, "Your Pokemon have been healed!");
    } else {
        attron(COLOR_PAIR(CP_MART) | A_BOLD);
        mvprintw(5, 30, "===  Poke Mart  ===");
        attroff(COLOR_PAIR(CP_MART) | A_BOLD);
        mvprintw(7, 20, "Restocking your supplies...");
        /* Restore to default supplies */
        g_bag.potions  = 5;
        g_bag.revives  = 3;
        g_bag.pokeballs= 10;
        mvprintw(9, 20, "Restocked: 5 Potions, 3 Revives, 10 Pokeballs.");
    }
    mvprintw(12, 28, "Press < to exit.");
    refresh();
    while (getch() != '<') {}
    clear();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Trainer list                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void show_trainer_list(map_t *m)
{
    int scroll = 0;
    const int display_rows = MAP_HEIGHT;
    int n = m->num_npcs;

    while (true) {
        clear();
        attron(COLOR_PAIR(CP_MSG) | A_BOLD);
        mvprintw(0, 0, "  Trainers on map  (arrows=scroll, ESC=return)  ");
        attroff(COLOR_PAIR(CP_MSG) | A_BOLD);

        for (int i = 0; i < display_rows && (scroll+i) < n; i++) {
            npc &np = m->npcs[scroll+i];
            char sym = (np.type == TRAINER_HIKER) ? 'h' : 'r';
            const char *tname = (np.type == TRAINER_HIKER) ? "Hiker" : "Rival";
            int rx = np.x - m->player.x;
            int ry = np.y - m->player.y;

            char ns_s[32]="", ew_s[32]="";
            if      (ry < 0) snprintf(ns_s, sizeof(ns_s), "%d north", -ry);
            else if (ry > 0) snprintf(ns_s, sizeof(ns_s), "%d south",  ry);
            if      (rx < 0) snprintf(ew_s, sizeof(ew_s), "%d west",  -rx);
            else if (rx > 0) snprintf(ew_s, sizeof(ew_s), "%d east",   rx);

            char pos[128];
            if      (!rx && !ry) snprintf(pos, sizeof(pos), "same cell");
            else if (!rx)        snprintf(pos, sizeof(pos), "%s", ns_s);
            else if (!ry)        snprintf(pos, sizeof(pos), "%s", ew_s);
            else                 snprintf(pos, sizeof(pos), "%s and %s", ns_s, ew_s);

            int pair = (np.type == TRAINER_HIKER) ? CP_NPC_HIKER : CP_NPC_RIVAL;
            attron(COLOR_PAIR(pair));
            mvprintw(i+1, 2, "%c  %s  %s%s", sym, tname, pos,
                     np.defeated ? " [defeated]" : "");
            attroff(COLOR_PAIR(pair));
        }
        refresh();

        int ch = getch();
        if (ch == 27) break;
        if (ch == KEY_UP   && scroll > 0) scroll--;
        if (ch == KEY_DOWN && scroll+display_rows < n) scroll++;
    }
    clear();
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  NPC movement                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */
static void move_npcs(map_t *m)
{
    int hiker_dist[MAP_HEIGHT][MAP_WIDTH];
    int rival_dist[MAP_HEIGHT][MAP_WIDTH];
    bool computed_hiker = false, computed_rival = false;

    const int dx8[] = {0,1,1,1,0,-1,-1,-1};
    const int dy8[] = {-1,-1,0,1,1,1,0,-1};

    for (int i = 0; i < m->num_npcs; i++) {
        npc &n = m->npcs[i];
        if (!n.active) continue;

        if (--n.next_move > 0) continue;
        n.next_move = n.speed;

        if (n.defeated) {
            int nx = n.x + n.wander_dx, ny = n.y + n.wander_dy;
            if (is_valid_pos(nx, ny) && !is_gate(m, nx, ny) &&
                get_move_cost(n.type, m->terrain[ny][nx]) != INT_MAX &&
                !(nx == m->player.x && ny == m->player.y) &&
                npc_at(m, nx, ny) == -1) {
                n.x = nx; n.y = ny;
            } else {
                n.wander_dx = (rand()%3)-1;
                n.wander_dy = (rand()%3)-1;
            }
            continue;
        }

        int (*dist)[MAP_WIDTH] = nullptr;
        if (n.type == TRAINER_HIKER) {
            if (!computed_hiker) { dijkstra(m, TRAINER_HIKER, hiker_dist); computed_hiker = true; }
            dist = hiker_dist;
        } else {
            if (!computed_rival) { dijkstra(m, TRAINER_RIVAL, rival_dist); computed_rival = true; }
            dist = rival_dist;
        }

        int best_x = n.x, best_y = n.y;
        int best_d = dist[n.y][n.x];

        for (int d = 0; d < 8; d++) {
            int nx = n.x+dx8[d], ny = n.y+dy8[d];
            if (!is_valid_pos(nx, ny)) continue;
            /* NPCs cannot enter gates */
            if (is_gate(m, nx, ny)) continue;
            if (nx == m->player.x && ny == m->player.y) {
                best_x = nx; best_y = ny; best_d = -1; break;
            }
            if (npc_at(m, nx, ny) != -1) continue;
            if (dist[ny][nx] < best_d) {
                best_d = dist[ny][nx];
                best_x = nx; best_y = ny;
            }
        }

        if (best_x == m->player.x && best_y == m->player.y) {
            do_battle(m, i);
        } else {
            n.x = best_x; n.y = best_y;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Fly command — ncurses coordinate input                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */
static map_t *do_fly(map_t *m)
{
    /* Prompt for X */
    attron(COLOR_PAIR(CP_MSG));
    mvhline(MSG_ROW, 0, ' ', MAP_WIDTH);
    mvprintw(MSG_ROW, 0, "Fly to X (range %d..%d): ", -WORLD_CTR, WORLD_CTR);
    attroff(COLOR_PAIR(CP_MSG));
    refresh();

    echo(); curs_set(1);
    char buf[32] = {};
    mvgetnstr(MSG_ROW, 24, buf, 8);
    noecho(); curs_set(0);

    int lx = atoi(buf);

    /* Prompt for Y */
    attron(COLOR_PAIR(CP_MSG));
    mvhline(MSG_ROW, 0, ' ', MAP_WIDTH);
    mvprintw(MSG_ROW, 0, "Fly to Y (range %d..%d): ", -WORLD_CTR, WORLD_CTR);
    attroff(COLOR_PAIR(CP_MSG));
    refresh();

    echo(); curs_set(1);
    memset(buf, 0, sizeof(buf));
    mvgetnstr(MSG_ROW, 24, buf, 8);
    noecho(); curs_set(0);

    int ly = atoi(buf);

    int nx = lx + WORLD_CTR;
    int ny = ly + WORLD_CTR;

    if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE) {
        set_msg("(%d,%d) is out of bounds (max ±%d).", lx, ly, WORLD_CTR);
        return m;
    }

    cur_ix = nx; cur_iy = ny;
    map_t *nm = get_or_generate(cur_ix, cur_iy);
    /* Place PC on a road in the new map */
    place_pc_on_road(nm);
    m_player_party_ptr = &nm->player.party;
    set_msg("Flew to (%d, %d).", lx, ly);
    return nm;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Attempt PC move — handles gate transitions                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */
static int attempt_move(map_t **mp, int dx, int dy)
{
    map_t *m = *mp;
    int nx = m->player.x + dx;
    int ny = m->player.y + dy;

    /* ── Gate transitions: PC steps INTO a gate cell → move to neighbor ── */
    if (is_valid_pos(nx, ny) && is_gate(m, nx, ny)) {
        /* North gate */
        if (ny == 0 && nx == m->north_gate) {
            if (cur_iy == 0) { set_msg("Can't go further north!"); return 0; }
            cur_iy--;
            map_t *nm = get_or_generate(cur_ix, cur_iy);
            nm->player.x = nm->south_gate;
            nm->player.y = MAP_HEIGHT - 2;
            *mp = nm;
            m_player_party_ptr = &nm->player.party;
            set_msg("Entered map to the north.");
            return 1;
        }
        /* South gate */
        if (ny == MAP_HEIGHT-1 && nx == m->south_gate) {
            if (cur_iy == WORLD_SIZE-1) { set_msg("Can't go further south!"); return 0; }
            cur_iy++;
            map_t *nm = get_or_generate(cur_ix, cur_iy);
            nm->player.x = nm->north_gate;
            nm->player.y = 1;
            *mp = nm;
            m_player_party_ptr = &nm->player.party;
            set_msg("Entered map to the south.");
            return 1;
        }
        /* West gate */
        if (nx == 0 && ny == m->west_gate) {
            if (cur_ix == 0) { set_msg("Can't go further west!"); return 0; }
            cur_ix--;
            map_t *nm = get_or_generate(cur_ix, cur_iy);
            nm->player.x = MAP_WIDTH - 2;
            nm->player.y = nm->east_gate;
            *mp = nm;
            m_player_party_ptr = &nm->player.party;
            set_msg("Entered map to the west.");
            return 1;
        }
        /* East gate */
        if (nx == MAP_WIDTH-1 && ny == m->east_gate) {
            if (cur_ix == WORLD_SIZE-1) { set_msg("Can't go further east!"); return 0; }
            cur_ix++;
            map_t *nm = get_or_generate(cur_ix, cur_iy);
            nm->player.x = 1;
            nm->player.y = nm->west_gate;
            *mp = nm;
            m_player_party_ptr = &nm->player.party;
            set_msg("Entered map to the east.");
            return 1;
        }
    }

    /* ── Normal movement within the map ── */
    if (!is_valid_pos(nx, ny)) { set_msg("Can't go that way!"); return 0; }

    /* NPC collision */
    int ni = npc_at(m, nx, ny);
    if (ni >= 0) {
        if (m->npcs[ni].defeated) { set_msg("The defeated trainer looks away..."); return 0; }
        do_battle(m, ni);
        return 1;
    }

    terrain_type_t t = m->terrain[ny][nx];
    int cost = get_move_cost(TRAINER_PC, t);
    if (cost == INT_MAX) {
        const char *names[] = {"boulder","tree","path","center","mart",
                               "grass","clearing","water","mountain","forest"};
        set_msg("There's a %s in the way!", (t < NUM_TERRAIN_TYPES) ? names[t] : "obstacle");
        return 0;
    }

    m->player.x = nx;
    m->player.y = ny;
    set_msg("");
    /* Wild encounter check when stepping into tall grass */
    if (t == TERRAIN_GRASS) maybe_wild_encounter(cur_ix, cur_iy);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Game loop                                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  PC starter selection
 * ═══════════════════════════════════════════════════════════════════════════ */
static void choose_starter(map_t *m)
{
    if (!g_db.loaded) return;

    /* Generate 3 random level-1 starters */
    GamePokemon options[3];
    for (int i = 0; i < 3; i++) options[i] = generate_starter();

    int choice = 0;
    while (true) {
        clear();
        attron(COLOR_PAIR(CP_MSG) | A_BOLD);
        mvprintw(0, 0, "  Choose your starter Pokemon!  (1/2/3 to pick)  ");
        attroff(COLOR_PAIR(CP_MSG) | A_BOLD);

        for (int i = 0; i < 3; i++) {
            int row = 2 + i * 5;
            if (i == choice)
                attron(COLOR_PAIR(CP_PATH) | A_BOLD);
            mvprintw(row, 2, "[%d]", i+1);
            if (i == choice) attroff(COLOR_PAIR(CP_PATH) | A_BOLD);
            print_pokemon_details(options[i], row, 6);
        }
        mvprintw(22, 2, "Press 1, 2, or 3 to select your starter.");
        refresh();

        int ch = getch();
        if (ch == '1') { choice = 0; break; }
        if (ch == '2') { choice = 1; break; }
        if (ch == '3') { choice = 2; break; }
    }

    m->player.party.push_back(options[choice]);
    clear();
    set_msg("You chose %s!", options[choice].name.c_str());
}

static void game_loop()
{
    map_t *m = world[cur_iy][cur_ix];
    m_player_party_ptr = &m->player.party;
    set_msg("Welcome! vi/numpad=move, f=fly, B=bag, t=trainers, Q=quit.");
    render_map(m, cur_ix, cur_iy);

    while (true) {
        int ch = getch();
        int moved = 0;

        switch (ch) {
            case '7': case 'y':  moved = attempt_move(&m, -1, -1); break;
            case '8': case 'k': case KEY_UP:    moved = attempt_move(&m,  0, -1); break;
            case '9': case 'u':  moved = attempt_move(&m,  1, -1); break;
            case '6': case 'l': case KEY_RIGHT: moved = attempt_move(&m,  1,  0); break;
            case '3': case 'n':  moved = attempt_move(&m,  1,  1); break;
            case '2': case 'j': case KEY_DOWN:  moved = attempt_move(&m,  0,  1); break;
            case '1': case 'b':  moved = attempt_move(&m, -1,  1); break;
            case '4': case 'h': case KEY_LEFT:  moved = attempt_move(&m, -1,  0); break;

            case '5': case ' ': case '.':
                moved = 1;
                set_msg("You rest for a moment.");
                break;

            case '>': {
                terrain_type_t t = m->terrain[m->player.y][m->player.x];
                if (t == TERRAIN_POKECENTER || t == TERRAIN_POKEMART) {
                    do_building(t); clear();
                } else {
                    set_msg("You're not standing in a building.");
                }
                break;
            }

            case 'f':
                m = do_fly(m);
                moved = 1;
                break;

            case 't':
                show_trainer_list(m); clear(); break;

            case 'B':
                if (m_player_party_ptr)
                    use_bag_outside_battle(*m_player_party_ptr, g_bag);
                break;

            case 'Q':
                set_msg("Goodbye!");
                render_map(m, cur_ix, cur_iy);
                return;

            default: break;
        }

        if (moved) {
            turn++;
            move_npcs(m);
        }

        render_map(m, cur_ix, cur_iy);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  main                                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* If a table name is given as argument, dump that CSV and exit */
    if (argc == 2) {
        return run_pokedex_dump(argv[1]);
    }
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [pokemon|moves|pokemon_moves|pokemon_species"
                        "|experience|type_names|pokemon_stats|stats|pokemon_types]\n",
                argv[0]);
        return 1;
    }

    /* Normal game startup */
    srand((unsigned)time(nullptr));
    memset(world, 0, sizeof(world));

    /* Load pokedex DB (needed for pokemon generation) */
    load_pokedex_db();

    world[cur_iy][cur_ix] = generate_map(cur_ix, cur_iy, -1, -1, -1, -1);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) init_colors();

    /* PC starter selection */
    choose_starter(world[cur_iy][cur_ix]);

    game_loop();

    endwin();

    for (int y = 0; y < WORLD_SIZE; y++)
        for (int x = 0; x < WORLD_SIZE; x++)
            delete world[y][x];

    return 0;
}
