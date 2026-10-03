#!/bin/bash
# Generate the full screenshot matrix.
# Run from the sim/ directory: ./generate_matrix.sh
set -e

SIM="${SIM:-./knobby_sim}"
OUT=screenshots
COUNT=0
FILES=()

mkdir -p "$OUT"

shot() {
    local filename="$1"
    shift
    $SIM --outdir "$OUT" --output "$filename" "$@"
    COUNT=$((COUNT + 1))
    FILES+=("$filename")
}

ORIENT_NAMES=("absolute" "centric" "tabletop")

# ============================================================
# 1. Damage preview — 1p mode
# ============================================================
for delta in +1 +5 -3 +99; do
    tag=$(echo "$delta" | tr '+' 'p' | tr '-' 'n')
    shot "1p_preview_${tag}.png" --screen 1p --track 1 \
        --damage 12 --preview-delta "$delta" --preview-player -1
done

# ============================================================
# 2. Damage preview — multiplayer modes × orientations
# ============================================================
for track in 2 3 4; do
    max_player=$((track - 1))
    for orient in 0 1 2; do
        oname=${ORIENT_NAMES[$orient]}
        for player in $(seq 0 $max_player); do
            shot "${track}p_${oname}_p${player}_preview_p5.png" \
                --screen ${track}p --track "$track" --orientation "$orient" \
                --damage 10,10,10,10 --preview-delta +5 --preview-player "$player"
        done
    done
done

# ============================================================
# 3. Damage totals — 1p and multiplayer, including destroyed bases
# ============================================================
for dmg in 0 9 15 23 29 30; do
    shot "1p_damage${dmg}.png" --screen 1p --track 1 --damage "$dmg"
    shot "1p_hpcolor_damage${dmg}.png" --screen 1p --track 1 --damage "$dmg" --color-mode 1
done
shot "1p_damage99_hp25.png" --screen 1p --track 1 --base-hp 25 --damage 99

for track in 2 3 4; do
    for orient in 0 1 2; do
        oname=${ORIENT_NAMES[$orient]}
        shot "${track}p_${oname}_damage.png" --screen ${track}p --track "$track" \
            --orientation "$orient" --damage 0,12,24,29 --player-hp 30,27,28,25
        shot "${track}p_${oname}_hpcolor.png" --screen ${track}p --track "$track" \
            --orientation "$orient" --damage 3,14,22,29 --color-mode 1
        shot "${track}p_${oname}_destroyed.png" --screen ${track}p --track "$track" \
            --orientation "$orient" --damage 30,12,30,5 --auto-eliminate 1
    done
done

# ============================================================
# 4. Tokens — initiative and Force on every seat
# ============================================================
shot "1p_tokens_both.png" --screen 1p --track 1 --damage 8 --initiative 0 --force 1
shot "1p_tokens_force.png" --screen 1p --track 1 --damage 8 --force 1
for track in 2 3 4; do
    max_player=$((track - 1))
    for orient in 0 1 2; do
        oname=${ORIENT_NAMES[$orient]}
        for player in $(seq 0 $max_player); do
            shot "${track}p_${oname}_tokens_p${player}.png" --screen ${track}p --track "$track" \
                --orientation "$orient" --damage 7,14,21,28 --initiative "$player" --force 1,1,1,1
        done
    done
done

# ============================================================
# 5. Selection and multi-select
# ============================================================
for track in 2 3 4; do
    shot "${track}p_selected_p0.png" --screen ${track}p --track "$track" --selected 0
done
shot "multiselect_4p_preview.png" --screen 4p --track 4 --multi-select 1 \
    --selected-players 0,1,2 --preview-delta 2 --damage 5,10,15,20

# ============================================================
# 6. Per-player colors
# ============================================================
for track in 2 4; do
    shot "${track}p_perplayer.png" --screen ${track}p --track "$track" \
        --player-colors 4,5,6,13 --player-override 1,1,1,1
done
shot "color_menu.png" --screen color-menu --menu-player 0
shot "color_picker.png" --screen color-picker --menu-player 0

# ============================================================
# 7. Player menus
# ============================================================
shot "player_menu_default.png" --screen player-menu
shot "player_menu_tokens.png" --screen player-menu --initiative 0 --force 1 --player-hp 27
shot "player_menu_facing_2p_tabletop_p1.png" --screen player-menu \
    --track 2 --orientation 2 --menu-facing 1 --menu-player 1
