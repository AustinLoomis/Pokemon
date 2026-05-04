#pragma once
/*
 * pokedex.hpp
 * CSV parsing for all required pokedex data files.
 * Loaded from /share/cs327/pokedex or $HOME/.poke327/pokedex (in that order).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <string>
#include <vector>
#include <stdexcept>

/* ─── Search paths ──────────────────────────────────────────────────────── */
static const char *POKEDEX_SEARCH_PATHS[] = {
    "/share/cs327/pokedex/pokedex/data/csv",
    nullptr,   /* filled in at runtime from $HOME/.poke327/pokedex/pokedex/data/csv */
    nullptr
};

/* Returns the first directory that contains pokemon.csv, or "" on failure. */
static std::string find_pokedex_dir()
{
    /* Build home-based path */
    static char home_path[512];
    const char *home = getenv("HOME");
    if (home)
        snprintf(home_path, sizeof(home_path),
                 "%s/.poke327/pokedex/pokedex/data/csv", home);
    else
        home_path[0] = '\0';

    POKEDEX_SEARCH_PATHS[1] = home_path;

    for (int i = 0; i < 2; i++) {
        if (!POKEDEX_SEARCH_PATHS[i] || !POKEDEX_SEARCH_PATHS[i][0]) continue;
        char probe[600];
        snprintf(probe, sizeof(probe), "%s/pokemon.csv", POKEDEX_SEARCH_PATHS[i]);
        FILE *f = fopen(probe, "r");
        if (f) { fclose(f); return std::string(POKEDEX_SEARCH_PATHS[i]); }
    }
    return "";
}

/* ─── CSV token helpers ─────────────────────────────────────────────────── */

/* Parse one integer field; empty string → INT_MAX */
static int parse_int(const char *s)
{
    if (!s || s[0] == '\0') return INT_MAX;
    return atoi(s);
}

/* Tokenise a CSV line into fields (handles empty fields between commas).
 * Strips trailing newline. Fills into out[]. Returns number of fields. */
