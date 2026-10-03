#include "game.h"
#include "storage.h"
#include "damage_log.h"
#include "esp_random.h"
#include "net_sync.h"
// Forward declarations for UI refresh (defined in screen modules)
extern void refresh_player_ui(void);
extern void refresh_rename_ui(void);
extern void select_kick_timer(void);

// ---------- state ----------
int player_damage[MAX_DISPLAY_PLAYERS] = {0};
int player_base_hp[MAX_DISPLAY_PLAYERS] = {
    DEFAULT_BASE_HP, DEFAULT_BASE_HP, DEFAULT_BASE_HP, DEFAULT_BASE_HP
};
bool player_force[MAX_DISPLAY_PLAYERS] = {false};
int initiative_player = -1;
bool player_selected[MAX_DISPLAY_PLAYERS] = {false};
char player_names[MAX_DISPLAY_PLAYERS][16] = {
    "P1", "P2", "P3", "P4"
};
int menu_player = 0;
int pending_damage_delta = 0;
bool damage_preview_active = false;
int hp_edit_value = DEFAULT_BASE_HP;
bool player_eliminated[MAX_DISPLAY_PLAYERS] = {false};
/* Conceded players (manual elimination) tracked apart from auto-elimination,
   so an undo that recomputes auto conditions can't revive someone who
   manually conceded while their base still stands. */
static bool player_manually_eliminated[MAX_DISPLAY_PLAYERS] = {false};

/* The damage commit that destroyed a base, kept so the eliminated-player
   menu can undo exactly that commit. */
typedef struct {
    bool valid;
    int delta;
} elimination_action_t;

static elimination_action_t elimination_action[MAX_DISPLAY_PLAYERS] = {{0}};

static lv_timer_t *damage_preview_timer = NULL;

/* Per-player Lamport versions for Table Sync, scoped by a game epoch.
   Every local commit bumps the touched player's version and broadcasts
   a full state snapshot; adopting a remote block adopts its version.
   The epoch dominates the comparison (see net_sync_apply_state), so
   versions from different games are never compared against each other.
   uint16 wrap is handled with serial arithmetic. */
static uint16_t game_epoch = 0;
static uint16_t player_version[MAX_DISPLAY_PLAYERS] = {0};
static uint16_t names_version = 0;

static void clear_player_elimination_action(int player);

static void net_sync_commit_player(int player)
{
    player_version[player]++;
    net_sync_send_state();
}

void net_sync_commit_names(void)
{
    names_version++;
    net_sync_send_names();
}

void net_sync_begin_game(void)
{
    int i;
    game_epoch++;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) player_version[i] = 1;
    /* Names outlive game resets, so a mid-session reset leaves the
       roster version alone. A fresh host must still seed it above a
       joiner's zero, or the joiner's leftover roster could win the
       first tie. */
    if (names_version == 0) names_version = 1;
}

void net_sync_reset_versions(void)
{
    int i;
    game_epoch = 0;
    memset(player_version, 0, sizeof(player_version));
    names_version = 0;
    /* Joining a table: elimination-undo bookkeeping and the event log
       refer to a game this device is leaving behind; replaying either
       against adopted state would corrupt damage and mirror the
       corruption table-wide. */
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        clear_player_elimination_action(i);
    damage_log_reset();
}

static bool valid_player(int player)
{
    return player >= 0 && player < MAX_DISPLAY_PLAYERS;
}

bool base_is_destroyed(int player)
{
    if (!valid_player(player)) return false;
    return player_damage[player] >= player_base_hp[player];
}

static void clear_player_elimination_action(int player)
{
    if (!valid_player(player)) return;
    elimination_action[player].valid = false;
}

static void set_player_elimination_action(int player, int delta)
{
    if (!valid_player(player)) return;
    elimination_action[player].valid = true;
    elimination_action[player].delta = delta;
}

bool elimination_action_available(int player)
{
    if (!valid_player(player)) return false;
    return elimination_action[player].valid;
}

void undo_elimination_action(int player)
{
    if (!elimination_action_available(player)) return;

    elimination_action_t action = elimination_action[player];
    clear_player_elimination_action(player);
    undo_damage_change(player, action.delta);

    /* Drop the log entry that destroyed the base so the same event can't
       be undone a second time from the Event Log. The destroying event is
       the newest one for this player (eliminated players take no more). */
    damage_log_remove_last_for(player);
}

