/*
 * Experimental Scott the Woz voice pack for Gex: Enter the Gecko (SLUS-00598).
 *
 * This is deliberately a default-off mod. It loads four PS1 ADPCM/VAG resources
 * through the trusted PSXRecomp plugin API and mixes them on a private SPU voice,
 * leaving the stock game's 24 SPU voices and pad data untouched.
 *
 * The health/lives locations below are a first-pass SLUS-00598 gameplay detector
 * based on publicly listed cheat addresses. Validate them during real gameplay;
 * they are not treated as authoritative reverse-engineered symbols. If the
 * values are not plausible, gameplay-event triggers stay dormant rather than
 * touching guest memory outside the emulated RAM API.
 */
#include "mod_plugins.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCOTT_PLUGIN_ID "nvd.gex.scott-voicepack"

/* Values seen in public cheat tables for the USA PS1 build. Validate in-game. */
#define GEX_LIVES_ADDRESS  0x80095310u
#define GEX_HEALTH_ADDRESS 0x80095314u

#define VAG_HEADER_BYTES 0x30u
#define VAG_MAX_ADPCM_BYTES 0x80000u
#define VOICE_SLOT 0u
#define FRAME_IDLE_DELAY 600u       /* 10 seconds at 60 guest VBlanks */
#define FRAME_IDLE_REPEAT 1800u     /* repeat the idle quip no more than every 30s */
#define FRAME_MOVE_COOLDOWN 180u    /* keep repeated taps from spamming voice */

enum ScottClip {
    CLIP_LEVEL_ENTRY = 0,
    CLIP_MOVEMENT_JUMP,
    CLIP_DEATH,
    CLIP_IDLE,
    CLIP_COUNT
};

static const char *const g_resource_ids[CLIP_COUNT] = {
    "level-entry",
    "movement-jump",
    "death",
    "idle"
};

static uint8_t g_clip_ready[CLIP_COUNT];
static uint32_t g_clip_duration_frames[CLIP_COUNT];
static uint32_t g_frame;
static uint32_t g_last_input_frame;
static uint32_t g_next_move_frame;
static uint32_t g_next_idle_frame;
static uint32_t g_play_until_frame;
static uint32_t g_invalid_player_frames;
static uint16_t g_last_health;
static int g_last_health_known;
static int g_saw_live_player;
static int g_death_transition;
static int g_pending_level_entry;
static int g_movement_down;
static int g_cross_down;

static uint32_t scott_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int scott_load_vag(enum ScottClip clip) {
    char path[2048];
    FILE *file = NULL;
    uint8_t header[VAG_HEADER_BYTES];
    uint8_t *adpcm = NULL;
    long actual_size;
    uint32_t data_size;
    int ok = 0;

    memset(path, 0, sizeof(path));
    if (!psx_mod_current_resource_path(g_resource_ids[clip], path,
                                       (uint32_t)sizeof(path)) || !path[0]) {
        fprintf(stderr, "Gex Scott voice pack: resource unavailable: %s\n",
                g_resource_ids[clip]);
        return 0;
    }

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Gex Scott voice pack: cannot open resource: %s\n",
                g_resource_ids[clip]);
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0 || (actual_size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        (unsigned long)actual_size < VAG_HEADER_BYTES ||
        fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fprintf(stderr, "Gex Scott voice pack: invalid VAG header: %s\n",
                g_resource_ids[clip]);
        goto done;
    }

    if (memcmp(header, "VAGp", 4) != 0) {
        fprintf(stderr, "Gex Scott voice pack: bad VAG magic: %s\n",
                g_resource_ids[clip]);
        goto done;
    }

    data_size = scott_be32(&header[0x0c]);
    if (!data_size || data_size > VAG_MAX_ADPCM_BYTES ||
        (data_size & 15u) != 0u ||
        (unsigned long)actual_size != VAG_HEADER_BYTES + (unsigned long)data_size) {
        fprintf(stderr, "Gex Scott voice pack: invalid VAG data length: %s\n",
                g_resource_ids[clip]);
        goto done;
    }

    adpcm = (uint8_t *)malloc(data_size);
    if (!adpcm) {
        fprintf(stderr, "Gex Scott voice pack: out of memory loading %s\n",
                g_resource_ids[clip]);
        goto done;
    }
    if (fread(adpcm, 1, data_size, file) != data_size) {
        fprintf(stderr, "Gex Scott voice pack: truncated VAG data: %s\n",
                g_resource_ids[clip]);
        goto done;
    }

    if (!psx_mod_spu_sample_bank((uint32_t)clip + 1u, adpcm, data_size)) {
        fprintf(stderr, "Gex Scott voice pack: SPU rejected clip: %s\n",
                g_resource_ids[clip]);
        goto done;
    }
    g_clip_duration_frames[clip] = (uint32_t)(((uint64_t)(data_size / 16u) * 28u * 60u + 22049u) / 22050u);
    if (!g_clip_duration_frames[clip]) g_clip_duration_frames[clip] = 1u;
    ok = 1;

done:
    free(adpcm);
    fclose(file);
    return ok;
}

static int scott_play(enum ScottClip clip) {
    /* SPU pitch 0x0800 plays 22,050-Hz VAG data at its authored rate. */
    uint16_t regs[8] = {
        0x3fffu, /* left volume */
        0x3fffu, /* right volume */
        0x0800u, /* pitch */
        0u,      /* start block: beginning of private bank */
        0u,      /* reserved ADSR register */
        0u,      /* reserved ADSR register */
        0u,
        0u       /* repeat block */
    };

    if (clip >= CLIP_COUNT || !g_clip_ready[clip]) return 0;
    if (!psx_mod_spu_private_voice_play(
            VOICE_SLOT, (uint32_t)clip + 1u, regs, 0u)) return 0;
    /* A PS1 voice remains logically active after its end block, but is silent. */
    g_play_until_frame = g_frame + g_clip_duration_frames[clip] + 1u;
    return 1;
}