static int csv_split(char *line, char **out, int max_fields)
{
    /* Strip \r\n */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
        line[--len] = '\0';

    int n = 0;
    char *p = line;
    while (n < max_fields) {
        out[n++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    return n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Data structures
 * ═══════════════════════════════════════════════════════════════════════════ */

struct Pokemon {
    int id;
    std::string identifier;
    int species_id;
    int height;
    int weight;
    int base_experience;
    int order;
    int is_default;
};

struct Move {
    int id;
    std::string identifier;
    int generation_id;
    int type_id;
    int power;
    int pp;
    int accuracy;
    int priority;
    int target_id;
    int damage_class_id;
    int effect_id;
    int effect_chance;
    int contest_type_id;
    int contest_effect_id;
    int super_contest_effect_id;
};

struct PokemonMove {
    int pokemon_id;
    int version_group_id;
    int move_id;
    int pokemon_move_method_id;
    int level;
    int order;
};

struct PokemonSpecies {
    int id;
    std::string identifier;
    int generation_id;
    int evolves_from_species_id;
    int evolution_chain_id;
    int color_id;
    int shape_id;
    int habitat_id;
    int gender_rate;
    int capture_rate;
    int base_happiness;
    int is_baby;
    int hatch_counter;
    int has_gender_differences;
    int growth_rate_id;
    int forms_switchable;
    int is_legendary;
    int is_mythical;
    int order;
    int conquest_order;
};

struct Experience {
    int growth_rate_id;
    int level;
    int experience;
};

struct TypeName {
    int type_id;
    int local_language_id;
    std::string name;
};

struct PokemonStat {
    int pokemon_id;
    int stat_id;
    int base_stat;
    int effort;
};

struct Stat {
    int id;
    int damage_class_id;
    std::string identifier;
    int is_battle_only;
    int game_index;
};

struct PokemonType {
    int pokemon_id;
    int type_id;
    int slot;
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Parse functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static std::vector<Pokemon> parse_pokemon(const std::string &dir)
{
    std::vector<Pokemon> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/pokemon.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[1024]; char *fields[16];
    fgets(line, sizeof(line), f); /* skip header */
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 8);
        if (n < 8) continue;
        Pokemon p;
        p.id             = parse_int(fields[0]);
        p.identifier     = fields[1];
        p.species_id     = parse_int(fields[2]);
        p.height         = parse_int(fields[3]);
        p.weight         = parse_int(fields[4]);
        p.base_experience= parse_int(fields[5]);
        p.order          = parse_int(fields[6]);
        p.is_default     = parse_int(fields[7]);
        v.push_back(p);
    }
    fclose(f);
    return v;
}

static std::vector<Move> parse_moves(const std::string &dir)
{
    std::vector<Move> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/moves.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[1024]; char *fields[16];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 15);
        if (n < 1) continue;
        Move m;
        m.id                      = parse_int(fields[0]);
        m.identifier              = (n > 1) ? fields[1] : "";
        m.generation_id           = (n > 2)  ? parse_int(fields[2])  : INT_MAX;
        m.type_id                 = (n > 3)  ? parse_int(fields[3])  : INT_MAX;
        m.power                   = (n > 4)  ? parse_int(fields[4])  : INT_MAX;
        m.pp                      = (n > 5)  ? parse_int(fields[5])  : INT_MAX;
        m.accuracy                = (n > 6)  ? parse_int(fields[6])  : INT_MAX;
        m.priority                = (n > 7)  ? parse_int(fields[7])  : INT_MAX;
        m.target_id               = (n > 8)  ? parse_int(fields[8])  : INT_MAX;
        m.damage_class_id         = (n > 9)  ? parse_int(fields[9])  : INT_MAX;
        m.effect_id               = (n > 10) ? parse_int(fields[10]) : INT_MAX;
        m.effect_chance           = (n > 11) ? parse_int(fields[11]) : INT_MAX;
        m.contest_type_id         = (n > 12) ? parse_int(fields[12]) : INT_MAX;
        m.contest_effect_id       = (n > 13) ? parse_int(fields[13]) : INT_MAX;
        m.super_contest_effect_id = (n > 14) ? parse_int(fields[14]) : INT_MAX;
        v.push_back(m);
    }
    fclose(f);
    return v;
}

static std::vector<PokemonMove> parse_pokemon_moves(const std::string &dir)
{
    std::vector<PokemonMove> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/pokemon_moves.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[512]; char *fields[8];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 6);
        if (n < 1) continue;
        PokemonMove pm;
        pm.pokemon_id             = parse_int(fields[0]);
        pm.version_group_id       = (n > 1) ? parse_int(fields[1]) : INT_MAX;
        pm.move_id                = (n > 2) ? parse_int(fields[2]) : INT_MAX;
        pm.pokemon_move_method_id = (n > 3) ? parse_int(fields[3]) : INT_MAX;
        pm.level                  = (n > 4) ? parse_int(fields[4]) : INT_MAX;
        pm.order                  = (n > 5) ? parse_int(fields[5]) : INT_MAX;
        v.push_back(pm);
    }
    fclose(f);
    return v;
}