void check_player_elimination(int player)
{
    if (!valid_player(player)) return;
    bool was_eliminated = player_eliminated[player];
    bool now_eliminated = false;

    /* Elimination is a multiplayer concept: with a single tracked player
       there is no eliminated-menu route in the 1p UI, so eliminating
       player 0 would brick the counter until reset. */
    if (nvs_get_auto_eliminate() && nvs_get_players_to_track() > 1) {
        now_eliminated = base_is_destroyed(player);
    }

    if (player_manually_eliminated[player]) {
        now_eliminated = true;
    }

    player_eliminated[player] = now_eliminated;
    if (!now_eliminated) {
        clear_player_elimination_action(player);
    } else if (player_selected[player]) {
        /* An eliminated player is no longer a damage target: drop it
           from the selection so the knob doesn't preview onto a dead panel
           that can't be tapped to deselect. */
        player_selected[player] = false;
        select_kick_timer();
    }

    if (was_eliminated != now_eliminated) {
        refresh_player_ui();
    }
}

void manual_eliminate_player(int player)
{
    if (!valid_player(player)) return;
    if (player_eliminated[player]) return;
    /* Same solo-mode exemption as check_player_elimination. */
    if (nvs_get_players_to_track() <= 1) return;
    player_eliminated[player] = true;
    player_manually_eliminated[player] = true;
    clear_player_elimination_action(player);
    if (player_selected[player]) {
        player_selected[player] = false;
        select_kick_timer();
    }
    net_sync_commit_player(player);
    refresh_player_ui();
}

void manual_uneliminate_player(int player)
{
    if (!valid_player(player)) return;
    if (!player_eliminated[player]) return;
    player_eliminated[player] = false;
    player_manually_eliminated[player] = false;
    clear_player_elimination_action(player);
    /* A remotely-caused elimination arrives with no local
       elimination_action to undo, so revival must also pull damage
       just below the base's HP — otherwise the base comes back
       destroyed and re-dies on the next touch. Only when
       auto-elimination would actually re-fire (same gate as
       check_player_elimination): with it off, a destroyed base is a
       legitimate alive state that must not be rewritten. */
    if (nvs_get_auto_eliminate() && nvs_get_players_to_track() > 1) {
        if (base_is_destroyed(player))
            player_damage[player] = player_base_hp[player] - 1;
    }
    net_sync_commit_player(player);
    refresh_player_ui();
}

// ---------- player colors ----------
static const uint32_t player_color_table[MAX_DISPLAY_PLAYERS][VIB_COUNT] = {
    /*  dim        mid        vivid  */
    {0x024D3A, 0x06D6A0, 0x66FFD9},  /* P1 green  (bottom-left) */
    {0x2A0A4D, 0x7B1FE0, 0x9C4DFF},  /* P2 purple (top-left)    */
    {0x0A3A4D, 0x29B6F6, 0x4FC3F7},  /* P3 blue   (top-right)   */
    {0x4D4400, 0xFFD600, 0xFFEA61},  /* P4 yellow (bottom-right) */
};

// ---------- custom color palette (18 colors) ----------
static const uint32_t custom_color_table[CUSTOM_COLOR_COUNT][VIB_COUNT] = {
    /*  dim        mid        vivid  */
    {0x024D3A, 0x06D6A0, 0x66FFD9},  /*  0 Green  */
    {0x2A0A4D, 0x7B1FE0, 0x9C4DFF},  /*  1 Purple */
    {0x0A3A4D, 0x29B6F6, 0x4FC3F7},  /*  2 Blue   */
    {0x4D4400, 0xFFD600, 0xFFEA61},  /*  3 Yellow */
    {0x4D1C1C, 0xF44336, 0xFF5252},  /*  4 Red    */
    {0x4D3300, 0xFF9800, 0xFFB74D},  /*  5 Orange */
    {0x004D4D, 0x00BCD4, 0x4DD0E1},  /*  6 Cyan   */
    {0x3D0A4D, 0xE040FB, 0xEA80FC},  /*  7 Pink   */
    {0x2D4D00, 0x8BC34A, 0xAED581},  /*  8 Lime   */
    {0x0A0A4D, 0x3F51B5, 0x7986CB},  /*  9 Indigo */
    {0x4D0A2A, 0xE91E63, 0xF06292},  /* 10 Rose   */
    {0x333333, 0xAAAAAA, 0xFFFFFF},  /* 11 White  */
    {0x00332E, 0x009688, 0x4DB6AC},  /* 12 Teal   */
    {0x4D3800, 0xFFC107, 0xFFD54F},  /* 13 Amber  */
    {0x2E1F16, 0x795548, 0xA1887F},  /* 14 Brown  */
    {0x1E2D33, 0x607D8B, 0x90A4AE},  /* 15 Gray   */
    {0x1A3D1A, 0xA5D6A7, 0x4CAF50},  /* 16 Sage   */
    {0x0A0A0A, 0x303030, 0x505050},  /* 17 Black  */
};

