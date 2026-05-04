#pragma once
/*
 * pokemon_gen.hpp
 * Pokemon generation: load pokedex DB into memory, generate wild pokemon
 * and trainer parties with stats, IVs, moves, gender, shiny.
 */

#include "pokedex.hpp"
#include <algorithm>
#include <cassert>

/* ═══════════════════════════════════════════════════════════════════════════
 *  In-game Pokemon representation
 * ═══════════════════════════════════════════════════════════════════════════ */

struct PokemonMove_ig {   /* "in-game" move (just id + name for now) */
    int  id;
    std::string name;
};

struct GamePokemon {
    int         species_id;       /* index into DB */
    std::string name;             /* species identifier */
    int         level;
    bool        is_shiny;
    bool        is_female;        /* simplified: 50/50 */

    /* IVs: indices 0-5 = hp,atk,def,spatk,spdef,spd */
    int iv[6];

    /* Calculated stats (same order) */
    int hp_max;
    int hp_cur;
    int atk;
    int def;
    int spatk;
    int spdef;
    int spd;

    /* Up to 2 level-up moves */
    PokemonMove_ig moves[2];
    int            num_moves;
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Pokedex database (loaded once at startup)
 * ═══════════════════════════════════════════════════════════════════════════ */
struct PokedexDB {
    std::vector<Pokemon>        pokemon;
    std::vector<Move>           moves;
    std::vector<PokemonMove>    pokemon_moves;
    std::vector<PokemonStat>    pokemon_stats;
    std::vector<PokemonType>    pokemon_types;
    bool loaded = false;
};

static PokedexDB g_db;

/* Load the DB (call once before generating any pokemon) */
static bool load_pokedex_db()
{
    if (g_db.loaded) return true;

    std::string dir = find_pokedex_dir();
    if (dir.empty()) {
        fprintf(stderr,
            "Error: pokedex database not found.\n"
            "Searched:\n"
            "  /share/cs327/pokedex/pokedex/data/csv\n"
            "  $HOME/.poke327/pokedex/pokedex/data/csv\n");
        return false;
    }

    try {
        g_db.pokemon       = parse_pokemon(dir);
        g_db.moves         = parse_moves(dir);
        g_db.pokemon_moves = parse_pokemon_moves(dir);
        g_db.pokemon_stats = parse_pokemon_stats(dir);
        g_db.pokemon_types = parse_pokemon_types(dir);
    } catch (const std::exception &ex) {
        fprintf(stderr, "DB load error: %s\n", ex.what());
        return false;
    }

    g_db.loaded = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Stat formula
 * ═══════════════════════════════════════════════════════════════════════════ */

/* stat_id: 1=hp 2=atk 3=def 4=spatk 5=spdef 6=spd */
static int calc_stat(int stat_id, int base, int iv, int level)
{
    if (stat_id == 1) {
        /* HP = floor(((base+iv)*2*level)/100) + level + 10 */
        return ((base + iv) * 2 * level) / 100 + level + 10;
    } else {
        /* Other = floor(((base+iv)*2*level)/100) + 5 */
        return ((base + iv) * 2 * level) / 100 + 5;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Level-up moveset helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Collect all unique (move_id, level) from level-up learnset for a species.
 * spec_id = pokemon.species_id (== pokemon_moves.pokemon_id for base forms).
 * Returns pairs (level_learned, move_id) sorted by level. */
static std::vector<std::pair<int,int>> get_levelup_moves(int pokemon_id)
{
    std::vector<std::pair<int,int>> all;

    for (const auto &pm : g_db.pokemon_moves) {
        if (pm.pokemon_id != pokemon_id) continue;
        if (pm.pokemon_move_method_id != 1) continue; /* level-up only */
        int lv = (pm.level == INT_MAX) ? 0 : pm.level;
        all.push_back({lv, pm.move_id});
    }

    /* Deduplicate (same move may appear in multiple version groups) */
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());

    return all;
}

/* Advance level until at least one level-up move is available */
static int find_min_usable_level(int pokemon_id, int start_level)
{
    auto all = get_levelup_moves(pokemon_id);
    if (all.empty()) return start_level; /* no level-up moves at all → keep level */

    int lv = start_level;
    while (lv <= 100) {
        for (const auto &p : all)
            if (p.first <= lv) return lv;
        lv++;
    }
    return start_level;
}

/* Pick up to 2 moves from the level-up learnset at or below the given level.
 * Selects randomly from the eligible set. */
static void assign_moves(GamePokemon &gp, int pokemon_id)
{
    auto all = get_levelup_moves(pokemon_id);

    /* Filter to moves learnable at or below this level */
    std::vector<int> eligible;
    for (const auto &p : all) {
        if (p.first <= gp.level)
            eligible.push_back(p.second);
    }

    /* Deduplicate move_ids */
    std::sort(eligible.begin(), eligible.end());
    eligible.erase(std::unique(eligible.begin(), eligible.end()), eligible.end());

    /* Shuffle and pick up to 2 */
    for (int i = (int)eligible.size()-1; i > 0; i--) {
        int j = rand() % (i+1);
        std::swap(eligible[i], eligible[j]);
    }

    gp.num_moves = 0;
    int take = std::min((int)eligible.size(), 2);
    for (int i = 0; i < take; i++) {
        int mid = eligible[i];
        gp.moves[gp.num_moves].id = mid;
        /* Look up move name */
        gp.moves[gp.num_moves].name = "unknown";
        for (const auto &mv : g_db.moves) {
            if (mv.id == mid) {
                gp.moves[gp.num_moves].name = mv.identifier;
                break;
            }
        }
        gp.num_moves++;
    }

    /* Fallback: Struggle if truly no moves */
    if (gp.num_moves == 0) {
        gp.moves[0].id   = 165;
        gp.moves[0].name = "struggle";
        gp.num_moves     = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Core: generate one GamePokemon at a given level
 * ═══════════════════════════════════════════════════════════════════════════ */
static GamePokemon generate_pokemon(int level)
{
    GamePokemon gp;
    gp.num_moves = 0;

    /* Pick a random species from the 1092 entries (skip header row in vector,
       indices 0..N-1, all uniform) */
    int idx = rand() % (int)g_db.pokemon.size();
    const Pokemon &poke = g_db.pokemon[idx];

    gp.species_id = poke.id;
    gp.name       = poke.identifier;

    /* Advance level if no move is learnable yet */
    level = find_min_usable_level(poke.species_id, level);
    if (level > 100) level = 100;
    gp.level = level;

    /* Shiny: 1/8192 */
    gp.is_shiny  = (rand() % 8192 == 0);
    gp.is_female = (rand() % 2 == 0);

    /* IVs [0,15] for hp,atk,def,spatk,spdef,spd */
    for (int i = 0; i < 6; i++) gp.iv[i] = rand() % 16;

    /* Base stats — look up each of the 6 stat_ids 1–6 */
    int base[7] = {}; /* index by stat_id 1-6 */
    for (const auto &ps : g_db.pokemon_stats) {
        if (ps.pokemon_id != poke.id) continue;
        if (ps.stat_id >= 1 && ps.stat_id <= 6)
            base[ps.stat_id] = ps.base_stat;
    }

    /* Map: iv[0]=hp(1), iv[1]=atk(2), iv[2]=def(3),
             iv[3]=spatk(4), iv[4]=spdef(5), iv[5]=spd(6) */
    gp.hp_max = calc_stat(1, base[1], gp.iv[0], level);
    gp.hp_cur = gp.hp_max;
    gp.atk    = calc_stat(2, base[2], gp.iv[1], level);
    gp.def    = calc_stat(3, base[3], gp.iv[2], level);
    gp.spatk  = calc_stat(4, base[4], gp.iv[3], level);
    gp.spdef  = calc_stat(5, base[5], gp.iv[4], level);
    gp.spd    = calc_stat(6, base[6], gp.iv[5], level);

    /* Moves */
    assign_moves(gp, poke.species_id);

    return gp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Level range for a given manhattan distance from origin
 * ═══════════════════════════════════════════════════════════════════════════ */
static void level_range_for_dist(int dist, int *min_lv, int *max_lv)
{
    if (dist <= 200) {
        *min_lv = 1;
        *max_lv = std::max(1, dist / 2);
    } else {
        *min_lv = std::max(1, (dist - 200) / 2);
        *max_lv = 100;
    }
}

/* Generate a Pokemon appropriate for the given map position (ix,iy internal) */
static GamePokemon generate_pokemon_for_map(int ix, int iy)
{
    int dist = abs(ix - 200) + abs(iy - 200);   /* WORLD_CTR = 200 */
    int min_lv, max_lv;
    level_range_for_dist(dist, &min_lv, &max_lv);
    int level = min_lv + rand() % (max_lv - min_lv + 1);
    return generate_pokemon(level);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Trainer party generation
 *  60% chance of each additional pokemon, up to 6 total
 * ═══════════════════════════════════════════════════════════════════════════ */
static std::vector<GamePokemon> generate_party(int ix, int iy)
{
    std::vector<GamePokemon> party;
    /* Always at least 1 */
    party.push_back(generate_pokemon_for_map(ix, iy));
    /* 60% chance for each additional, max 6 */
    while ((int)party.size() < 6 && rand() % 100 < 60)
        party.push_back(generate_pokemon_for_map(ix, iy));
    return party;
}

/* Level-1 pokemon for starter/PC selection */
static GamePokemon generate_starter()
{
    return generate_pokemon(1);
}
