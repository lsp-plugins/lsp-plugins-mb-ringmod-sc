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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/bits.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/misc/envelope.h>
#include <lsp-plug.in/dsp-units/misc/windows.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>

#include <private/plugins/mb_ringmod_sc.h>

namespace lsp
{
    namespace plugins
    {
        /* The size of temporary buffer for audio processing */
        static constexpr size_t BUFFER_SIZE         = 0x200;

        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::mb_ringmod_sc_mono,
            &meta::mb_ringmod_sc_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new mb_ringmod_sc(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        mb_ringmod_sc::mb_ringmod_sc(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_out_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels       = NULL;
            vBuffer         = NULL;

            // Pre-mixing ports
            sPremix.fInToSc     = GAIN_AMP_M_INF_DB;
            sPremix.fInToLink   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc   = GAIN_AMP_M_INF_DB;
            sPremix.fScToIn     = GAIN_AMP_M_INF_DB;
            sPremix.fScToLink   = GAIN_AMP_M_INF_DB;

            for (size_t i=0; i<2; ++i)
            {
                sPremix.vIn[i]      = NULL;
                sPremix.vOut[i]     = NULL;
                sPremix.vSc[i]      = NULL;
                sPremix.vLink[i]    = NULL;
                sPremix.vTmpIn[i]   = NULL;
                sPremix.vTmpSc[i]   = NULL;
                sPremix.vTmpLink[i] = NULL;
            }

            sPremix.pInToSc     = NULL;
            sPremix.pInToLink   = NULL;
            sPremix.pLinkToIn   = NULL;
            sPremix.pLinkToSc   = NULL;
            sPremix.pScToIn     = NULL;
            sPremix.pScToLink   = NULL;

            pBypass             = NULL;
            pGainIn             = NULL;
            pGainSc             = NULL;
            pGainOut            = NULL;
            pOutIn              = NULL;
            pOutSc              = NULL;
            pActive             = NULL;
            pType               = NULL;
            pMode               = NULL;
            pSlope              = NULL;
            pZoom               = NULL;
            pReactivity         = NULL;
            pShift              = NULL;
            pFilterMesh         = NULL;
            pMeterMesh          = NULL;
            pSource             = NULL;

            // Bind split ports
            lsp_trace("Binding split ports");
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX-1; ++i)
            {
                split_t *s          = &vSplits[i];

                s->pEnabled         = NULL;
                s->pFreq            = NULL;
            }

            // Bind split ports
            lsp_trace("Binding band ports");
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t *b           = &vBands[i];

                b->pSolo            = NULL;
                b->pMute            = NULL;
                b->pEnable          = NULL;
                b->pLookahead       = NULL;
                b->pHold            = NULL;
                b->pRelease         = NULL;
                b->pDuck            = NULL;
                b->pAmount          = NULL;
                b->pFreqEnd         = NULL;
                if (nChannels > 1)
                    b->pStereoLink      = NULL;
            }

            pData               = NULL;
        }

        mb_ringmod_sc::~mb_ringmod_sc()
        {
            do_destroy();
        }

        void mb_ringmod_sc::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t alloc            = szof_channels + buf_sz;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vBuffer                 = advance_ptr_bytes<float>(ptr, buf_sz);

            // Initialize analyzer
            if (!sAnalyzer.init(nChannels * 2, meta::mb_ringmod_sc::FFT_RANK,
                MAX_SAMPLE_RATE, meta::mb_ringmod_sc::REFRESH_RATE))
                return;
            sAnalyzer.set_rank(meta::mb_ringmod_sc::FFT_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(dspu::envelope::WHITE_NOISE);
            sAnalyzer.set_window(meta::mb_ringmod_sc::FFT_WINDOW);
            sAnalyzer.set_rate(meta::mb_ringmod_sc::REFRESH_RATE);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();
                c->sDryDelay.construct();
                c->sCrossover.construct();
                c->sScCrossover.construct();
                c->sFFTCrossover.construct();
                c->sFFTScCrossover.construct();

                if (!c->sCrossover.init(meta::mb_ringmod_sc::BANDS_MAX, BUFFER_SIZE))
                    return;
                if (!c->sScCrossover.init(meta::mb_ringmod_sc::BANDS_MAX, BUFFER_SIZE))
                    return;

                for (size_t j=0; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                {
                    ch_band_t *cb   = &c->vBands[j];

                    cb->sPreDelay.destroy();
                    cb->sPostDelay.destroy();
                    cb->sScDelay.destroy();
                }

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pSc                  = NULL;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pIn);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pOut);

            // Bind sidechain audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pSc);