static const char *custom_color_names[CUSTOM_COLOR_COUNT] = {
    "Green", "Purple", "Blue", "Yellow",
    "Red", "Orange", "Cyan", "Pink",
    "Lime", "Indigo", "Rose", "White",
    "Teal", "Amber", "Brown", "Gray",
    "Sage", "Black",
};

// ---------- per-player color state (runtime only, lost on reboot) ----------
int player_color_index[MAX_DISPLAY_PLAYERS] = {0, 1, 2, 3};
bool player_hp_color[MAX_DISPLAY_PLAYERS] = {false, false, false, false};
bool player_has_override[MAX_DISPLAY_PLAYERS] = {false, false, false, false};

lv_color_t get_player_color_vib(int index, int vibrancy)
{
    if (index < 0 || index >= MAX_DISPLAY_PLAYERS) return lv_color_hex(0x303030);
    if (vibrancy < 0 || vibrancy >= VIB_COUNT) vibrancy = VIB_MID;
    return lv_color_hex(player_color_table[index][vibrancy]);
}

lv_color_t get_player_base_color(int index)
{
    return get_player_color_vib(index, VIB_MID);
}

lv_color_t get_player_active_color(int index)
{
    return get_player_color_vib(index, VIB_VIV);
}

lv_color_t get_player_text_color(int index)
{
    lv_color_t bg = get_player_base_color(index);
    return color_is_light(bg) ? lv_color_black() : lv_color_white();
}

/* Preview color for a pending damage change: red for damage, green for
   healing, with contrast fallbacks on the green and yellow seats. */
lv_color_t get_player_preview_color(int index, int damage_delta)
{
    bool hurt = damage_delta > 0;

    if (index == 3) {
        return hurt ? lv_color_hex(0x7A1020) : lv_color_hex(0x215A2A);
    }
    if (index == 0) {
        return hurt ? lv_palette_main(LV_PALETTE_RED) : lv_color_white();
    }
    return hurt ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
}

lv_color_t get_custom_color_vib(int index, int vibrancy)
{
    if (index < 0 || index >= CUSTOM_COLOR_COUNT) index = 0;
    if (vibrancy < 0 || vibrancy >= VIB_COUNT) vibrancy = VIB_MID;
    return lv_color_hex(custom_color_table[index][vibrancy]);
}

const char *get_custom_color_name(int index)
{
    if (index < 0 || index >= CUSTOM_COLOR_COUNT) index = 0;
    return custom_color_names[index];
}

lv_color_t get_effective_player_color(int player_i, int color_i, int vibrancy)
{
    /* Per-player override takes precedence over global mode */
    if (player_has_override[player_i]) {
        if (player_hp_color[player_i]) {
            int tier = get_hp_tier(player_damage[player_i], player_base_hp[player_i]);
            return get_hp_color_vib(tier, vibrancy);
        }
        return get_custom_color_vib(player_color_index[player_i], vibrancy);
    }

    /* No override: use global mode */
    if (nvs_get_color_mode() == COLOR_MODE_HP) {
        int tier = get_hp_tier(player_damage[player_i], player_base_hp[player_i]);
        return get_hp_color_vib(tier, vibrancy);
    }

    /* COLOR_MODE_PLAYER: use position color */
    return get_player_color_vib(color_i, vibrancy);
}

// ---------- player selection set ----------
int selection_count(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        if (player_selected[i]) n++;
    return n;
}

bool is_player_selected(int player)
{
    if (!valid_player(player)) return false;
    return player_selected[player];
}

void selection_clear(void)
{
    int i;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        player_selected[i] = false;
}