static int scott_any_input(const PSXModPadFrame *frame) {
    const int dx = (int)frame->lx - 128;
    const int dy = (int)frame->ly - 128;
    return frame->buttons != 0xffffu ||
           dx < -24 || dx > 24 || dy < -24 || dy > 24;
}

static int scott_movement_input(const PSXModPadFrame *frame) {
    const int dx = (int)frame->lx - 128;
    const int dy = (int)frame->ly - 128;
    const int dpad = (frame->buttons & 0x00f0u) != 0x00f0u;
    const int stick = dx < -24 || dx > 24 || dy < -24 || dy > 24;
    return dpad || stick;
}

static int scott_pad_transform(const PSXModPadFrame *frame,
                               PSXModPadOutput *output) {
    int moving;
    (void)output;

    if (!frame || !g_saw_live_player) return 0;

    if (scott_any_input(frame)) {
        g_last_input_frame = g_frame;
    }

    moving = scott_movement_input(frame);
    {
        int cross_pressed = (frame->buttons & 0x4000u) == 0u;
        int new_action = (moving && !g_movement_down) ||
                         (cross_pressed && !g_cross_down);
        if (new_action && g_frame >= g_next_move_frame) {
            (void)scott_play(CLIP_MOVEMENT_JUMP);
            g_next_move_frame = g_frame + FRAME_MOVE_COOLDOWN;
        }
        g_cross_down = cross_pressed;
    }
    g_movement_down = moving;
    /* Return zero: deliver the original controller frame unchanged. */
    return 0;
}

static void scott_vblank(void) {
    uint16_t health;
    uint16_t lives;
    int plausible_player_state;

    ++g_frame;
    if (!psx_mod_game_started()) return;

    health = psx_mod_read_half(GEX_HEALTH_ADDRESS);
    lives = psx_mod_read_half(GEX_LIVES_ADDRESS);
    plausible_player_state = (health <= 4u && lives <= 99u);

    if (!plausible_player_state) {
        if (g_saw_live_player && !g_death_transition) {
            if (g_invalid_player_frames < 0xffffffffu)
                ++g_invalid_player_frames;
            if (g_invalid_player_frames >= 90u)
                g_pending_level_entry = 1;
        }
        g_last_health_known = 0;
        return;
    }

    g_invalid_player_frames = 0;

    if (health == 0u) {
        if (g_saw_live_player && g_last_health_known &&
            g_last_health >= 1u && g_last_health <= 4u) {
            g_death_transition = 1;
            (void)scott_play(CLIP_DEATH);
        }
        g_last_health = 0u;
        g_last_health_known = 1;
        return;
    }

    if (health >= 1u && health <= 4u) {
        if (!g_saw_live_player) {
            g_saw_live_player = 1;
            g_last_input_frame = g_frame;
            (void)scott_play(CLIP_LEVEL_ENTRY);
        } else if (g_pending_level_entry && !g_death_transition) {
            (void)scott_play(CLIP_LEVEL_ENTRY);
            g_pending_level_entry = 0;
        }

        /* Returning health after a death is a respawn, not a new level. */
        if (g_death_transition) {
            g_death_transition = 0;
            g_pending_level_entry = 0;
        }

        g_last_health = health;
        g_last_health_known = 1;

        if ((uint32_t)(g_frame - g_last_input_frame) >= FRAME_IDLE_DELAY &&
            g_frame >= g_next_idle_frame &&
            g_frame >= g_play_until_frame) {
            if (scott_play(CLIP_IDLE))
                g_next_idle_frame = g_frame + FRAME_IDLE_REPEAT;
        }
    }
}

static void scott_activate(void) {
    unsigned i;

    memset(g_clip_ready, 0, sizeof(g_clip_ready));
    g_frame = 0;
    g_last_input_frame = 0;
    g_next_move_frame = 0;
    g_next_idle_frame = 0;
    g_play_until_frame = 0;
    memset(g_clip_duration_frames, 0, sizeof(g_clip_duration_frames));
    g_invalid_player_frames = 0;
    g_last_health = 0;
    g_last_health_known = 0;
    g_saw_live_player = 0;
    g_death_transition = 0;
    g_pending_level_entry = 0;
    g_movement_down = 0;
    g_cross_down = 0;

    psx_mod_spu_private_voice_stop(1u << VOICE_SLOT);
    for (i = 0; i < CLIP_COUNT; ++i) {
        g_clip_ready[i] = (uint8_t)scott_load_vag((enum ScottClip)i);
    }

    {
        static const PSXModPadTransform pad_transform = {
            sizeof(PSXModPadTransform),
            PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DIGITAL) |
            PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DUALSHOCK) |
            PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_JOGCON) |
            PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_NEGCON),
            PSX_MOD_PAD_DIGITAL,
            scott_pad_transform
        };
        if (!psx_mod_set_pad_transform(0u, &pad_transform)) {
            fprintf(stderr, "Gex Scott voice pack: could not register passive input observer.\n");
        }
    }
}

PSX_MOD_CONSTRUCTOR(register_gex_scott_voicepack) {
    if (!psx_mod_register_activation_plugin(SCOTT_PLUGIN_ID, scott_activate)) {
        fprintf(stderr, "Gex Scott voice pack: activation registration failed.\n");
    }
    if (!psx_mod_register_vblank_plugin(SCOTT_PLUGIN_ID, scott_vblank)) {
        fprintf(stderr, "Gex Scott voice pack: VBlank registration failed.\n");
    }
}
