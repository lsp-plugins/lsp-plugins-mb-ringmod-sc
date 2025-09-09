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

#ifndef PRIVATE_META_MB_RINGMOD_SC_H_
#define PRIVATE_META_MB_RINGMOD_SC_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct mb_ringmod_sc
        {
            static constexpr size_t BANDS_MAX           = 8;
            static constexpr size_t FFT_MESH_POINTS     = 640;

            static constexpr float  HOLD_MIN            = 0.0f;
            static constexpr float  HOLD_MAX            = 10.0f;
            static constexpr float  HOLD_DFL            = 0.0f;
            static constexpr float  HOLD_STEP           = 0.005f;

            static constexpr float  RELEASE_MIN         = 0.0f;
            static constexpr float  RELEASE_MAX         = 100.0f;
            static constexpr float  RELEASE_DFL         = 0.0f;
            static constexpr float  RELEASE_STEP        = 0.01f;

            static constexpr float  LOOKAHEAD_MIN       = 0.0f;
            static constexpr float  LOOKAHEAD_MAX       = 10.0f;
            static constexpr float  LOOKAHEAD_DFL       = 0.0f;
            static constexpr float  LOOKAHEAD_STEP      = 0.005f;

            static constexpr float  DUCK_MIN            = 0.0f;
            static constexpr float  DUCK_MAX            = 10.0f;
            static constexpr float  DUCK_DFL            = 0.0f;
            static constexpr float  DUCK_STEP           = 0.005f;

            static constexpr float  AMOUNT_MIN          = -12.0f;
            static constexpr float  AMOUNT_MAX          = 0.0f;
            static constexpr float  AMOUNT_DFL          = 0.0f;
            static constexpr float  AMOUNT_STEP         = 0.01f;

            static constexpr float  REACT_TIME_MIN      = 0.000;
            static constexpr float  REACT_TIME_MAX      = 1.000;
            static constexpr float  REACT_TIME_DFL      = 0.200;
            static constexpr float  REACT_TIME_STEP     = 0.001;

            static constexpr float  ZOOM_MIN            = GAIN_AMP_M_18_DB;
            static constexpr float  ZOOM_MAX            = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_DFL            = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_STEP           = 0.0125f;

            static constexpr float  FREQ_MIN            = 10.0f;
            static constexpr float  FREQ_MAX            = 20000.0f;
            static constexpr float  FREQ_DFL            = 1000.0f;
            static constexpr float  FREQ_STEP           = 0.002f;

            static constexpr float  OUT_FREQ_MIN        = 0.0f;
            static constexpr float  OUT_FREQ_MAX        = MAX_SAMPLE_RATE;
            static constexpr float  OUT_FREQ_DFL        = 1000.0f;
            static constexpr float  OUT_FREQ_STEP       = 0.002f;

        } mb_ringmod_sc;

        // Plugin type metadata
        extern const plugin_t mb_ringmod_sc_mono;
        extern const plugin_t mb_ringmod_sc_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_MB_RINGMOD_SC_H_ */