void selection_toggle(int player)
{
    if (!valid_player(player)) return;
    if (player_eliminated[player]) return;
    player_selected[player] = !player_selected[player];
}

void selection_set_single(int player)
{
    selection_clear();
    if (!valid_player(player)) return;
    if (player_eliminated[player]) return;
    player_selected[player] = true;
}

/* The single entry point for committing a damage change as a game event:
   log + clamp + elimination-undo action + elimination check. Every path
   that applies damage must use this so the elimination machinery can't
   be bypassed. */
void apply_damage_delta(int player, int delta)
{
    int next;

    if (!valid_player(player)) return;
    if (player_eliminated[player]) return;
    next = clamp_damage(player_damage[player] + delta);
    delta = next - player_damage[player];
    if (delta == 0) return;

    damage_log_add(player, delta);
    player_damage[player] = next;
    if (base_is_destroyed(player)) {
        set_player_elimination_action(player, delta);
    }
    check_player_elimination(player);
    net_sync_commit_player(player);
}

// ---------- damage preview ----------
void damage_preview_commit_cb(lv_timer_t *timer)
{
    int track = nvs_get_players_to_track();
    int i;

    (void)timer;

    if (!damage_preview_active || selection_count() == 0) {
        pending_damage_delta = 0;
        damage_preview_active = false;
        if (damage_preview_timer != NULL) {
            lv_timer_pause(damage_preview_timer);
        }
        return;
    }

    for (i = 0; i < track && i < MAX_DISPLAY_PLAYERS; i++) {
        if (!player_selected[i]) continue;
        apply_damage_delta(i, pending_damage_delta);
    }
    pending_damage_delta = 0;
    damage_preview_active = false;
    if (damage_preview_timer != NULL) {
        lv_timer_pause(damage_preview_timer);
    }
    /* Applying a damage change ends the operation: in multi-select mode
       clear the selection so the next tap starts a fresh selection.
       Otherwise sequential per-player damage keeps stacking players into
       the set. */
    if (nvs_get_multi_select()) {
        selection_clear();
        select_kick_timer();
    }
    refresh_player_ui();
}

// ---------- damage changes ----------
void change_player_damage(int delta)
{
    /* The shared delta applies to every currently-selected player. Clamp it
       to the headroom of the selected set so the previewed totals always
       equal what the commit will store and overshoot detents at 0 or the
       damage cap are absorbed instead of accumulating. */
    int track = nvs_get_players_to_track();
    int max_up = DAMAGE_MAX;
    int min_down = -DAMAGE_MAX;
    int i;

    /* The roulette walks the selection every tick, so a delta dialed
       mid-spin would land on whichever player the wheel stops at. */
    if (player_selection_animation_active()) return;

    if (selection_count() == 0) return;

    select_kick_timer();

    for (i = 0; i < track && i < MAX_DISPLAY_PLAYERS; i++) {
        if (!player_selected[i] || player_eliminated[i]) continue;
        if (DAMAGE_MAX - player_damage[i] < max_up) max_up = DAMAGE_MAX - player_damage[i];
        if (DAMAGE_MIN - player_damage[i] > min_down) min_down = DAMAGE_MIN - player_damage[i];
    }

    pending_damage_delta += delta;
    if (pending_damage_delta > max_up) pending_damage_delta = max_up;
    if (pending_damage_delta < min_down) pending_damage_delta = min_down;
    damage_preview_active = (pending_damage_delta != 0);

    if (damage_preview_timer != NULL) {
        if (damage_preview_active) {
            lv_timer_reset(damage_preview_timer);
            lv_timer_resume(damage_preview_timer);
        } else {
            lv_timer_pause(damage_preview_timer);
        }
    }

    refresh_player_ui();
}

// ---------- undo ----------
void undo_damage_change(int player, int delta)
{
    if (!valid_player(player)) return;

    player_damage[player] = clamp_damage(player_damage[player] - delta);
    check_player_elimination(player);
    net_sync_commit_player(player);
    refresh_player_ui();
}

// ---------- base HP ----------
void set_all_base_hp(int hp)
{
    int i;

    hp = clamp_base_hp(hp);
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        player_base_hp[i] = hp;
        nvs_set_player_base_hp(i, hp);
        check_player_elimination(i);
    }
}

void begin_hp_edit(int player)
{
    if (!valid_player(player)) return;
    menu_player = player;
    hp_edit_value = player_base_hp[player];
}