shot "hp_edit.png" --screen hp-edit --player-hp 30
shot "hp_edit_preview_neg.png" --screen hp-edit --player-hp 30 --hp-edit-delta -4
shot "hp_edit_preview_pos.png" --screen hp-edit --player-hp 25 --hp-edit-delta 8
shot "eliminated_menu.png" --screen eliminated
shot "rename.png" --screen rename
shot "rename_longnames.png" --screen rename \
    --names "Bossk the Hunter,Sabine Wren,Grand Moff Tarkin,Hera Syndulla"

# ============================================================
# 8. Game mode and default base HP
# ============================================================
shot "game_mode_default.png" --screen game-mode
shot "game_mode_4p_25.png" --screen game-mode --track 4 --base-hp 25 --random-first 0
shot "custom_hp_33.png" --screen custom-hp --base-hp 33

# ============================================================
# 9. Settings: every toggle in every state, page-agnostic.
# ============================================================
for dim in 0 1 2 3; do
    shot "setting_autodim_${dim}.png" --screen setting:autodim --auto-dim "$dim"
done
for cm in 0 1; do
    shot "setting_colormode_${cm}.png" --screen setting:color-mode --color-mode "$cm"
done
for dt in 0 1 2 3; do
    shot "setting_deselect_${dt}.png" --screen setting:deselect --deselect "$dt"
done
for rot in 0 1 2; do
    shot "setting_orientation_${rot}.png" --screen setting:orientation --orientation "$rot"
done
for ae in 0 1; do
    shot "setting_autoelim_${ae}.png" --screen setting:auto-eliminate --auto-eliminate "$ae"
done
for ms in 0 1; do
    shot "setting_multiselect_${ms}.png" --screen setting:multi-select --multi-select "$ms"
done
for mf in 0 1; do
    shot "setting_menufacing_${mf}.png" --screen setting:menu-facing --menu-facing "$mf"
done
shot "setting_rotate_screen.png" --screen rotate
shot "1p_rot90.png"  --screen 1p --track 1 --display-rotation 1
shot "4p_rot180.png" --screen 4p --track 4 --display-rotation 2
shot "setting_tablesync_off.png"    --screen table-sync --track 4
shot "setting_tablesync_ingame.png" --screen table-sync --track 4 --table-sync 1 --table-session 14242
shot "setting_tablesync_1p.png"     --screen table-sync --track 1

PAGES=$($SIM --print-settings-pages)
for p in $(seq 1 "$PAGES"); do
    shot "settings_page${p}.png" --screen "settings-page${p}"
done

for bri in 1 30 100; do
    shot "brightness_${bri}pct.png" --screen brightness --brightness "$bri"
done

# ============================================================
# 10. Battery and low-battery indicator
# ============================================================
for v in 4.15 3.85 3.55; do
    shot "battery_${v}v.png" --screen battery --battery-voltage "$v"
done
# 3.62V is ~8% (solid icon, 5-10% tier).  3.40V is ~3% (blink, <5% tier).
shot "lowbatt_solid_1p.png" --screen 1p --track 1 --battery-voltage 3.62
shot "lowbatt_blink_1p.png" --screen 1p --track 1 --battery-voltage 3.40
for track in 2 3 4; do
    shot "lowbatt_solid_${track}p.png" --screen ${track}p --track "$track" \
        --battery-voltage 3.62
done

# ============================================================
# 11. Tools: coin flip, event log, round timer
# ============================================================
shot "coin_heads.png" --screen coin --coin 1
shot "coin_tails.png" --screen coin --coin 2
shot "damage_log_random.png" --screen damage-log --random-log \
    --names Luke,Vader,Leia,Han
shot "damage_log_empty.png" --screen damage-log
for dmg in 0 29; do
    shot "1p_timer_damage${dmg}.png" --screen 1p --track 1 --damage "$dmg" \
        --turn-number 7 --turn-elapsed 2520000
done
shot "1p_timer_preview.png" --screen 1p --track 1 --damage 12 \
    --turn-number 3 --turn-elapsed 600000 --preview-delta 4

# ============================================================
# 12. Menus and intro
# ============================================================
shot "menu_main.png" --screen menu
shot "menu_tools.png" --screen tools
shot "intro.png" --screen intro