static std::vector<PokemonSpecies> parse_pokemon_species(const std::string &dir)
{
    std::vector<PokemonSpecies> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/pokemon_species.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[1024]; char *fields[24];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 20);
        if (n < 1) continue;
        PokemonSpecies ps;
        ps.id                      = parse_int(fields[0]);
        ps.identifier              = (n > 1)  ? fields[1]           : "";
        ps.generation_id           = (n > 2)  ? parse_int(fields[2])  : INT_MAX;
        ps.evolves_from_species_id = (n > 3)  ? parse_int(fields[3])  : INT_MAX;
        ps.evolution_chain_id      = (n > 4)  ? parse_int(fields[4])  : INT_MAX;
        ps.color_id                = (n > 5)  ? parse_int(fields[5])  : INT_MAX;
        ps.shape_id                = (n > 6)  ? parse_int(fields[6])  : INT_MAX;
        ps.habitat_id              = (n > 7)  ? parse_int(fields[7])  : INT_MAX;
        ps.gender_rate             = (n > 8)  ? parse_int(fields[8])  : INT_MAX;
        ps.capture_rate            = (n > 9)  ? parse_int(fields[9])  : INT_MAX;
        ps.base_happiness          = (n > 10) ? parse_int(fields[10]) : INT_MAX;
        ps.is_baby                 = (n > 11) ? parse_int(fields[11]) : INT_MAX;
        ps.hatch_counter           = (n > 12) ? parse_int(fields[12]) : INT_MAX;
        ps.has_gender_differences  = (n > 13) ? parse_int(fields[13]) : INT_MAX;
        ps.growth_rate_id          = (n > 14) ? parse_int(fields[14]) : INT_MAX;
        ps.forms_switchable        = (n > 15) ? parse_int(fields[15]) : INT_MAX;
        ps.is_legendary            = (n > 16) ? parse_int(fields[16]) : INT_MAX;
        ps.is_mythical             = (n > 17) ? parse_int(fields[17]) : INT_MAX;
        ps.order                   = (n > 18) ? parse_int(fields[18]) : INT_MAX;
        ps.conquest_order          = (n > 19) ? parse_int(fields[19]) : INT_MAX;
        v.push_back(ps);
    }
    fclose(f);
    return v;
}

static std::vector<Experience> parse_experience(const std::string &dir)
{
    std::vector<Experience> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/experience.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[256]; char *fields[4];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 3);
        if (n < 3) continue;
        Experience e;
        e.growth_rate_id = parse_int(fields[0]);
        e.level          = parse_int(fields[1]);
        e.experience     = parse_int(fields[2]);
        v.push_back(e);
    }
    fclose(f);
    return v;
}

static std::vector<TypeName> parse_type_names(const std::string &dir)
{
    std::vector<TypeName> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/type_names.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[256]; char *fields[4];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 3);
        if (n < 3) continue;
        int lang = parse_int(fields[1]);
        if (lang != 9) continue;   /* English only */
        TypeName tn;
        tn.type_id           = parse_int(fields[0]);
        tn.local_language_id = lang;
        tn.name              = fields[2];
        v.push_back(tn);
    }
    fclose(f);
    return v;
}

static std::vector<PokemonStat> parse_pokemon_stats(const std::string &dir)
{
    std::vector<PokemonStat> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/pokemon_stats.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[256]; char *fields[4];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 4);
        if (n < 4) continue;
        PokemonStat ps;
        ps.pokemon_id = parse_int(fields[0]);
        ps.stat_id    = parse_int(fields[1]);
        ps.base_stat  = parse_int(fields[2]);
        ps.effort     = parse_int(fields[3]);
        v.push_back(ps);
    }
    fclose(f);
    return v;
}

static std::vector<Stat> parse_stats(const std::string &dir)
{
    std::vector<Stat> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/stats.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[256]; char *fields[6];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 5);
        if (n < 1) continue;
        Stat s;
        s.id             = parse_int(fields[0]);
        s.damage_class_id= (n > 1) ? parse_int(fields[1]) : INT_MAX;
        s.identifier     = (n > 2) ? fields[2] : "";
        s.is_battle_only = (n > 3) ? parse_int(fields[3]) : INT_MAX;
        s.game_index     = (n > 4) ? parse_int(fields[4]) : INT_MAX;
        v.push_back(s);
    }
    fclose(f);
    return v;
}

static std::vector<PokemonType> parse_pokemon_types(const std::string &dir)
{
    std::vector<PokemonType> v;
    char path[600];
    snprintf(path, sizeof(path), "%s/pokemon_types.csv", dir.c_str());
    FILE *f = fopen(path, "r");
    if (!f) throw std::runtime_error(std::string("Cannot open ") + path);

    char line[256]; char *fields[4];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int n = csv_split(line, fields, 3);
        if (n < 3) continue;
        PokemonType pt;
        pt.pokemon_id = parse_int(fields[0]);
        pt.type_id    = parse_int(fields[1]);
        pt.slot       = parse_int(fields[2]);
        v.push_back(pt);
    }
    fclose(f);
    return v;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Print helpers (do not print INT_MAX — print blank instead)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define PINT(x) ((x) == INT_MAX ? "" : std::to_string(x).c_str())