void change_hp_edit(int delta)
{
    hp_edit_value = clamp_base_hp(hp_edit_value + delta);
}

/* Knob turns since the editor opened, i.e. the change apply_hp_edit()
   will commit against the player's live base HP. */
int hp_edit_pending_delta(void)
{
    if (!valid_player(menu_player)) return 0;
    return hp_edit_value - player_base_hp[menu_player];
}

void apply_hp_edit(void)
{
    int player = menu_player;

    if (!valid_player(player)) return;
    hp_edit_value = clamp_base_hp(hp_edit_value);
    if (player_base_hp[player] == hp_edit_value) return;

    player_base_hp[player] = hp_edit_value;
    nvs_set_player_base_hp(player, hp_edit_value);
    settings_save();
    check_player_elimination(player);
    net_sync_commit_player(player);
    refresh_player_ui();
}

// ---------- tokens ----------
/* Only one player holds the initiative. Taking it moves it from the
   previous holder; tapping again as the holder hands it back to nobody. */
void toggle_initiative(int player)
{
    int previous = initiative_player;

    if (!valid_player(player)) return;
    initiative_player = (previous == player) ? -1 : player;

    player_version[player]++;
    if (valid_player(previous) && previous != player) player_version[previous]++;
    net_sync_send_state();
    refresh_player_ui();
}

void toggle_force(int player)
{
    if (!valid_player(player)) return;
    player_force[player] = !player_force[player];
    net_sync_commit_player(player);
    refresh_player_ui();
}

// ---------- reset ----------
/* A new game clears damage and tokens. Base HP stays, since players
   usually keep their bases across the games of a match. */
void game_reset(void)
{
    int i;

    damage_log_reset();

    pending_damage_delta = 0;
    selection_clear();
    damage_preview_active = false;

    memset(player_damage, 0, sizeof(player_damage));
    memset(player_force, 0, sizeof(player_force));
    initiative_player = -1;
    menu_player = 0;
    memset(player_eliminated, 0, sizeof(player_eliminated));
    memset(player_manually_eliminated, 0, sizeof(player_manually_eliminated));
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) clear_player_elimination_action(i);

    /* A reset starts a new game: bump the epoch (which outranks any
       version drift a strayed device accumulated) and broadcast once. */
    net_sync_begin_game();
    net_sync_send_state();

    if (damage_preview_timer != NULL) {
        lv_timer_pause(damage_preview_timer);
    }
}

// ---------- init ----------
void game_init(void)
{
    int i;

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        player_damage[i] = 0;
        player_base_hp[i] = nvs_get_player_base_hp(i);
    }

    damage_preview_timer = lv_timer_create(damage_preview_commit_cb, 3000, NULL);
    if (damage_preview_timer != NULL) {
        lv_timer_pause(damage_preview_timer);
    }
}

// ---------- player selection animation ----------
/* Roulette that picks who starts with the initiative. */
static lv_timer_t *player_select_anim_timer = NULL;
static int player_select_anim_steps = 0;
static int player_select_anim_period = 0;
static int roulette_idx = 0;

static void player_select_anim_cb(lv_timer_t *timer)
{
    int track = nvs_get_players_to_track();
    (void)timer;

    if (track <= 1) {
        lv_timer_pause(player_select_anim_timer);
        return;
    }

    // Move to next player (clockwise logic mapping to bottom/left/top/right)
    roulette_idx = (roulette_idx + 1) % track;
    selection_set_single(roulette_idx);
    /* Restart the deselect-timeout countdown like any selection change,
       so a timer left running from before the reset can't fire mid-spin
       and blank the selection for a tick. */
    select_kick_timer();
    refresh_player_ui();

    player_select_anim_steps--;
    if (player_select_anim_steps <= 0) {
        lv_timer_pause(player_select_anim_timer);
        select_kick_timer();
        if (initiative_player != roulette_idx) toggle_initiative(roulette_idx);
    } else {
        // Linear deceleration
        player_select_anim_period += (200 / (player_select_anim_steps + 1));
        if (player_select_anim_period > 600) player_select_anim_period = 600;
        lv_timer_set_period(player_select_anim_timer, player_select_anim_period);
    }
}

