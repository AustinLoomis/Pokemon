# Changelog

All notable changes to the Pokemon Terrain Generator project.

## [1.09] - 2026-04-18
 
### Added
- `battle.hpp`: full Pokemon battle engine (all new code in C++)
- `Bag` struct: holds `potions`, `revives`, `pokeballs`; `g_bag` global initialized to 5 potions, 3 revives, 10 Pokeballs at startup
- `calc_damage()`: implements the full damage formula — `(((2L/5+2) * Power * Atk/Def)/50 + 2) * Crit * Rand[85-100] * STAB * Type`; accuracy check via `rand()%100 >= accuracy` (miss returns -1); critical hit threshold `floor(base_speed/2)` checked against `rand()%256`; STAB 1.5x if any of attacker's types matches move type; type effectiveness simplified to 1.0; minimum 1 damage enforced
- `get_pokemon_type_ids()`: looks up all type IDs for a species from `g_db.pokemon_types` for STAB calculation
- `get_base_speed()`: fetches base speed stat for crit threshold
- `do_battle_turn()`: core turn engine; PC action menu with arrow-key and number-key navigation across four options:
  - **Fight** — move selection submenu; turn order determined by move priority then speed then coin flip; both sides attack in order; KO triggers auto-switch to next alive party member
  - **Bag** — Potion (+20 HP to active Pokemon), Revive (first KO'd party member restored to 50% HP), Pokeball (wild only; succeeds if party size < 6, otherwise wild flees)
  - **Pokemon** — party switch submenu; cannot switch to a KO'd Pokemon; uses the turn
  - **Run** — wild battles only; uses full escape probability formula `(SpeedPC*32)/(SpeedWild/4%256+1) + 30*attempts`; failed attempts tracked per battle
- `battle_wild()`: top-level wild battle loop; returns `true` on win or catch, `false` on flee or loss
- `battle_trainer()`: top-level trainer battle loop; opening message names the trainer and their lead Pokemon; cannot be fled; loops until one side is fully KO'd
- `ai_choose_move()`: NPC AI selects a random move from the active Pokemon; switches automatically when the current Pokemon faints
- `draw_battle_hud()`: ncurses battle screen with enemy Pokemon top-right and player Pokemon bottom-left, HP bars, and a log line
- `draw_battle_menu()`, `draw_move_menu()`, `draw_party_menu()`, `draw_bag_menu()`: individual ncurses submenu renderers
- `battle_msg()`: pauses on a message and waits for any key
- `use_bag_outside_battle()`: bag screen accessible via `B` key on the overworld; shows full party HP and lets the player apply Potions or Revives by party slot number
- `B` key command in `game_loop()`: opens the out-of-battle bag
- Status bar now shows live bag counts and party size: `Bag: NP NR NPB  Party:N`
### Changed
- `do_battle()` (trainer battle): replaced display-only placeholder with a call to `battle_trainer()`; NPC party KOs now persist after battle ends
- `maybe_wild_encounter()`: replaced display-only placeholder with a call to `battle_wild()`; real catch and flee mechanics apply
- `do_building()` PokéCenter: now fully heals all party Pokemon HP to max on entry
- `do_building()` PokéMart: now restocks `g_bag` to default quantities on entry (5 potions, 3 revives, 10 Pokeballs)
- `PokedexDB` gains `pokemon_types` vector; `load_pokedex_db()` now parses `pokemon_types.csv` at startup for STAB lookup
- `m_player_party_ptr` global added and kept in sync across all map transitions so wild encounters always reference the active map's party
- Makefile updated to list `battle.hpp` as a build dependency


## [1.08] - 2026-04-12
 
### Added
- `pokemon_gen.hpp`: Pokémon generation engine (all new code in C++)
- `PokedexDB` struct and `load_pokedex_db()`: loads `pokemon`, `moves`, `pokemon_moves`, and `pokemon_stats` CSVs into a global singleton at startup; searches the same two paths as the CSV dump mode
- `GamePokemon` struct: holds species id, name, level, shiny flag, gender, six IVs, six calculated stats (HP, Atk, Def, SpAtk, SpDef, Spd), and up to two moves
- `PokemonMove_ig` struct: in-game move representation (id + name)
- `generate_pokemon(level)`: picks a uniformly random species from all 1092 entries, rolls IVs [0, 15] per stat, computes stats using the spec formulae, assigns up to two level-up moves, rolls shiny (1/8192) and gender (50/50)
- `calc_stat()`: implements HP = floor(((base+IV)×2×level)/100) + level + 10 and other stats = floor(((base+IV)×2×level)/100) + 5
- `find_min_usable_level()`: advances a Pokémon's level until at least one level-up move is learnable; assigns Struggle as a final fallback
- `assign_moves()`: deduplicates level-up moves across version groups, filters to moves learnable at or below the Pokémon's level, shuffles, and picks up to two
- `level_range_for_dist(dist, &min, &max)`: dist ≤ 200 → min=1, max=dist/2; dist > 200 → min=(dist−200)/2, max=100
- `generate_pokemon_for_map(ix, iy)`: derives manhattan distance from world center, computes level range, generates a Pokémon at a random level in that range
- `generate_party(ix, iy)`: generates at least one Pokémon for a trainer; each additional Pokémon has a 60% chance of being added, up to a maximum of 6
- `generate_starter()`: generates a level-1 Pokémon for the PC starter selection screen
- `choose_starter()`: ncurses screen shown at game start; presents three randomly generated level-1 Pokémon; player presses 1, 2, or 3 to choose; chosen Pokémon added to `m->player.party`
- `print_pokemon_details()`: ncurses helper that renders one Pokémon's name, level, shiny/gender flags, HP/Atk/Def/SpAtk/SpDef/Spd, IVs, and moves to a given screen row
- `maybe_wild_encounter(ix, iy)`: called on every step into tall grass; 10% encounter rate; spawns a wild Pokémon appropriate to the map's manhattan distance; displays its full details in an ncurses screen; dismissed with Escape
- Updated `do_battle()`: now displays the NPC's full Pokémon party (name, level, shiny/gender, all stats, IVs, moves) before waiting for Escape
### Changed
- `pc` class gains `std::vector<GamePokemon> party`
- `npc` class gains `std::vector<GamePokemon> party`
- `place_npcs()` now accepts `ix, iy` map coordinates and calls `generate_party(ix, iy)` for each NPC at spawn time
- `generate_map()` passes `ix, iy` through to `place_npcs()`
- `attempt_move()`: extracts `terrain_type_t t` before the passability check so it is in scope for the grass encounter call
- `main()`: calls `load_pokedex_db()` before map generation, then `choose_starter()` before `game_loop()`
- Makefile updated to list `pokemon_gen.hpp` as a build dependency


## [1.07] - 2026-04-03
 
### Added
- `pokedex.hpp`: Pokédex CSV parsing module (all new code in C++)
- Nine structs to hold parsed data: `Pokemon`, `Move`, `PokemonMove`, `PokemonSpecies`, `Experience`, `TypeName`, `PokemonStat`, `Stat`, `PokemonType`
- Nine corresponding parse functions, each opening the relevant CSV from the resolved database directory
- Nine print functions that reproduce the parsed data to stdout in CSV format; empty fields (stored as `INT_MAX`) are printed as blank
- `find_pokedex_dir()`: searches `/share/cs327/pokedex/pokedex/data/csv` first, then `$HOME/.poke327/pokedex/pokedex/data/csv` (resolved via `getenv("HOME")`); prints an error and terminates if neither location contains the database
- `run_pokedex_dump()`: top-level dispatcher that resolves the database path, selects the correct parse/print pair by table name, and returns a status code
- CSV dump mode: passing a single table name argument (e.g. `./terrain_generator pokemon`) parses and prints that file to stdout and exits before initializing ncurses
- Two-word argument support: `argc == 3` joins `argv[1]` and `argv[2]` so both `pokemon_moves` and `pokemon moves` are accepted as equivalent table names
- `type_names.csv` filtered to English-only rows (`local_language_id == 9`) to avoid non-ASCII output
 
### Changed
- `main()` now checks `argc` before any game initialization: `argc == 2` or `argc == 3` triggers CSV dump mode and returns without starting the game


## [1.06] - 2026-03-28
 
### Changed
- C++ port:
- File renamed terrain_generator.cpp; all malloc/free replaced with new/delete; C-style casts replaced with C++ casts; NULL → nullptr; int booleans → bool struct → class for map_t, heap_t, heap_node_t, seed_t — all members public per the spec's allowance
- Inheritance: character base class holds x, y, type; pc and npc both inherit from it. map_t now stores a pc player and npc npcs[] instead of raw coordinate fields
- Gate traversal:
- PC can now walk into a gate cell — attempt_move detects this via is_gate() and transitions to the neighboring map, placing the PC at the cell directly abeam of the corresponding gate (e.g. north gate → row MAP_HEIGHT-2 on the south side of the neighbor)
- NPCs are explicitly blocked from entering gate cells in move_npcs
- NPC positions and defeated state are preserved across transitions (they live in map_t, which persists in the world cache)
- f triggers do_fly(), which prompts for X and Y coordinates directly in the message row using mvgetnstr with echo/curs_set toggled on briefly, then off again — no Enter-key weirdness in the rest of the UI
- PC is placed on a road in the destination map; bounds-checked against ±WORLD_CTR


## [1.05] - 2026-03-17
 
### Added
- ncurses-based user interface replacing the text-only simulation loop
- Unbuffered input via `cbreak()`/`noecho()`/`keypad()` — no Enter required
- Full color terrain rendering: grass (green), water (blue), paths (yellow), PokéCenter (cyan), PokéMart (magenta), boulders/mountains (white), trees/forests (green)
- Colored NPC rendering: hikers in bold yellow (`h`), rivals in bold magenta (`r`), PC in bold white-on-red (`@`)
- Screen layout: row 0 = message line, rows 1–21 = map, rows 22–23 = status bar
- All required movement key bindings:
  - Numpad: `7` `8` `9` `6` `3` `2` `1` `4`
  - vi keys: `y` `k` `u` `l` `n` `j` `b` `h`
  - Arrow keys mapped to cardinal directions
- `5` / `space` / `.` — rest for one turn (NPCs still move)
- `>` — enter PokéCenter or PokéMart (placeholder UI, exit with `<`)
- `t` — trainer list overlay showing each NPC's symbol, type, and position relative to the PC (e.g. `h  Hiker  3 north and 14 west`); scrollable with up/down arrows, dismissed with Escape
- `Q` — quit the game
- Battle placeholder: stepping into an NPC (or NPC stepping into PC) triggers a battle screen; press Escape to flee and mark the trainer as defeated
- Defeated trainers stop pathfinding toward the PC and switch to random wandering
- PC cannot enter a cell occupied by a defeated trainer
- World map transitions: walking off a map edge via a gate loads/generates the adjacent map and repositions the PC at the corresponding entry point
- PC blocked from standing directly on gate cells (prevents pathfinding corruption)
 
### Changed
- `attempt_move` now takes `map_t **` to support in-place world transitions
- NPC Dijkstra distance maps computed once per turn per trainer type (not per NPC)
- PC movement costs defined separately from NPC costs: mountains, forests, and water are impassable for the PC
- `place_pc` skips gate cells when finding an initial road position
- Random-wander direction for defeated NPCs re-randomized on each blocked step
 
### Removed
- `usleep`-based auto-drive PC movement
- Per-frame distance map printout to stdout
- Text-prompt game loop (`fgets` / command parsing)
 

## [1.04] - 2026-03-04

### Added
- Six NPC trainer types, each with a distinct movement strategy:
  - `h` **Hiker** — paths to the PC via steepest descent on the hiker distance map
  - `r` **Rival** — paths to the PC via steepest descent on the rival distance map
  - `p` **Pacer** — walks in one direction, reverses when blocked
  - `w` **Wanderer** — stays within its spawn terrain type; picks a new random direction at the boundary
  - `s` **Sentry** — stationary; does not move
  - `e` **Explorer** — like a wanderer but freely crosses terrain boundaries, only redirecting at impassable cells
- `--numtrainers <n>` CLI switch (default: 10, max: 32); always guarantees at least one hiker and one rival when n ≥ 2
- Turn-based priority queue: all characters (PC + NPCs) are scheduled by `next_move_time`; next turn time = current time + terrain cost of destination
- PC random movement along `#` path cells, redrawn at 4 fps via `usleep(250000)`
- Distance maps regenerated automatically whenever the PC moves
- Collision enforcement: no character may spawn in or move into a cell occupied by another character or an exit

### Changed
- Program now runs as a continuous simulation (quit with Ctrl-C) rather than a command-driven navigator
- `map_t` extended with `npcs[]` array and `num_npcs` count
- Added `character_t` struct (type, position, direction, next_move_time, alive flag)
- Added second min-heap (`turn_heap_t`) for the turn-order priority queue
- Map legend line printed after each frame showing all NPC symbols

### Removed
- Interactive navigation commands (`n`, `s`, `e`, `w`, `f`, `q`) — world navigation is replaced by the live simulation loop
- Per-frame distance map printout (maps are still computed internally and used by hikers/rivals)


## [1.03] - 2026-02-20

### Fixed
- Fixed bug where there were still exits on world borders even though you could not go through them

### Added
- Player Character (PC) placement on random road locations (displayed as `@`)
- Dijkstra's algorithm for pathfinding with min-heap priority queue
- Distance map calculations for two trainer types:
  - **Hikers**: Can traverse mountains/forests, cost 15 for tall grass
  - **Rivals**: Cannot traverse mountains/forests/water, cost 20 for tall grass
- Movement cost system based on terrain and trainer type (8-directional pathfinding)
- Distance map display (2-digit numbers, distance mod 100, unreachable cells shown as blanks)
- Dual terrain representation system:
  - `cells[][]` for visual display
  - `terrain[][]` for internal pathfinding logic
- New terrain types:
  - **Mountains** (display as `%`, hikers can traverse at cost 15, rivals cannot)
  - **Forests** (display as `^`, hikers can traverse at cost 15, rivals cannot)
- Mountain and forest region generation (1 seed each)
- Automatic recalculation and display of both distance maps after each navigation command

### Changed
- Map structure now includes both display characters and internal terrain types
- Terrain generation now creates 5 region types (grass, water, clearing, mountain, forest)
- Decorations (boulders/trees) are truly impassable, distinct from mountains/forests


## [1.02] - 2026-02-10

### Added
- 401×401 world grid — all maps NULL until visited, cached forever after
- Interactive game loop with 6 commands: `n` `s` `e` `w` `f <x> <y>` `q`
- Gate alignment: new generated maps inherit gate positions from already-generated maps
- Coordinate display (`Map (x, y)`) printed beneath each map
- Building probability scaled by Manhattan distance: `−45d/200 + 50`, 5% minnimum
- Centre map (0, 0) guaranteed both Pokemon Center and PokeMart
- Graceful handling of out-of-bounds moves/fly and unknown commands


## [1.01] - 2026-02-04

### Added
- Initial terrain generator implementation
- Map structure, 80×21 dimension
- Border system with one exit on each side (north, south, east, west)
- N-S and E-W intersecting path generation with natural meandering
- 2×2 Pokemon Center and PokeMart building placement
- Terrain types:
  - Tall grass regions (`:`)
  - Water regions (`~`)
  - Clearing regions (`.`)
  - Boulders (`%`)
  - Trees (`^`)


# Format

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
