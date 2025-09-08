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

#define LSP_MB_RINGMOD_SC_VERSION_MAJOR       1
#define LSP_MB_RINGMOD_SC_VERSION_MINOR       0
#define LSP_MB_RINGMOD_SC_VERSION_MICRO       0

#define LSP_MB_RINGMOD_SC_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_MB_RINGMOD_SC_VERSION_MAJOR, \
        LSP_MB_RINGMOD_SC_VERSION_MINOR, \
        LSP_MB_RINGMOD_SC_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata

        static const port_t mb_ringmod_sc_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,

            BYPASS,

            PORTS_END
        };

        static const port_t mb_ringmod_sc_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,

            // Input controls
            BYPASS,

            PORTS_END
        };

        static const int plugin_classes[]       = { C_UTILITY, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t mb_ringmod_sc_bundle =
        {
            "mb_ringmod_sc", // TODO: write proper bundle identifier
            "Plugin Template", // TODO: write proper bundle name
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "This plugin allows to apply a specific multiban sidechaining technique based on\nring modulation and subtraction of the original signal."
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
            LSP_MB_RINGMOD_SC_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            mb_ringmod_sc_mono_ports,
            "utils/mb_ringmod_sc.xml",
            NULL,
            mono_plugin_port_groups,
            &mb_ringmod_sc_bundle
        };

        const plugin_t mb_ringmod_sc_stereo =
        {
            "Pluginschablone Stereo",
            "Plugin Template Stereo",
            "Plugin Template Stereo",
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
            LSP_MB_RINGMOD_SC_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            mb_ringmod_sc_stereo_ports,
            "utils/mb_ringmod_sc.xml",
            NULL,
            stereo_plugin_port_groups,
            &mb_ringmod_sc_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



