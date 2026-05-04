# Pokemon Terrain Generator

Randomly generated 80x21 terrain maps for a Pokemon-inspired game, featuring Dijkstra pathfinding, NPC trainer types, a full ncurses-based user interface with color rendering, Pokedex CSV parsing, Pokemon generation with stats/IVs/moves, and a complete turn-based battle system.

## Building and Running

```bash
make                 # Compile
./terrain_generator  # Run the game
make clean           # Clean
```

### Pokedex CSV Dump Mode

Pass a table name as a single argument to parse and print that CSV file to stdout, then exit — the game will not start.

```bash
./terrain_generator pokemon
./terrain_generator moves
./terrain_generator pokemon_moves
./terrain_generator pokemon_species
./terrain_generator experience
./terrain_generator type_names
./terrain_generator pokemon_stats
./terrain_generator stats
./terrain_generator pokemon_types
```

The program searches for the Pokedex database in the following locations, in order:

1. `/share/cs327/pokedex/pokedex/data/csv`
2. `$HOME/.poke327/pokedex/pokedex/data/csv`

If the database is not found in either location, the program prints an error and exits. **Do not place the database under your source tree.**

## Controls

All commands activate immediately on key-press — no Enter required.

| Key(s) | Action |
|--------|--------|
| `7` or `y` | Move upper-left |
| `8` or `k` or `↑` | Move up |
| `9` or `u` | Move upper-right |
| `6` or `l` or `→` | Move right |
| `3` or `n` | Move lower-right |
| `2` or `j` or `↓` | Move down |
| `1` or `b` | Move lower-left |
| `4` or `h` or `←` | Move left |
| `5` or `space` or `.` | Rest one turn (NPCs still move) |
| `>` | Enter a PokeCenter or PokeMart (must be standing in one) |
| `f` | Fly to any map coordinate |
| `B` | Open bag (use potions/revives outside of battle) |
| `t` | Show trainer list (↑/↓ to scroll, Escape to return) |
| `Q` | Quit |

Walking off a map edge via a gate transitions to the adjacent map. The `f` command prompts for X and Y coordinates in the message bar.

## Pokemon

### Starter Selection
At game start, three random level-1 Pokemon are presented. Press `1`, `2`, or `3` to choose your starter.

### Wild Encounters
Each step into tall grass (`:`) has a 10% chance of triggering a wild encounter. The Pokemon's level scales with manhattan distance from the origin:

| Distance | Min Level | Max Level |
|----------|-----------|-----------|
| <= 200 | 1 | distance / 2 |
| > 200 | (distance - 200) / 2 | 100 |

### Stats and IVs
Each Pokemon has IVs [0-15] for each stat. Stats are calculated as:

- **HP** = `floor(((base + IV) * 2 * level) / 100) + level + 10`
- **Other** = `floor(((base + IV) * 2 * level) / 100) + 5`

### Moves
Pokemon know up to two moves drawn from their level-up learnset at or below their level. If no level-up move is available at the generated level, the level is advanced until one is. Struggle is assigned as a final fallback.

### Shiny and Gender
Each Pokemon has a 1/8192 chance of being shiny. Gender is determined with equal probability of male or female.

## Battles

### Wild Battles
Stepping into tall grass with a 10% chance triggers a wild battle. Options each turn:

- **Fight** — choose a move; turn order by priority, then speed, then coin flip
- **Bag** — use a Potion, Revive, or Pokeball
- **Pokemon** — switch your active Pokemon
- **Run** — attempt to flee using the full escape probability formula

### Trainer Battles
Stepping into an NPC (or an NPC stepping into the PC) triggers a trainer battle. Trainer battles cannot be fled and end when all of one side's Pokemon are KO'd. The NPC AI picks a random move each turn and auto-switches on KO.

### Damage Formula
`damage = (2*Level/5 + 2) * Power * Atk/Def / 50 + 2) * Crit * Rand * STAB * Type`

- **Crit**: 1.5x if `rand()%256 < floor(base_speed/2)`
- **Rand**: uniform in [85, 100] / 100
- **STAB**: 1.5x if move type matches any of attacker's types
- **Type**: simplified to 1.0
- Minimum 1 damage always applied