            // Bind stereo link
            SKIP_PORT("Stereo link name");
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pShmIn);

            // Pre-mixing ports
            lsp_trace("Binding pre-mix ports");
            SKIP_PORT("Show premix overlay");
            BIND_PORT(sPremix.pInToLink);
            BIND_PORT(sPremix.pLinkToIn);
            BIND_PORT(sPremix.pLinkToSc);
            BIND_PORT(sPremix.pInToSc);
            BIND_PORT(sPremix.pScToIn);
            BIND_PORT(sPremix.pScToLink);

            // Bind bypass
            lsp_trace("Binding common ports");
            BIND_PORT(pBypass);
            BIND_PORT(pGainIn);
            BIND_PORT(pGainSc);
            BIND_PORT(pGainOut);
            BIND_PORT(pOutIn);
            BIND_PORT(pOutSc);
            BIND_PORT(pActive);
            BIND_PORT(pType);
            BIND_PORT(pMode);
            BIND_PORT(pSlope);
            BIND_PORT(pZoom);
            SKIP_PORT("Band filter curves");
            BIND_PORT(pReactivity);
            BIND_PORT(pShift);
            BIND_PORT(pFilterMesh);
            BIND_PORT(pMeterMesh);

            if (nChannels > 1)
                BIND_PORT(pSource);

            // Bind split ports
            lsp_trace("Binding split ports");
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX-1; ++i)
            {
                split_t *s          = &vSplits[i];

                BIND_PORT(s->pEnabled);
                BIND_PORT(s->pFreq);
            }

            // Bind split ports
            lsp_trace("Binding band ports");
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t *b           = &vBands[i];

                BIND_PORT(b->pSolo);
                BIND_PORT(b->pMute);
                BIND_PORT(b->pEnable);
                BIND_PORT(b->pLookahead);
                BIND_PORT(b->pHold);
                BIND_PORT(b->pRelease);
                BIND_PORT(b->pDuck);
                BIND_PORT(b->pAmount);
                BIND_PORT(b->pFreqEnd);
                if (nChannels > 1)
                    BIND_PORT(b->pStereoLink);

                for (size_t j=0; j<nChannels; ++j)
                {
                    channel_t *c        = &vChannels[j];
                    ch_band_t *cb       = &c->vBands[i];

                    BIND_PORT(cb->pReduction);
                }
            }
        }

        void mb_ringmod_sc::destroy()
        {
            Module::destroy();
            do_destroy();
        }

        void mb_ringmod_sc::do_destroy()
        {
            // Destroy analyzer
            sAnalyzer.destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    c->sBypass.destroy();
                    c->sDryDelay.destroy();
                    c->sCrossover.destroy();
                    c->sScCrossover.destroy();
                    c->sFFTCrossover.destroy();
                    c->sFFTScCrossover.destroy();

                    for (size_t j=0; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                    {
                        ch_band_t *cb   = &c->vBands[j];

                        cb->sPreDelay.destroy();
                        cb->sPostDelay.destroy();
                        cb->sScDelay.destroy();
                    }
                }
                vChannels   = NULL;
            }

            vBuffer     = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        size_t mb_ringmod_sc::select_fft_rank(size_t sample_rate)
        {
            const size_t k = (sample_rate + meta::mb_ringmod_sc::FFT_XOVER_FREQ_MIN/2) / meta::mb_ringmod_sc::FFT_XOVER_FREQ_MIN;
            const size_t n = int_log2(k);
            return meta::mb_ringmod_sc::FFT_XOVER_RANK_MIN + n;
        }

        void mb_ringmod_sc::update_sample_rate(long sr)
        {
            const size_t fft_rank       = select_fft_rank(sr);
            const size_t in_max_delay   = dspu::millis_to_samples(sr, meta::mb_ringmod_sc::LOOKAHEAD_MAX) + BUFFER_SIZE;
            const size_t sc_max_delay   =
                in_max_delay +
                dspu::millis_to_samples(sr, meta::mb_ringmod_sc::DUCK_MAX) ;

            // Update analyzer's sample rate
            sAnalyzer.set_sample_rate(sr);

            // Update channels
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.init(sr);
                c->sDryDelay.init(in_max_delay);
                c->sCrossover.set_sample_rate(sr);
                c->sScCrossover.set_sample_rate(sr);
                c->sFFTCrossover.set_sample_rate(sr);
                c->sFFTScCrossover.set_sample_rate(sr);

                // Need to re-initialize FFT crossovers?
                if (fft_rank != c->sFFTCrossover.rank())
                {
                    c->sFFTCrossover.init(fft_rank, meta::mb_ringmod_sc::BANDS_MAX);
                    c->sFFTScCrossover.init(fft_rank, meta::mb_ringmod_sc::BANDS_MAX);
                    for (size_t j=0; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                    {
                        c->sFFTCrossover.set_handler(j, process_band, this, c);
                        c->sFFTScCrossover.set_handler(j, process_sc_band, this, c);
                    }
                    c->sFFTCrossover.set_phase(i);
                    c->sFFTScCrossover.set_phase(i);
                }

                for (size_t j=0; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                {
                    ch_band_t *cb   = &c->vBands[j];

                    cb->sPreDelay.init(in_max_delay);
                    cb->sPostDelay.init(in_max_delay);
                    cb->sScDelay.init(sc_max_delay);

                    c->sCrossover.set_handler(j, process_band, this, c);
                    c->sScCrossover.set_handler(j, process_sc_band, this, c);
                }
            }
        }

        void mb_ringmod_sc::update_settings()
        {
            // TODO
//            bool bypass             = pBypass->value() >= 0.5f;
        }

        void mb_ringmod_sc::process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count)
        {
            // TODO
        }

        void mb_ringmod_sc::process_sc_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count)
        {
            // TODO
        }

        void mb_ringmod_sc::process(size_t samples)
        {
            // TODO
            // Process each channel independently
//            for (size_t i=0; i<nChannels; ++i)
//            {
//                channel_t *c            = &vChannels[i];
//
//            }
        }

        void mb_ringmod_sc::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // TODO
            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pSc", c->pSc);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);

            v->write("pBypass", pBypass);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