/* For printf we need a helper since PINT returns a pointer to a temporary */
static void print_int(int v) { if (v != INT_MAX) printf("%d", v); }
static void print_sep()      { printf(","); }
static void print_nl()       { printf("\n"); }

static void print_pokemon(const std::vector<Pokemon> &v)
{
    printf("id,identifier,species_id,height,weight,base_experience,order,is_default\n");
    for (const auto &p : v) {
        print_int(p.id);             print_sep();
        printf("%s",p.identifier.c_str()); print_sep();
        print_int(p.species_id);     print_sep();
        print_int(p.height);         print_sep();
        print_int(p.weight);         print_sep();
        print_int(p.base_experience);print_sep();
        print_int(p.order);          print_sep();
        print_int(p.is_default);     print_nl();
    }
}

static void print_moves(const std::vector<Move> &v)
{
    printf("id,identifier,generation_id,type_id,power,pp,accuracy,priority,"
           "target_id,damage_class_id,effect_id,effect_chance,"
           "contest_type_id,contest_effect_id,super_contest_effect_id\n");
    for (const auto &m : v) {
        print_int(m.id);                      print_sep();
        printf("%s",m.identifier.c_str());    print_sep();
        print_int(m.generation_id);           print_sep();
        print_int(m.type_id);                 print_sep();
        print_int(m.power);                   print_sep();
        print_int(m.pp);                      print_sep();
        print_int(m.accuracy);                print_sep();
        print_int(m.priority);                print_sep();
        print_int(m.target_id);               print_sep();
        print_int(m.damage_class_id);         print_sep();
        print_int(m.effect_id);               print_sep();
        print_int(m.effect_chance);           print_sep();
        print_int(m.contest_type_id);         print_sep();
        print_int(m.contest_effect_id);       print_sep();
        print_int(m.super_contest_effect_id); print_nl();
    }
}

static void print_pokemon_moves(const std::vector<PokemonMove> &v)
{
    printf("pokemon_id,version_group_id,move_id,pokemon_move_method_id,level,order\n");
    for (const auto &pm : v) {
        print_int(pm.pokemon_id);             print_sep();
        print_int(pm.version_group_id);       print_sep();
        print_int(pm.move_id);                print_sep();
        print_int(pm.pokemon_move_method_id); print_sep();
        print_int(pm.level);                  print_sep();
        print_int(pm.order);                  print_nl();
    }
}

static void print_pokemon_species(const std::vector<PokemonSpecies> &v)
{
    printf("id,identifier,generation_id,evolves_from_species_id,evolution_chain_id,"
           "color_id,shape_id,habitat_id,gender_rate,capture_rate,base_happiness,"
           "is_baby,hatch_counter,has_gender_differences,growth_rate_id,"
           "forms_switchable,is_legendary,is_mythical,order,conquest_order\n");
    for (const auto &ps : v) {
        print_int(ps.id);                      print_sep();
        printf("%s",ps.identifier.c_str());    print_sep();
        print_int(ps.generation_id);           print_sep();
        print_int(ps.evolves_from_species_id); print_sep();
        print_int(ps.evolution_chain_id);      print_sep();
        print_int(ps.color_id);                print_sep();
        print_int(ps.shape_id);                print_sep();
        print_int(ps.habitat_id);              print_sep();
        print_int(ps.gender_rate);             print_sep();
        print_int(ps.capture_rate);            print_sep();
        print_int(ps.base_happiness);          print_sep();
        print_int(ps.is_baby);                 print_sep();
        print_int(ps.hatch_counter);           print_sep();
        print_int(ps.has_gender_differences);  print_sep();
        print_int(ps.growth_rate_id);          print_sep();
        print_int(ps.forms_switchable);        print_sep();
        print_int(ps.is_legendary);            print_sep();
        print_int(ps.is_mythical);             print_sep();
        print_int(ps.order);                   print_sep();
        print_int(ps.conquest_order);          print_nl();
    }
}

