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

#ifndef PRIVATE_PLUGINS_MB_RINGMOD_SC_H_
#define PRIVATE_PLUGINS_MB_RINGMOD_SC_H_

#include <lsp-plug.in/dsp-units/util/Analyzer.h>
#include <lsp-plug.in/dsp-units/util/Crossover.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/FFTCrossover.h>
#include <lsp-plug.in/dsp-units/util/RingBuffer.h>
#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/mb_ringmod_sc.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class mb_ringmod_sc: public plug::Module
        {
            protected:
                enum mode_t
                {
                    CD_MONO,
                    CD_STEREO,
                    CD_X2_STEREO
                };

                typedef struct premix_t
                {
                    float               fInToSc;                // Input -> Sidechain mix
                    float               fInToLink;              // Input -> Link mix
                    float               fLinkToIn;              // Link -> Input mix
                    float               fLinkToSc;              // Link -> Sidechain mix
                    float               fScToIn;                // Sidechain -> Input mix
                    float               fScToLink;              // Sidechain -> Link mix

                    float              *vIn[2];                 // Input buffer
                    float              *vOut[2];                // Output buffer
                    float              *vSc[2];                 // Sidechain buffer
                    float              *vLink[2];               // Link buffer

                    float              *vTmpIn[2];              // Replacement buffer for input
                    float              *vTmpLink[2];            // Replacement buffer for link
                    float              *vTmpSc[2];              // Replacement buffer for sidechain

                    plug::IPort        *pInToSc;                // Input -> Sidechain mix
                    plug::IPort        *pInToLink;              // Input -> Link mix
                    plug::IPort        *pLinkToIn;              // Link -> Input mix
                    plug::IPort        *pLinkToSc;              // Link -> Sidechain mix
                    plug::IPort        *pScToIn;                // Sidechain -> Input mix
                    plug::IPort        *pScToLink;              // Sidechain -> Link mix
                } premix_t;

                typedef struct split_t
                {
                    plug::IPort        *pEnabled;               // Enable port
                    plug::IPort        *pFreq;                  // Split frequency
                } split_t;

                typedef struct band_t
                {
                    plug::IPort        *pSolo;                  // Solo band
                    plug::IPort        *pMute;                  // Mute band
                    plug::IPort        *pEnable;                // Enable band
                    plug::IPort        *pLookahead;             // Look-ahead time
                    plug::IPort        *pHold;                  // Hold time
                    plug::IPort        *pRelease;               // Release time
                    plug::IPort        *pDuck;                  // Duck time
                    plug::IPort        *pAmount;                // Amount
                    plug::IPort        *pFreqEnd;               // Frequency range end
                    plug::IPort        *pStereoLink;            // Stereo linking
                } band_t;

                typedef struct ch_band_t
                {
                    dspu::Delay         sPreDelay;              // Pre-delay for band-limited signal
                    dspu::Delay         sPostDelay;             // Post-delay for band-limited signal
                    dspu::RingBuffer    sScDelay;               // Delay for sidechain

                    plug::IPort        *pReduction;             // Reduction level meters
                } ch_band_t;

                typedef struct channel_t
                {
                    dspu::Bypass        sBypass;                // Bypass
                    dspu::Delay         sDryDelay;              // Delay for dry (unprocessed) signal
                    dspu::Crossover     sCrossover;             // Crossover
                    dspu::Crossover     sScCrossover;           // Sidechain Crossover
                    dspu::FFTCrossover  sFFTCrossover;          // FFT crossover
                    dspu::FFTCrossover  sFFTScCrossover;        // Sidechain FFT crossover
                    ch_band_t           vBands[meta::mb_ringmod_sc::BANDS_MAX]; // Band processors

                    plug::IPort        *pIn;                    // Input port
                    plug::IPort        *pOut;                   // Output port
                    plug::IPort        *pSc;                    // Sidechain port
                    plug::IPort        *pShmIn;                 // Shared memory link input
                } channel_t;

            protected:
                size_t              nChannels;              // Number of channels
                channel_t          *vChannels;              // Delay channels
                dspu::Analyzer      sAnalyzer;              // Analyzer
                split_t             vSplits[meta::mb_ringmod_sc::BANDS_MAX - 1];    // Band splits
                band_t              vBands[meta::mb_ringmod_sc::BANDS_MAX];         // Bands
                float              *vBuffer;                // Temporary buffer for audio processing
                premix_t            sPremix;                // Sidechain pre-mix

                plug::IPort        *pBypass;                // Bypass
                plug::IPort        *pGainIn;                // Input gain
                plug::IPort        *pGainSc;                // Sidechain gain
                plug::IPort        *pGainOut;               // Output gain
                plug::IPort        *pOutIn;                 // Output processed input signal
                plug::IPort        *pOutSc;                 // Output sidechain signal
                plug::IPort        *pActive;                // Activity
                plug::IPort        *pType;                  // Type of sidechain
                plug::IPort        *pMode;                  // Mode of sidechain
                plug::IPort        *pSlope;                 // Slope of sidechain
                plug::IPort        *pZoom;                  // Zoom
                plug::IPort        *pReactivity;            // FFT Reactivity
                plug::IPort        *pShift;                 // FFT shift
                plug::IPort        *pFilterMesh;            // Filter meshes
                plug::IPort        *pMeterMesh;             // Metering meshes
                plug::IPort        *pSource;                // Sidechain source

                uint8_t            *pData;                  // Allocated data

            protected:
                static void         process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count);
                static void         process_sc_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count);
                static size_t       select_fft_rank(size_t sample_rate);

            protected:
                void                do_destroy();

            public:
                explicit mb_ringmod_sc(const meta::plugin_t *meta);
                mb_ringmod_sc (const mb_ringmod_sc &) = delete;
                mb_ringmod_sc (mb_ringmod_sc &&) = delete;
                virtual ~mb_ringmod_sc() override;

                mb_ringmod_sc & operator = (const mb_ringmod_sc &) = delete;
                mb_ringmod_sc & operator = (mb_ringmod_sc &&) = delete;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_MB_RINGMOD_SC_H_ */