void start_player_selection_animation(void)
{
    int track = nvs_get_players_to_track();
    int random_stops;

    if (track <= 1) return;

    if (player_select_anim_timer == NULL) {
        player_select_anim_timer = lv_timer_create(player_select_anim_cb, 50, NULL);
    }

    // Randomize length to ensure random landing
    random_stops = (int)(esp_random() % track) + (track * 3);
    random_stops += esp_random() % (track * 2);

    player_select_anim_steps = random_stops;
    player_select_anim_period = 40; // start fast

    roulette_idx = 0;
    selection_set_single(0);
    select_kick_timer();

    lv_timer_set_period(player_select_anim_timer, player_select_anim_period);
    lv_timer_resume(player_select_anim_timer);
}

void stop_player_selection_animation(void)
{
    player_select_anim_steps = 0;
    if (player_select_anim_timer != NULL) {
        lv_timer_del(player_select_anim_timer);
        player_select_anim_timer = NULL;
    }
}

bool player_selection_animation_active(void)
{
    return player_select_anim_timer != NULL && player_select_anim_steps > 0;
}

// ---------- table sync (ESP-NOW) ----------
_Static_assert(NET_SYNC_MAX_PLAYERS == MAX_DISPLAY_PLAYERS, "packet layout");
_Static_assert(sizeof(((net_sync_names_t *)0)->names) == sizeof(player_names),
               "packet layout");
_Static_assert(DAMAGE_MAX <= INT16_MAX && BASE_HP_MAX <= UINT8_MAX, "packet layout");

void net_sync_fill_names(net_sync_names_t *out)
{
    int i;
    /* Copy per-row through snprintf, not one memcpy: bytes past each
       name's NUL are residue from earlier longer names and must not
       go out on the air (also keeps roster comparison canonical). */
    memset(out, 0, sizeof(*out));
    out->version = names_version;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        snprintf(out->names[i], NET_SYNC_NAME_LEN, "%s", player_names[i]);
}

/* Adopt a remote roster. Whole-set LWW on the roster version (serial
   arithmetic, MAC tiebreak), like a single state block. Runs on the
   main task, same as net_sync_apply_state. */
void net_sync_apply_names(const net_sync_names_t *in, int wins_ties)
{
    int16_t newer = (int16_t)(in->version - names_version);
    int i;

    if (newer < 0) {
        /* Same self-healing as state: answer a stale roster so the
           sender converges without waiting for a rename or invite. */
        net_sync_send_reply();
        return;
    }
    if (newer == 0 && !wins_ties) return;
    names_version = in->version;
    if (memcmp(player_names, in->names, sizeof(player_names)) == 0) return;
    memcpy(player_names, in->names, sizeof(player_names));
    /* Wire bytes are untrusted: every name must terminate. */
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        player_names[i][sizeof(player_names[i]) - 1] = '\0';
    /* Same refresh set as a local rename (rename.c). */
    refresh_player_ui();
    refresh_rename_ui();
}

void net_sync_fill_state(net_sync_state_t *out)
{
    int p;

    memset(out, 0, sizeof(*out));
    out->epoch = game_epoch;
    for (p = 0; p < MAX_DISPLAY_PLAYERS; p++) {
        net_sync_player_t *rp = &out->players[p];
        rp->version = player_version[p];
        rp->damage = (int16_t)player_damage[p];
        rp->base_hp = (uint8_t)player_base_hp[p];
        if (player_eliminated[p]) rp->flags |= NET_SYNC_ELIM;
        if (player_manually_eliminated[p]) rp->flags |= NET_SYNC_ELIM_MANUAL;
        if (player_force[p]) rp->flags |= NET_SYNC_FORCE;
        if (initiative_player == p) rp->flags |= NET_SYNC_INITIATIVE;
    }
}

/* Adopt a remote state snapshot. Runs on the main (LVGL) task — packets
   are queued by the radio and drained from loop() — so UI refresh is
   safe here. The game epoch dominates: a newer epoch (new game started
   or reset elsewhere) is adopted wholesale, an older one is rejected
   and answered with our own state (anti-entropy: the stale device
   converges in one exchange instead of waiting out the beacon). Within
   the same epoch, a player's block is adopted iff its version is newer
   (serial arithmetic, so uint16 wrap is fine); equal versions defer to
   the sender with the higher MAC so both devices pick the same winner.

   State is adopted directly — never route remote state through the
   apply/undo helpers: they bump versions and re-broadcast, and they'd
   re-derive elimination that the sender already decided. The damage
   log stays a per-device view of local actions. */