# ============================================================
# Generate index.html
# ============================================================
INDEX="$OUT/index.html"
cat > "$INDEX" << 'HEADER'
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Knobby SWU Screenshot Matrix</title>
<style>
  body { font-family: sans-serif; max-width: 1200px; margin: 2rem auto; padding: 0 1rem; background: #f5f5f5; }
  h1 { margin-bottom: 0.25rem; }
  .count { color: #666; margin-bottom: 1.5rem; }
  h2 { margin-top: 2rem; border-bottom: 1px solid #ccc; padding-bottom: 0.25rem; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 1rem; }
  .item { text-align: center; }
  .item img { max-width: 100%; border: 1px solid #ccc; border-radius: 4px; background: #fff; }
  .item a { display: block; margin-top: 0.25rem; word-break: break-all; font-size: 0.75rem; color: #333; }
</style>
</head>
<body>
<h1>Knobby SWU Screenshot Matrix</h1>
HEADER

echo "<p class=\"count\">$COUNT screenshots</p>" >> "$INDEX"

write_section() {
    local title="$1"
    shift
    local files=("$@")
    if [ ${#files[@]} -eq 0 ]; then return; fi
    echo "<h2>$title</h2>" >> "$INDEX"
    echo '<div class="grid">' >> "$INDEX"
    for f in "${files[@]}"; do
        echo "  <div class=\"item\"><a href=\"$f\"><img src=\"$f\" alt=\"$f\"></a><a href=\"$f\">$f</a></div>" >> "$INDEX"
    done
    echo '</div>' >> "$INDEX"
}

# Sort files into sections
SEC_PREV=(); SEC_DAMAGE=(); SEC_HPCOLOR=(); SEC_DESTROYED=(); SEC_TOKENS=()
SEC_SELECT=(); SEC_COLORS=(); SEC_PLAYER_MENU=(); SEC_GAME_MODE=(); SEC_SETTINGS=()
SEC_BATTERY=(); SEC_TOOLS=(); SEC_OTHER=()

for f in "${FILES[@]}"; do
    case "$f" in
        *_preview_*|multiselect_*)                 SEC_PREV+=("$f") ;;
        *_hpcolor*)                                SEC_HPCOLOR+=("$f") ;;
        *_destroyed.png)                           SEC_DESTROYED+=("$f") ;;
        *_tokens*)                                 SEC_TOKENS+=("$f") ;;
        *_damage*)                                 SEC_DAMAGE+=("$f") ;;
        *_selected_*)                              SEC_SELECT+=("$f") ;;
        *_perplayer.png|color_*)                   SEC_COLORS+=("$f") ;;
        player_menu_*|hp_edit*|eliminated_*|rename*) SEC_PLAYER_MENU+=("$f") ;;
        game_mode_*|custom_hp_*)                   SEC_GAME_MODE+=("$f") ;;
        setting_*|settings_*|brightness_*|*_rot*)  SEC_SETTINGS+=("$f") ;;
        battery_*|lowbatt_*)                       SEC_BATTERY+=("$f") ;;
        coin_*|damage_log_*|1p_timer_*)            SEC_TOOLS+=("$f") ;;
        *)                                         SEC_OTHER+=("$f") ;;
    esac
done

write_section "Damage Preview" "${SEC_PREV[@]}"
write_section "Damage Totals" "${SEC_DAMAGE[@]}"
write_section "HP Colors" "${SEC_HPCOLOR[@]}"
write_section "Destroyed Bases" "${SEC_DESTROYED[@]}"
write_section "Initiative and Force" "${SEC_TOKENS[@]}"
write_section "Selected Player" "${SEC_SELECT[@]}"
write_section "Player Colors" "${SEC_COLORS[@]}"
write_section "Player Menus" "${SEC_PLAYER_MENU[@]}"
write_section "Game Mode" "${SEC_GAME_MODE[@]}"
write_section "Settings" "${SEC_SETTINGS[@]}"
write_section "Battery" "${SEC_BATTERY[@]}"
write_section "Tools" "${SEC_TOOLS[@]}"
write_section "Other" "${SEC_OTHER[@]}"

echo '</body></html>' >> "$INDEX"

echo ""
echo "Generated $COUNT screenshots in $OUT/"
echo "Index: $OUT/index.html"