### Items
| Item | Effect | In Battle | Out of Battle |
|------|--------|-----------|---------------|
| Potion | Restore 20 HP (not above max) | Yes | Yes (`B` key) |
| Revive | Revive KO'd Pokemon to 50% HP | Yes | Yes (`B` key) |
| Pokeball | Attempt to catch a wild Pokemon | Wild only | No |

### Buildings
- **PokeCenter** (`>`): fully heals all party Pokemon HP to max
- **PokeMart** (`>`): restocks bag to defaults (5 potions, 3 revives, 10 Pokeballs)

## Trainer Types

| Symbol | Type | Movement Behaviour |
|--------|------|--------------------|
| `@` | Player Character | Controlled by the player |
| `h` | Hiker | Paths toward PC via hiker distance map |
| `r` | Rival | Paths toward PC via rival distance map |

No character may move into a cell occupied by another character or a gate. The PC cannot stand on gate cells. Defeated trainers wander randomly and cannot be re-battled.

## World Structure

- **World size**: 401x401 maps
- **Starting position**: Map (0, 0) — the center of the world
- **Coordinate range**: -200 to +200 in both axes
- Maps are generated on-demand when first visited and cached persistently
- Walking off a map edge transitions to the adjacent map with aligned gate positions

## Map Legend

| Symbol | Terrain / Character | Notes |
|--------|---------------------|-------|
| `@` | Player Character | Controlled by player |
| `h` | Hiker NPC | Paths toward PC |
| `r` | Rival NPC | Paths toward PC |
| `%` | Boulder or Mountain | Boulders are impassable; mountains are passable by hikers |
| `^` | Tree or Forest | Trees are impassable; forests are passable by hikers |
| `#` | Path/Road | Main travel routes |
| `C` | PokeCenter | Heals all party Pokemon (2x2) |
| `M` | PokeMart | Restocks bag supplies (2x2) |
| `:` | Tall grass | Wild Pokemon encounters (10% per step) |
| `.` | Clearing | Open ground |
| `~` | Water | Impassable for all |

## Movement Costs

### PC
| Terrain | Cost |
|---------|------|
| Path / Clearing / Building | 10 |
| Tall Grass | 20 |
| Boulder / Tree / Water / Mountain / Forest | Impassable |

### Hikers
| Terrain | Cost |
|---------|------|
| Path / Clearing | 10 |
| Tall Grass / Mountain / Forest | 15 |
| Building (C/M) | 50 |
| Boulder / Tree / Water | Impassable |

### Rivals
| Terrain | Cost |
|---------|------|
| Path / Clearing | 10 |
| Tall Grass | 20 |
| Building (C/M) | 50 |
| Boulder / Tree / Mountain / Forest / Water | Impassable |

## Implementation Details

### Class Hierarchy
```cpp
class character {          // base: x, y, type
public:
    int x, y;
    trainer_type_t type;
};

class pc : public character {
public:
    std::vector<GamePokemon> party;
};

class npc : public character {
public:
    int active, defeated, speed, next_move;
    int wander_dx, wander_dy;
    std::vector<GamePokemon> party;
};
```

### Key Structs
```cpp
struct Bag {
    int potions, revives, pokeballs;
};

struct PokemonMove_ig { int id; std::string name; };

struct GamePokemon {
    int species_id; std::string name;
    int level; bool is_shiny, is_female;
    int iv[6];                   // hp, atk, def, spatk, spdef, spd
    int hp_max, hp_cur, atk, def, spatk, spdef, spd;
    PokemonMove_ig moves[2]; int num_moves;
};
```

### Source Files
| File | Purpose |
|------|---------|
| `terrain_generator.cpp` | Map generation, world navigation, ncurses game loop |
| `pokedex.hpp` | CSV parsing for all nine Pokedex data files |
| `pokemon_gen.hpp` | Pokemon generation: stats, IVs, moves, level scaling |
| `battle.hpp` | Complete battle engine: damage, menus, items, AI, catch |

### Dual Terrain System
Maps store two parallel representations: `cells[][]` for visual display and `terrain[][]` for internal pathfinding. This allows the same symbol to have different traversability — `%` may be an impassable boulder or a hiker-traversable mountain; `^` may be an impassable tree or a hiker-traversable forest.