void net_sync_apply_state(const net_sync_state_t *in, int wins_ties)
{
    bool changed = false;
    bool remote_stale = false;
    int16_t epoch_newer = (int16_t)(in->epoch - game_epoch);
    int p;

    if (epoch_newer < 0) {
        net_sync_send_reply();
        return;
    }
    if (epoch_newer > 0) {
        game_epoch = in->epoch;
        /* A new game from the table: local elimination-undo actions
           and the event log refer to a game that no longer exists
           (see net_sync_reset_versions). */
        for (p = 0; p < MAX_DISPLAY_PLAYERS; p++)
            clear_player_elimination_action(p);
        damage_log_reset();
    }

    for (p = 0; p < MAX_DISPLAY_PLAYERS; p++) {
        const net_sync_player_t *rp = &in->players[p];
        int16_t newer = (int16_t)(rp->version - player_version[p]);
        bool was_eliminated = player_eliminated[p];
        bool now_eliminated = (rp->flags & NET_SYNC_ELIM) != 0;
        bool force = (rp->flags & NET_SYNC_FORCE) != 0;
        bool initiative = (rp->flags & NET_SYNC_INITIATIVE) != 0;
        bool p_changed = false;
        int damage = clamp_damage(rp->damage);
        int base_hp = clamp_base_hp(rp->base_hp);

        /* Same epoch: per-player Lamport rule. A newer epoch adopts
           every block regardless of version drift. */
        if (epoch_newer == 0 && (newer < 0 || (newer == 0 && !wins_ties))) {
            if (newer < 0) remote_stale = true;
            continue;
        }

        if (player_damage[p] != damage) {
            player_damage[p] = damage;
            /* A pending preview was dialed against a total that no
               longer exists: if this player is in the current
               selection, drop the whole group's proposal (it is one
               shared delta) so a duplicate entry can't silently
               double-apply — the user sees the total snap and can
               re-dial. Changes to unselected players compose as
               usual. The selection itself stays: the grouping is
               still valid intent, only the number was invalidated. */
            if (damage_preview_active && player_selected[p]) {
                pending_damage_delta = 0;
                damage_preview_active = false;
                if (damage_preview_timer != NULL) {
                    lv_timer_pause(damage_preview_timer);
                }
                select_kick_timer();
            }
            p_changed = true;
        }
        if (player_base_hp[p] != base_hp) {
            player_base_hp[p] = base_hp;
            nvs_set_player_base_hp(p, base_hp);
            p_changed = true;
        }
        if (player_force[p] != force) {
            player_force[p] = force;
            p_changed = true;
        }
        if (initiative && initiative_player != p) {
            initiative_player = p;
            p_changed = true;
        } else if (!initiative && initiative_player == p) {
            initiative_player = -1;
            p_changed = true;
        }
        if (was_eliminated != now_eliminated) {
            player_eliminated[p] = now_eliminated;
            if (!now_eliminated) {
                clear_player_elimination_action(p);
            } else if (player_selected[p]) {
                /* Same rule as check_player_elimination: an eliminated
                   player leaves the selection set. */
                player_selected[p] = false;
                select_kick_timer();
            }
            p_changed = true;
        } else if (was_eliminated && p_changed) {
            /* Still eliminated but the block changed underneath (a
               missed revive+re-kill): the stored undo action belongs
               to the old elimination and would restore the wrong
               amount. Undo then falls back to the revive clamp. */
            clear_player_elimination_action(p);
        }
        player_manually_eliminated[p] =
            now_eliminated && (rp->flags & NET_SYNC_ELIM_MANUAL) != 0;
        player_version[p] = rp->version;
        changed = changed || p_changed;
    }

    if (changed) {
        refresh_player_ui();
    }
    /* The sender is behind and we adopted nothing: answer immediately
       so its lost-update window is one exchange, not a 5s beacon. */
    if (remote_stale && !changed) {
        net_sync_send_reply();
    }
    /* A fresh joiner can adopt the table's epoch while still awaiting
       the roster (every invite-window names packet lost): announcing
       its version-0 roster makes any peer see it as stale and reply
       with the real one (see net_sync_apply_names). */
    if (epoch_newer > 0 && names_version == 0) {
        net_sync_send_names();
    }
}
