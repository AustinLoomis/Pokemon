#pragma once
/*
 * battle.hpp
 * Handles: trainer battles, wild encounters, Fight/Bag/Run/Pokemon menu,
 * damage formula, STAB, crits, accuracy, capture, items, AI.
 */

#include "pokemon_gen.hpp"
#include <algorithm>

/* ═══════════════════════════════════════════════════════════════════════════
 *  PC inventory
 * ═══════════════════════════════════════════════════════════════════════════ */
struct Bag {
    int potions;    /* restore 20 HP */
    int revives;    /* revive KO'd pokemon to 50% HP */
    int pokeballs;  /* catch wild pokemon */
};

/* Starting supplies */
static Bag g_bag = { 5, 3, 10 };

/* ═══════════════════════════════════════════════════════════════════════════
 *  Type lookup helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Get all type_ids for a given pokemon_id from g_db */
static std::vector<int> get_pokemon_type_ids(int pokemon_id)
{
    std::vector<int> types;
    for (const auto &pt : g_db.pokemon_types) {
        if (pt.pokemon_id == pokemon_id)
            types.push_back(pt.type_id);
    }
    return types;
}

/* Get base speed for a pokemon species (stat_id 6) */
static int get_base_speed(int pokemon_id)
{
    for (const auto &ps : g_db.pokemon_stats)
        if (ps.pokemon_id == pokemon_id && ps.stat_id == 6)
            return ps.base_stat;
    return 50; /* fallback */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Damage calculation
 * ═══════════════════════════════════════════════════════════════════════════ */

static int calc_damage(const GamePokemon &attacker, const GamePokemon &defender,
                       const PokemonMove_ig &move_used)
{
    /* Look up move data */
    int power    = 40;   /* fallback */
    int accuracy = 100;
    int type_id  = 0;

    for (const auto &mv : g_db.moves) {
        if (mv.id == move_used.id) {
            power    = (mv.power    != INT_MAX && mv.power    > 0) ? mv.power    : 0;
            accuracy = (mv.accuracy != INT_MAX) ? mv.accuracy : 100;
            type_id  = (mv.type_id  != INT_MAX) ? mv.type_id  : 0;
            break;
        }
    }

    /* Accuracy check: rand()%100 < accuracy hits */
    if (rand() % 100 >= accuracy) return -1; /* miss */

    /* Critical hit threshold: floor(base_speed / 2), checked against [0,255] */
    int base_spd  = get_base_speed(attacker.species_id);
    int crit_thr  = base_spd / 2;
    double crit   = (rand() % 256 < crit_thr) ? 1.5 : 1.0;

    /* Random factor [85,100] */
    double rnd = (85 + rand() % 16) / 100.0;

    /* STAB: 1.5 if move type matches any of attacker's types */
    double stab = 1.0;
    auto atk_types = get_pokemon_type_ids(attacker.species_id);
    for (int tid : atk_types)
        if (tid == type_id) { stab = 1.5; break; }

    /* Type effectiveness: simplified to 1 as the spec allows */
    double type_eff = 1.0;

    /* Damage formula (using physical attack/defense for simplicity) */
    double dmg = ((2.0 * attacker.level / 5.0 + 2.0) *
                   power *
                   (double)attacker.atk / (double)defender.def /
                   50.0 + 2.0)
                 * crit * rnd * stab * type_eff;

    int d = (int)dmg;
    if (d < 1) d = 1; /* minimum 1 damage */
    return d;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ncurses battle UI helpers
 *  (these need CP_ constants and mvprintw — included from terrain_generator)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define CP_DEFAULT   1
#define CP_PATH      4
#define CP_MSG       12
#define CP_CENTER    5
#define CP_NPC_HIKER 10

/* Draw the battle HUD: two pokemon side by side */
static void draw_battle_hud(const GamePokemon &pc_poke, const GamePokemon &foe_poke,
                             const char *log_line)
{
    clear();
    /* Header */
    attron(COLOR_PAIR(CP_MSG) | A_BOLD);
    mvprintw(0, 0, "%-80s", "  POKEMON BATTLE  ");
    attroff(COLOR_PAIR(CP_MSG) | A_BOLD);

    /* Enemy pokemon (top right) */
    attron(COLOR_PAIR(CP_NPC_HIKER) | A_BOLD);
    mvprintw(2, 42, "%-20s Lv.%d%s", foe_poke.name.c_str(), foe_poke.level,
             foe_poke.is_shiny ? " *" : "");
    attroff(COLOR_PAIR(CP_NPC_HIKER) | A_BOLD);
    /* HP bar for foe */
    int foe_pct = (foe_poke.hp_max > 0) ? (foe_poke.hp_cur * 20 / foe_poke.hp_max) : 0;
    mvprintw(3, 42, "HP: [");
    for (int i = 0; i < 20; i++)
        addch(i < foe_pct ? '=' : ' ');
    printw("] %3d/%3d", foe_poke.hp_cur, foe_poke.hp_max);

    /* Player pokemon (bottom left) */
    attron(COLOR_PAIR(CP_PATH) | A_BOLD);
    mvprintw(6, 2, "%-20s Lv.%d%s", pc_poke.name.c_str(), pc_poke.level,
             pc_poke.is_shiny ? " *" : "");
    attroff(COLOR_PAIR(CP_PATH) | A_BOLD);
    int pc_pct = (pc_poke.hp_max > 0) ? (pc_poke.hp_cur * 20 / pc_poke.hp_max) : 0;
    mvprintw(7, 2, "HP: [");
    for (int i = 0; i < 20; i++)
        addch(i < pc_pct ? '=' : ' ');
    printw("] %3d/%3d", pc_poke.hp_cur, pc_poke.hp_max);

    /* Log line */
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(10, 2, "%-76s", log_line);
    attroff(COLOR_PAIR(CP_MSG));
}

/* Draw PC action menu */
static void draw_battle_menu(const GamePokemon &pc_poke, bool is_wild,
                              const Bag &bag, int highlight)
{
    const char *opts[] = { "Fight", "Bag", "Pokemon", "Run" };
    mvprintw(13, 2, "What will %s do?", pc_poke.name.c_str());
    for (int i = 0; i < 4; i++) {
        bool show_run = (i == 3);
        /* Disable "Run" in trainer battles */
        bool disabled = (show_run && !is_wild);
        if (i == highlight)
            attron(A_REVERSE);
        if (disabled)
            attron(COLOR_PAIR(CP_NPC_HIKER));
        mvprintw(14 + (i/2), 4 + (i%2)*20, "[%d] %-12s", i+1, opts[i]);
        if (disabled)
            attroff(COLOR_PAIR(CP_NPC_HIKER));
        if (i == highlight)
            attroff(A_REVERSE);
    }
    mvprintw(17, 2, "Bag: %d Potion(s)  %d Revive(s)  %d Pokeball(s)%s",
             bag.potions, bag.revives, bag.pokeballs,
             is_wild ? "" : "  (Pokeballs: trainer only)");
    mvprintw(18, 2, "Use arrow keys or 1-4, Enter/Space to confirm. ESC=back.");
    refresh();
}

/* Draw move selection menu */
static int draw_move_menu(const GamePokemon &poke, int highlight)
{
    mvprintw(13, 2, "%-40s", "Choose a move:");
    for (int i = 0; i < poke.num_moves; i++) {
        if (i == highlight) attron(A_REVERSE);
        mvprintw(14+i, 4, "[%d] %-20s", i+1, poke.moves[i].name.c_str());
        if (i == highlight) attroff(A_REVERSE);
    }
    mvprintw(14 + poke.num_moves, 4, "ESC = back");
    refresh();
    return highlight;
}

/* Draw party selection screen */
static int draw_party_menu(const std::vector<GamePokemon> &party, int highlight)
{
    clear();
    attron(COLOR_PAIR(CP_MSG) | A_BOLD);
    mvprintw(0, 0, "%-80s", "  Switch Pokemon  (ESC=cancel)  ");
    attroff(COLOR_PAIR(CP_MSG) | A_BOLD);
    for (int i = 0; i < (int)party.size(); i++) {
        const auto &gp = party[i];
        bool ko = (gp.hp_cur <= 0);
        if (i == highlight) attron(A_REVERSE);
        if (ko) attron(COLOR_PAIR(CP_NPC_HIKER));
        mvprintw(2+i, 2, "[%d] %-20s Lv.%2d  HP:%3d/%-3d %s",
                 i+1, gp.name.c_str(), gp.level,
                 gp.hp_cur, gp.hp_max, ko ? "[KO]" : "    ");
        if (ko) attroff(COLOR_PAIR(CP_NPC_HIKER));
        if (i == highlight) attroff(A_REVERSE);
    }
    refresh();
    return highlight;
}

/* Draw bag menu */
static int draw_bag_menu(const Bag &bag, bool is_wild, int highlight)
{
    clear();
    attron(COLOR_PAIR(CP_MSG) | A_BOLD);
    mvprintw(0, 0, "%-80s", "  Bag  (ESC=cancel)  ");
    attroff(COLOR_PAIR(CP_MSG) | A_BOLD);

    struct BagItem { const char *name; int qty; bool avail; };
    BagItem items[] = {
        { "Potion   (restore 20 HP)",        bag.potions,   true     },
        { "Revive   (revive KO at 50% HP)",   bag.revives,   true     },
        { "Pokeball (catch wild pokemon)",     bag.pokeballs, is_wild  },
    };

    for (int i = 0; i < 3; i++) {
        bool disabled = !items[i].avail || items[i].qty == 0;
        if (i == highlight) attron(A_REVERSE);
        if (disabled) attron(COLOR_PAIR(CP_NPC_HIKER));
        mvprintw(2+i, 2, "[%d] %-40s x%d", i+1, items[i].name, items[i].qty);
        if (disabled) attroff(COLOR_PAIR(CP_NPC_HIKER));
        if (i == highlight) attroff(A_REVERSE);
    }
    mvprintw(6, 2, "ESC = back");
    refresh();
    return highlight;
}

/* Wait for any key, show a message */
static void battle_msg(const char *fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    attron(COLOR_PAIR(CP_MSG));
    mvprintw(10, 2, "%-76s", buf);
    mvprintw(11, 2, "%-76s", "(press any key)");
    attroff(COLOR_PAIR(CP_MSG));
    refresh();
    getch();
    /* clear those lines */
    mvprintw(11, 2, "%-76s", "");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  AI move selection
 * ═══════════════════════════════════════════════════════════════════════════ */
static int ai_choose_move(const GamePokemon &poke)
{
    /* Simple: pick a random move */
    if (poke.num_moves == 0) return 0;
    return rand() % poke.num_moves;
}

/* Find first non-KO'd pokemon in party; returns index or -1 */
static int first_alive(const std::vector<GamePokemon> &party)
{
    for (int i = 0; i < (int)party.size(); i++)
        if (party[i].hp_cur > 0) return i;
    return -1;
}

/* Is the whole party KO'd? */
static bool all_ko(const std::vector<GamePokemon> &party)
{
    return first_alive(party) == -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Flee probability (optional full formula)
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool attempt_flee(const GamePokemon &pc_poke, const GamePokemon &wild_poke,
                          int flee_attempts)
{
    int spd_t = pc_poke.spd;
    int spd_w = std::max(1, wild_poke.spd);
    int odds  = (spd_t * 32) / ((spd_w / 4) % 256 + 1) + 30 * flee_attempts;
    if (odds > 255) return true;
    return (rand() % 256 < odds);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Apply one side's attack
 *  Returns: "hit", "miss", "ko"
 * ═══════════════════════════════════════════════════════════════════════════ */
static const char *apply_attack(GamePokemon &attacker, GamePokemon &defender,
                                  const PokemonMove_ig &move_used,
                                  char *log, int logsz)
{
    int dmg = calc_damage(attacker, defender, move_used);
    if (dmg < 0) {
        snprintf(log, logsz, "%s used %s... but it missed!",
                 attacker.name.c_str(), move_used.name.c_str());
        return "miss";
    }
    defender.hp_cur -= dmg;
    if (defender.hp_cur < 0) defender.hp_cur = 0;
    snprintf(log, logsz, "%s used %s! (%d dmg)%s",
             attacker.name.c_str(), move_used.name.c_str(), dmg,
             defender.hp_cur == 0 ? " -- KO!" : "");
    return defender.hp_cur == 0 ? "ko" : "hit";
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Core battle turn
 *  Returns: "continue", "pc_win", "foe_win", "fled", "caught"
 * ═══════════════════════════════════════════════════════════════════════════ */
static const char *do_battle_turn(
    std::vector<GamePokemon> &pc_party,  int &pc_active,
    std::vector<GamePokemon> &foe_party, int &foe_active,
    bool is_wild, int &flee_attempts, Bag &bag,
    int &pc_action,   /* out: action chosen (for priority) */
    int &foe_move_idx /* out: foe move chosen */
)
{
    GamePokemon &pc_poke  = pc_party[pc_active];
    GamePokemon &foe_poke = foe_party[foe_active];

    char log[256] = "Your turn!";
    int menu_sel = 0;
    pc_action = -1;

    /* ── PC chooses action ── */
    while (pc_action < 0) {
        draw_battle_hud(pc_poke, foe_poke, log);
        draw_battle_menu(pc_poke, is_wild, bag, menu_sel);

        int ch = getch();
        if (ch == KEY_UP   && menu_sel > 0) menu_sel--;
        if (ch == KEY_DOWN && menu_sel < 3) menu_sel++;
        if (ch >= '1' && ch <= '4') menu_sel = ch - '1';

        if (ch == '\n' || ch == ' ' || ch == KEY_RIGHT ||
            (ch >= '1' && ch <= '4')) {

            /* ── Fight ── */
            if (menu_sel == 0) {
                if (pc_poke.num_moves == 0) {
                    snprintf(log, sizeof(log), "%s has no moves!", pc_poke.name.c_str());
                    continue;
                }
                int mv_sel = 0;
                while (true) {
                    draw_battle_hud(pc_poke, foe_poke, log);
                    draw_move_menu(pc_poke, mv_sel);
                    int c2 = getch();
                    if (c2 == 27) break;
                    if (c2 == KEY_UP   && mv_sel > 0) mv_sel--;
                    if (c2 == KEY_DOWN && mv_sel < pc_poke.num_moves-1) mv_sel++;
                    if (c2 >= '1' && c2 <= ('0'+pc_poke.num_moves)) mv_sel = c2-'1';
                    if (c2 == '\n' || c2 == ' ') { pc_action = mv_sel; break; }
                }

            /* ── Bag ── */
            } else if (menu_sel == 1) {
                int bag_sel = 0;
                while (true) {
                    draw_battle_hud(pc_poke, foe_poke, log);
                    draw_bag_menu(bag, is_wild, bag_sel);
                    int c2 = getch();
                    if (c2 == 27) break;
                    if (c2 == KEY_UP   && bag_sel > 0) bag_sel--;
                    if (c2 == KEY_DOWN && bag_sel < 2)  bag_sel++;
                    if (c2 >= '1' && c2 <= '3') bag_sel = c2-'1';
                    if (c2 == '\n' || c2 == ' ') {
                        /* Potion */
                        if (bag_sel == 0) {
                            if (bag.potions == 0) {
                                snprintf(log, sizeof(log), "No Potions left!");
                            } else {
                                /* Choose target */
                                (void)pc_active; /* target is active pokemon */
                                /* Simple: apply to active */
                                bag.potions--;
                                int heal = std::min(20, pc_poke.hp_max - pc_poke.hp_cur);
                                pc_poke.hp_cur += heal;
                                snprintf(log, sizeof(log), "Used Potion on %s! (+%d HP)",
                                         pc_poke.name.c_str(), heal);
                                pc_action = -99; /* bag action = skip fight phase */
                                break;
                            }
                        /* Revive */
                        } else if (bag_sel == 1) {
                            if (bag.revives == 0) {
                                snprintf(log, sizeof(log), "No Revives left!");
                            } else {
                                /* Choose KO'd pokemon */
                                int ko_idx = -1;
                                for (int i = 0; i < (int)pc_party.size(); i++)
                                    if (pc_party[i].hp_cur == 0) { ko_idx = i; break; }
                                if (ko_idx < 0) {
                                    snprintf(log, sizeof(log), "No KO'd Pokemon to revive!");
                                } else {
                                    bag.revives--;
                                    pc_party[ko_idx].hp_cur = pc_party[ko_idx].hp_max / 2;
                                    snprintf(log, sizeof(log), "Revived %s!",
                                             pc_party[ko_idx].name.c_str());
                                    pc_action = -99;
                                    break;
                                }
                            }
                        /* Pokeball */
                        } else if (bag_sel == 2) {
                            if (!is_wild) {
                                snprintf(log, sizeof(log), "Can't use Pokeballs in trainer battles!");
                            } else if (bag.pokeballs == 0) {
                                snprintf(log, sizeof(log), "No Pokeballs left!");
                            } else {
                                bag.pokeballs--;
                                pc_action = -98; /* catch action */
                                break;
                            }
                        }
                        break;
                    }
                }

            /* ── Pokemon switch ── */
            } else if (menu_sel == 2) {
                int sw_sel = pc_active;
                while (true) {
                    draw_battle_hud(pc_poke, foe_poke, log);
                    draw_party_menu(pc_party, sw_sel);
                    int c2 = getch();
                    if (c2 == 27) break;
                    if (c2 == KEY_UP   && sw_sel > 0) sw_sel--;
                    if (c2 == KEY_DOWN && sw_sel < (int)pc_party.size()-1) sw_sel++;
                    if (c2 >= '1' && c2 <= ('0'+(int)pc_party.size())) sw_sel = c2-'1';
                    if (c2 == '\n' || c2 == ' ') {
                        if (sw_sel == pc_active) {
                            snprintf(log, sizeof(log), "%s is already out!",
                                     pc_party[sw_sel].name.c_str());
                        } else if (pc_party[sw_sel].hp_cur <= 0) {
                            snprintf(log, sizeof(log), "%s is KO'd! Choose another.",
                                     pc_party[sw_sel].name.c_str());
                        } else {
                            pc_active = sw_sel;
                            snprintf(log, sizeof(log), "Go, %s!",
                                     pc_party[pc_active].name.c_str());
                            pc_action = -97; /* switch action */
                            break;
                        }
                    }
                }

            /* ── Run ── */
            } else if (menu_sel == 3) {
                if (!is_wild) {
                    snprintf(log, sizeof(log), "You can't run from a trainer battle!");
                } else {
                    flee_attempts++;
                    if (attempt_flee(pc_poke, foe_poke, flee_attempts)) {
                        draw_battle_hud(pc_poke, foe_poke, "Got away safely!");
                        refresh(); getch();
                        return "fled";
                    } else {
                        snprintf(log, sizeof(log), "Can't escape!");
                        pc_action = -96; /* failed flee = still uses turn */
                    }
                }
            }
        }
    }

    /* ── Catch attempt ── */
    if (pc_action == -98) {
        draw_battle_hud(pc_party[pc_active], foe_poke, "Threw a Pokeball...");
        refresh(); getch();
        if ((int)pc_party.size() < 6) {
            foe_poke.hp_cur = foe_poke.hp_max; /* restore before adding */
            pc_party.push_back(foe_poke);
            battle_msg("Gotcha! %s was caught!", foe_poke.name.c_str());
            return "caught";
        } else {
            battle_msg("%s broke free! (Party full)", foe_poke.name.c_str());
            return "fled"; /* wild flees after pokeball bounce */
        }
    }

    /* ── Non-fight actions: foe still attacks ── */
    if (pc_action < 0) {
        /* Foe attacks */
        foe_move_idx = ai_choose_move(foe_poke);
        if (foe_poke.num_moves > 0) {
            char flog[256];
            apply_attack(foe_poke, pc_party[pc_active], foe_poke.moves[foe_move_idx],
                         flog, sizeof(flog));
            draw_battle_hud(pc_party[pc_active], foe_poke, flog);
            refresh(); getch();
            if (pc_party[pc_active].hp_cur <= 0) {
                battle_msg("%s was KO'd!", pc_party[pc_active].name.c_str());
                /* Auto-switch to next alive */
                int nxt = first_alive(pc_party);
                if (nxt < 0) return "foe_win";
                pc_active = nxt;
            }
        }
        return "continue";
    }

    /* ── Both fight: determine order ── */
    foe_move_idx = ai_choose_move(foe_poke);

    /* Priority ordering */
    int pc_prio  = 0, foe_prio = 0;
    if (pc_action >= 0 && pc_action < pc_poke.num_moves) {
        for (const auto &mv : g_db.moves)
            if (mv.id == pc_poke.moves[pc_action].id) {
                pc_prio = (mv.priority != INT_MAX) ? mv.priority : 0;
                break;
            }
    }
    if (foe_poke.num_moves > 0) {
        for (const auto &mv : g_db.moves)
            if (mv.id == foe_poke.moves[foe_move_idx].id) {
                foe_prio = (mv.priority != INT_MAX) ? mv.priority : 0;
                break;
            }
    }

    /* PC goes first? */
    bool pc_first;
    if      (pc_prio > foe_prio) pc_first = true;
    else if (foe_prio > pc_prio) pc_first = false;
    else if (pc_poke.spd > foe_poke.spd) pc_first = true;
    else if (foe_poke.spd > pc_poke.spd) pc_first = false;
    else pc_first = (rand() % 2 == 0);

    auto do_turn = [&](GamePokemon &atk, GamePokemon &def,
                        const PokemonMove_ig &mv) -> bool /* returns true if def KO'd */ {
        if (atk.hp_cur <= 0) return false; /* already KO'd, forfeit */
        char flog[256];
        apply_attack(atk, def, mv, flog, sizeof(flog));
        draw_battle_hud(pc_party[pc_active], foe_party[foe_active], flog);
        refresh(); getch();
        return def.hp_cur <= 0;
    };

    if (pc_first) {
        /* PC attacks */
        if (pc_poke.num_moves > 0) {
            bool ko = do_turn(pc_poke, foe_poke, pc_poke.moves[pc_action]);
            if (ko) {
                battle_msg("%s fainted!", foe_poke.name.c_str());
                /* Find next alive foe */
                int nxt = -1;
                for (int i = foe_active+1; i < (int)foe_party.size(); i++)
                    if (foe_party[i].hp_cur > 0) { nxt = i; break; }
                if (nxt < 0 && all_ko(foe_party)) return "pc_win";
                if (nxt >= 0) {
                    foe_active = nxt;
                    battle_msg("Foe sends out %s!", foe_party[foe_active].name.c_str());
                }
                return "continue";
            }
        }
        /* Foe attacks back */
        if (foe_poke.num_moves > 0) {
            bool ko = do_turn(foe_poke, pc_party[pc_active],
                              foe_poke.moves[foe_move_idx]);
            if (ko) {
                battle_msg("%s fainted!", pc_party[pc_active].name.c_str());
                int nxt = first_alive(pc_party);
                if (nxt < 0) return "foe_win";
                pc_active = nxt;
                battle_msg("Go, %s!", pc_party[pc_active].name.c_str());
            }
        }
    } else {
        /* Foe attacks first */
        if (foe_poke.num_moves > 0) {
            bool ko = do_turn(foe_poke, pc_party[pc_active],
                              foe_poke.moves[foe_move_idx]);
            if (ko) {
                battle_msg("%s fainted!", pc_party[pc_active].name.c_str());
                int nxt = first_alive(pc_party);
                if (nxt < 0) return "foe_win";
                pc_active = nxt;
                battle_msg("Go, %s!", pc_party[pc_active].name.c_str());
            }
        }
        /* PC attacks back */
        if (pc_poke.num_moves > 0 && pc_party[pc_active].hp_cur > 0) {
            bool ko = do_turn(pc_party[pc_active], foe_poke,
                              pc_party[pc_active].moves[pc_action]);
            if (ko) {
                battle_msg("%s fainted!", foe_poke.name.c_str());
                int nxt = -1;
                for (int i = foe_active+1; i < (int)foe_party.size(); i++)
                    if (foe_party[i].hp_cur > 0) { nxt = i; break; }
                if (nxt < 0 && all_ko(foe_party)) return "pc_win";
                if (nxt >= 0) {
                    foe_active = nxt;
                    battle_msg("Foe sends out %s!", foe_party[foe_active].name.c_str());
                }
            }
        }
    }

    return "continue";
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Top-level battle entry points
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Wild pokemon battle. pc_party is the player's party (modified in place). */
static bool battle_wild(std::vector<GamePokemon> &pc_party, GamePokemon wild_poke,
                         Bag &bag)
{
    std::vector<GamePokemon> foe = { wild_poke };
    int pc_active  = first_alive(pc_party);
    int foe_active = 0;
    int flee_atts  = 0;

    if (pc_active < 0) return false; /* no pokemon */

    while (true) {
        int pa = 0, fm = 0;
        const char *res = do_battle_turn(pc_party, pc_active,
                                          foe, foe_active,
                                          true, flee_atts, bag, pa, fm);
        if      (strcmp(res, "pc_win")  == 0) { battle_msg("You won!"); return true; }
        else if (strcmp(res, "foe_win") == 0) { battle_msg("You blacked out!"); return false; }
        else if (strcmp(res, "fled")    == 0) return false;
        else if (strcmp(res, "caught")  == 0) return true;
    }
}

/* Trainer battle. npc_party modified in-place (KOs persist). */
static bool battle_trainer(std::vector<GamePokemon> &pc_party,
                             std::vector<GamePokemon> &npc_party,
                             const char *trainer_name, Bag &bag)
{
    int pc_active  = first_alive(pc_party);
    int foe_active = first_alive(npc_party);
    int flee_atts  = 0;

    if (pc_active < 0 || foe_active < 0) return false;

    /* Opening message */
    clear();
    attron(COLOR_PAIR(CP_MSG) | A_BOLD);
    mvprintw(0, 0, "%-80s", "  TRAINER BATTLE  ");
    attroff(COLOR_PAIR(CP_MSG) | A_BOLD);
    mvprintw(2, 2, "%s wants to fight!", trainer_name);
    mvprintw(3, 2, "%s sends out %s!", trainer_name, npc_party[foe_active].name.c_str());
    mvprintw(4, 2, "Go, %s!", pc_party[pc_active].name.c_str());
    mvprintw(6, 2, "(press any key)");
    refresh(); getch();

    while (true) {
        int pa = 0, fm = 0;
        const char *res = do_battle_turn(pc_party, pc_active,
                                          npc_party, foe_active,
                                          false, flee_atts, bag, pa, fm);
        if      (strcmp(res, "pc_win")  == 0) { battle_msg("You won the battle!"); return true; }
        else if (strcmp(res, "foe_win") == 0) { battle_msg("You blacked out!"); return false; }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Out-of-battle bag ('B' command)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void use_bag_outside_battle(std::vector<GamePokemon> &party, Bag &bag)
{
    int sel = 0;
    while (true) {
        clear();
        attron(COLOR_PAIR(CP_MSG) | A_BOLD);
        mvprintw(0, 0, "%-80s", "  Bag  (ESC=close)  ");
        attroff(COLOR_PAIR(CP_MSG) | A_BOLD);

        /* Party display */
        for (int i = 0; i < (int)party.size(); i++) {
            const auto &gp = party[i];
            bool ko = gp.hp_cur <= 0;
            if (ko) attron(COLOR_PAIR(CP_NPC_HIKER));
            mvprintw(2+i, 2, "[%d] %-20s Lv.%2d  HP:%3d/%-3d %s",
                     i+1, gp.name.c_str(), gp.level,
                     gp.hp_cur, gp.hp_max, ko ? "[KO]" : "");
            if (ko) attroff(COLOR_PAIR(CP_NPC_HIKER));
        }

        int base = (int)party.size() + 3;
        mvprintw(base,   2, "Items:");
        if (sel == 0) attron(A_REVERSE);
        mvprintw(base+1, 4, "[P] Use Potion  (x%d)  -- on which pokemon? (1-%d)",
                 bag.potions, (int)party.size());
        if (sel == 0) attroff(A_REVERSE);
        if (sel == 1) attron(A_REVERSE);
        mvprintw(base+2, 4, "[R] Use Revive  (x%d)  -- on which pokemon? (1-%d)",
                 bag.revives, (int)party.size());
        if (sel == 1) attroff(A_REVERSE);

        mvprintw(base+4, 2, "Arrow keys / P / R to select, then number to apply. ESC=exit.");
        refresh();

        int ch = getch();
        if (ch == 27) break;
        if (ch == KEY_UP && sel > 0) sel--;
        if (ch == KEY_DOWN && sel < 1) sel++;
        if (ch == 'p' || ch == 'P') sel = 0;
        if (ch == 'r' || ch == 'R') sel = 1;

        if (ch >= '1' && ch <= ('0'+(int)party.size())) {
            int idx = ch - '1';
            if (sel == 0) {
                /* Potion */
                if (bag.potions == 0) {
                    mvprintw(base+5, 2, "No Potions left!                    ");
                    refresh(); getch();
                } else if (party[idx].hp_cur <= 0) {
                    mvprintw(base+5, 2, "Can't use Potion on a fainted Pokemon!");
                    refresh(); getch();
                } else if (party[idx].hp_cur == party[idx].hp_max) {
                    mvprintw(base+5, 2, "%s already at full HP!              ", party[idx].name.c_str());
                    refresh(); getch();
                } else {
                    bag.potions--;
                    int heal = std::min(20, party[idx].hp_max - party[idx].hp_cur);
                    party[idx].hp_cur += heal;
                    mvprintw(base+5, 2, "%s healed %d HP!                   ", party[idx].name.c_str(), heal);
                    refresh(); getch();
                }
            } else {
                /* Revive */
                if (bag.revives == 0) {
                    mvprintw(base+5, 2, "No Revives left!                    ");
                    refresh(); getch();
                } else if (party[idx].hp_cur > 0) {
                    mvprintw(base+5, 2, "%s is not fainted!                  ", party[idx].name.c_str());
                    refresh(); getch();
                } else {
                    bag.revives--;
                    party[idx].hp_cur = party[idx].hp_max / 2;
                    mvprintw(base+5, 2, "%s was revived!                     ", party[idx].name.c_str());
                    refresh(); getch();
                }
            }
        }
    }
    clear();
}