static void print_experience(const std::vector<Experience> &v)
{
    printf("growth_rate_id,level,experience\n");
    for (const auto &e : v) {
        print_int(e.growth_rate_id); print_sep();
        print_int(e.level);          print_sep();
        print_int(e.experience);     print_nl();
    }
}

static void print_type_names(const std::vector<TypeName> &v)
{
    printf("type_id,local_language_id,name\n");
    for (const auto &tn : v) {
        print_int(tn.type_id);           print_sep();
        print_int(tn.local_language_id); print_sep();
        printf("%s",tn.name.c_str());    print_nl();
    }
}

static void print_pokemon_stats(const std::vector<PokemonStat> &v)
{
    printf("pokemon_id,stat_id,base_stat,effort\n");
    for (const auto &ps : v) {
        print_int(ps.pokemon_id); print_sep();
        print_int(ps.stat_id);    print_sep();
        print_int(ps.base_stat);  print_sep();
        print_int(ps.effort);     print_nl();
    }
}

static void print_stats(const std::vector<Stat> &v)
{
    printf("id,damage_class_id,identifier,is_battle_only,game_index\n");
    for (const auto &s : v) {
        print_int(s.id);               print_sep();
        print_int(s.damage_class_id);  print_sep();
        printf("%s",s.identifier.c_str()); print_sep();
        print_int(s.is_battle_only);   print_sep();
        print_int(s.game_index);       print_nl();
    }
}

static void print_pokemon_types(const std::vector<PokemonType> &v)
{
    printf("pokemon_id,type_id,slot\n");
    for (const auto &pt : v) {
        print_int(pt.pokemon_id); print_sep();
        print_int(pt.type_id);    print_sep();
        print_int(pt.slot);       print_nl();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Top-level dispatcher: parse and print one file by name
 * ═══════════════════════════════════════════════════════════════════════════ */
static int run_pokedex_dump(const char *table_name)
{
    std::string dir = find_pokedex_dir();
    if (dir.empty()) {
        fprintf(stderr,
            "Error: pokedex database not found.\n"
            "Searched:\n"
            "  /share/cs327/pokedex/pokedex/data/csv\n"
            "  $HOME/.poke327/pokedex/pokedex/data/csv\n"
            "Copy the database to one of these locations and try again.\n");
        return 1;
    }

    try {
        if      (strcmp(table_name, "pokemon")         == 0) print_pokemon(parse_pokemon(dir));
        else if (strcmp(table_name, "moves")           == 0) print_moves(parse_moves(dir));
        else if (strcmp(table_name, "pokemon_moves")   == 0) print_pokemon_moves(parse_pokemon_moves(dir));
        else if (strcmp(table_name, "pokemon_species") == 0) print_pokemon_species(parse_pokemon_species(dir));
        else if (strcmp(table_name, "experience")      == 0) print_experience(parse_experience(dir));
        else if (strcmp(table_name, "type_names")      == 0) print_type_names(parse_type_names(dir));
        else if (strcmp(table_name, "pokemon_stats")   == 0) print_pokemon_stats(parse_pokemon_stats(dir));
        else if (strcmp(table_name, "stats")           == 0) print_stats(parse_stats(dir));
        else if (strcmp(table_name, "pokemon_types")   == 0) print_pokemon_types(parse_pokemon_types(dir));
        else {
            fprintf(stderr,
                "Unknown table '%s'.\n"
                "Valid options: pokemon, moves, pokemon_moves, pokemon_species,\n"
                "               experience, type_names, pokemon_stats, stats, pokemon_types\n",
                table_name);
            return 1;
        }
    } catch (const std::exception &ex) {
        fprintf(stderr, "Error: %s\n", ex.what());
        return 1;
    }
    return 0;
}
