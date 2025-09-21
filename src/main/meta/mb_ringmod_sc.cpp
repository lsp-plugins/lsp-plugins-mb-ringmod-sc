/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-mb-ringmod-sc
 * Created on: 08 сен 2025 г.
 *
 * lsp-plugins-mb-ringmod-sc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-mb-ringmod-sc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-mb-ringmod-sc. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/mb_ringmod_sc.h>

#define LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MAJOR         1
#define LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MINOR         0
#define LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MICRO         0

#define LSP_PLUGINS_MB_RINGMOD_SC_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MAJOR, \
        LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MINOR, \
        LSP_PLUGINS_MB_RINGMOD_SC_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const port_item_t ringmod_sc_types[] =
        {
            { "Internal",       "sidechain.internal" },
            { "External",       "sidechain.external" },
            { "Link",           "sidechain.link" },
            { NULL, NULL }
        };

        static const port_item_t ringmod_sc_sources[] =
        {
            { "Left/Right",     "sidechain.left_right"      },
            { "Right/Left",     "sidechain.right_left"      },
            { "Left",           "sidechain.left"            },
            { "Right",          "sidechain.right"           },
            { "Mid/Side",       "sidechain.mid_side"        },
            { "Side/Mid",       "sidechain.side_mid"        },
            { "Middle",         "sidechain.middle"          },
            { "Side",           "sidechain.side"            },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t mb_ringmod_sc_modes[] =
        {
            { "Classic",        "multiband.classic"         },
            { "Linear Phase",   "multiband.linear_phase"    },
            { NULL, NULL }
        };

        static const port_item_t mb_ringmod_sc_slopes[] =
        {
            { "12 dB/oct",      "eq.slope.12dbo"            },
            { "24 dB/oct",      "eq.slope.24dbo"            },
            { "48 dB/oct",      "eq.slope.48dbo"            },
            { "72 dB/oct",      "eq.slope.72dbo"            },
            { NULL, NULL }
        };

    #define RMOD_COMMON(channels) \
        BYPASS, \
        IN_GAIN, \
        SC_GAIN, \
        OUT_GAIN, \
        SWITCH("out_in", "Output input signal", "Out In", 1), \
        SWITCH("out_sc", "Output sidechain signal", "Out SC", 1), \
        SWITCH("active", "Sidechain processing active", "Active", 1), \
        SWITCH("invert", "Invert sidechain processing", "Invert", 0), \
        COMBO("type", "Sidechain type", "Type", 1, ringmod_sc_types), \
        COMBO("mode", "Crossover mode", "Mode", 0, mb_ringmod_sc_modes), \
        COMBO("slope", "Crossover slope", "Slope", 2, mb_ringmod_sc_slopes), \
        SWITCH("showmx", "Show mix overlay", "Show mix bar", 0.0f), \
        AMP_GAIN10("dry", "Dry gain", "Dry", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("wet", "Wet gain", "Wet", GAIN_AMP_0_DB), \
        PERCENTS("drywet", "Dry/Wet balance", "Dry/Wet", 100.0f, 0.1f), \
        LOG_CONTROL("zoom", "Graph zoom", "Zoom", U_GAIN_AMP, mb_ringmod_sc::ZOOM), \
        SWITCH("flt", "Band filter curves", "Show filters", 1.0f), \
        LOG_CONTROL("react", "FFT reactivity", "Reactivity", U_MSEC, mb_ringmod_sc::REACT_TIME), \
        AMP_GAIN100("shift", "Shift gain", "Shift", 1.0f), \
        MESH("bfc", "Band filter charts", 9, mb_ringmod_sc::FFT_MESH_POINTS + 4), \
        MESH("meters", "Band filter reduction meters", 1 + channels * 4, mb_ringmod_sc::FFT_MESH_POINTS + 4)

    #define RMOD_METER_BUTTONS(id, label, alias) \
        SWITCH("ifft" id, "Input FFT analysis" label, "FFT In" alias, 1), \
        SWITCH("sfft" id, "Sidechain FFT analysis" label, "FFT Sc" alias, 1), \
        SWITCH("offt" id, "Output FFT analysis" label, "FFT Out" alias, 1), \
        METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_24_DB), \
        METER_GAIN("slm" id, "Sidechain level meter" label, GAIN_AMP_P_24_DB), \
        METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_24_DB)

    #define RMOD_COMMON_MONO \
        RMOD_COMMON(1)

    #define RMOD_COMMON_STEREO \
        RMOD_COMMON(2), \
        COMBO("source", "Sidechain source", "Source", 0, ringmod_sc_sources)

    #define RMOD_MIX_SIGNAL \
        SWITCH("showmx", "Show mix overlay", "Show mix bar", 0.0f), \
        AMP_GAIN10("dry", "Dry gain", "Dry", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("wet", "Wet gain", "Wet", GAIN_AMP_0_DB), \
        PERCENTS("drywet", "Dry/Wet balance", "Dry/Wet", 100.0f, 0.1f)

    #define RMOD_PREMIX \
        SWITCH("showpmx", "Show pre-mix overlay", "Show premix bar", 0.0f), \
        AMP_GAIN10("in2lk", "Input to Link mix", "In to Link mix", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("lk2in", "Link to Input mix", "Link to In mix", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("lk2sc", "Link to Sidechain mix", "Link to SC mix", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("in2sc", "Input to Sidechain mix", "In to SC mix", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("sc2in", "Sidechain to Input mix", "SC to In mix", GAIN_AMP_M_INF_DB), \
        AMP_GAIN10("sc2lk", "Sidechain to Link mix", "SC to Link mix", GAIN_AMP_M_INF_DB)

    #define RMOD_SHM_LINK_MONO \
        OPT_RETURN_MONO("link", "shml", "Side-chain shared memory link")

    #define RMOD_SHM_LINK_STEREO \
        OPT_RETURN_STEREO("link", "shml_", "Side-chain shared memory link")

    #define RMOD_SPLIT(id, label, enable, freq) \
        SWITCH("se" id, "Band split enable" label, "Split on" label, enable), \
        LOG_CONTROL_DFL("sf" id, "Band split frequency" label, "Split" label, U_HZ, mb_ringmod_sc::FREQ, freq)

    #define RMOD_BAND_COMMON(id, label, alias) \
        SWITCH("bs" id, "Solo band" label, "Solo" alias, 0.0f), \
        SWITCH("bm" id, "Mute band" label, "Mute" alias, 0.0f), \
        SWITCH("be" id, "Enable band processing" label, "Enable" alias, 1.0f), \
        CONTROL("lk" id, "Lookahead time" label, "Lookahead" alias, U_MSEC, mb_ringmod_sc::LOOKAHEAD), \
        CONTROL("ht" id, "Hold time" label, "Hold" alias, U_MSEC, mb_ringmod_sc::HOLD), \
        LOG_CONTROL("rt" id, "Release time" label, "Release" alias, U_MSEC, mb_ringmod_sc::RELEASE), \
        CONTROL("dt" id, "Ducking time" label, "Duck" alias, U_MSEC, mb_ringmod_sc::DUCK), \
        CONTROL("am" id, "Amount" label, "Amount" alias, U_DB, mb_ringmod_sc::AMOUNT), \
        AMP_GAIN10("bg" id, "Band Gain" label, "Gain" alias, GAIN_AMP_0_DB), \
        METER("fre" id, "Frequency range end" label, U_HZ, mb_ringmod_sc::OUT_FREQ)

    #define RMOD_BAND_METERS(id, label) \
        METER_OUT_GAIN("rlm" id, "Reduction level meter" label, GAIN_AMP_0_DB)

    #define RMOD_BAND_MONO(id, label, alias) \
        RMOD_BAND_COMMON(id, label, alias), \
        RMOD_BAND_METERS(id, label)

    #define RMOD_BAND_STEREO(id, label, alias, slink) \
        RMOD_BAND_COMMON(id, label, alias), \
        PERCENTS("bsl" id, "Band stereo linking" label, "Stereo link" label, slink, 0.1f), \
        RMOD_BAND_METERS(id "l", label " Left"), \
        RMOD_BAND_METERS(id "r", label " Right")

        //-------------------------------------------------------------------------
        // Plugin metadata
        static const port_t mb_ringmod_sc_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            RMOD_SHM_LINK_MONO,
            RMOD_PREMIX,
            RMOD_COMMON_MONO,

            RMOD_METER_BUTTONS("", "", ""),

            RMOD_SPLIT("_1", " 1", 0.0f, 40.0f),
            RMOD_SPLIT("_2", " 2", 1.0f, 100.0f),
            RMOD_SPLIT("_3", " 3", 0.0f, 252.0f),
            RMOD_SPLIT("_4", " 4", 1.0f, 632.0f),
            RMOD_SPLIT("_5", " 5", 0.0f, 1587.0f),
            RMOD_SPLIT("_6", " 6", 1.0f, 3984.0f),
            RMOD_SPLIT("_7", " 7", 0.0f, 10000.0f),

            RMOD_BAND_MONO("_1", " 1", " 1"),
            RMOD_BAND_MONO("_2", " 2", " 2"),
            RMOD_BAND_MONO("_3", " 3", " 3"),
            RMOD_BAND_MONO("_4", " 4", " 4"),
            RMOD_BAND_MONO("_5", " 5", " 5"),
            RMOD_BAND_MONO("_6", " 6", " 6"),
            RMOD_BAND_MONO("_7", " 7", " 7"),
            RMOD_BAND_MONO("_8", " 8", " 8"),

            PORTS_END
        };

        static const port_t mb_ringmod_sc_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            RMOD_SHM_LINK_STEREO,
            RMOD_PREMIX,
            RMOD_COMMON_STEREO,

            RMOD_METER_BUTTONS("_l", " Left", " L"),
            RMOD_METER_BUTTONS("_r", " Right", " R"),

            RMOD_SPLIT("_1", " 1", 0.0f, 40.0f),
            RMOD_SPLIT("_2", " 2", 1.0f, 100.0f),
            RMOD_SPLIT("_3", " 3", 0.0f, 252.0f),
            RMOD_SPLIT("_4", " 4", 1.0f, 632.0f),
            RMOD_SPLIT("_5", " 5", 0.0f, 1587.0f),
            RMOD_SPLIT("_6", " 6", 1.0f, 3984.0f),
            RMOD_SPLIT("_7", " 7", 0.0f, 10000.0f),

            RMOD_BAND_STEREO("_1", " 1", " 1", 100.0f),
            RMOD_BAND_STEREO("_2", " 2", " 2", 85.0f),
            RMOD_BAND_STEREO("_3", " 3", " 3", 71.0f),
            RMOD_BAND_STEREO("_4", " 4", " 4", 57.0f),
            RMOD_BAND_STEREO("_5", " 5", " 5", 43.0f),
            RMOD_BAND_STEREO("_6", " 6", " 6", 28.0f),
            RMOD_BAND_STEREO("_7", " 7", " 7", 14.0f),
            RMOD_BAND_STEREO("_8", " 8", " 8", 0.0f),

            PORTS_END
        };

        static const int plugin_classes[]       = { C_DYNAMICS, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t mb_ringmod_sc_bundle =
        {
            "mb_ringmod_sc",
            "Ring Modulated Sidechain",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "This plugin allows to apply a specific multiband sidechaining technique based on\nring modulation and subtraction of the original signal."
        };

        const plugin_t mb_ringmod_sc_mono =
        {
            "Multiband Ring Modulated Sidechain Mono",
            "Multiband Ring Modulated Sidechain Mono",
            "MB Ring Modulated SC Mono",
            "MBRMSC1M",
            &developers::v_sadovnikov,
            "mb_ringmod_sc_mono",
            {
                LSP_LV2_URI("mb_ringmod_sc_mono"),
                LSP_LV2UI_URI("mb_ringmod_sc_mono"),
                "mbr1",
                LSP_VST3_UID("mbr1mb4msc1m"),
                LSP_VST3UI_UID("mbr1mb4msc1m"),
                LSP_LADSPA_MB_RINGMOD_SC_BASE + 0,
                LSP_LADSPA_URI("mb_ringmod_sc_mono"),
                LSP_CLAP_URI("mb_ringmod_sc_mono"),
                LSP_GST_UID("mb_ringmod_sc_mono"),
            },
            LSP_PLUGINS_MB_RINGMOD_SC_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            mb_ringmod_sc_mono_ports,
            "utils/mb_ringmod_sc.xml",
            NULL,
            mono_plugin_port_groups,
            &mb_ringmod_sc_bundle
        };

        const plugin_t mb_ringmod_sc_stereo =
        {
            "Multiband Ring Modulated Sidechain Stereo",
            "Multiband Ring Modulated Sidechain Stereo",
            "MB Ring Modulated SC Stereo",
            "MBRMSC1S",
            &developers::v_sadovnikov,
            "mb_ringmod_sc_stereo",
            {
                LSP_LV2_URI("mb_ringmod_sc_stereo"),
                LSP_LV2UI_URI("mb_ringmod_sc_stereo"),
                "mbR1",
                LSP_VST3_UID("mbR1mb4msc1s"),
                LSP_VST3UI_UID("mbR1mb4msc1s"),
                LSP_LADSPA_MB_RINGMOD_SC_BASE + 1,
                LSP_LADSPA_URI("mb_ringmod_sc_stereo"),
                LSP_CLAP_URI("mb_ringmod_sc_stereo"),
                LSP_GST_UID("mb_ringmod_sc_stereo"),
            },
            LSP_PLUGINS_MB_RINGMOD_SC_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            mb_ringmod_sc_stereo_ports,
            "utils/mb_ringmod_sc.xml",
            NULL,
            stereo_plugin_port_groups,
            &mb_ringmod_sc_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



