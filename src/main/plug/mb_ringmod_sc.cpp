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
#include <lsp-plug.in/plug-fw/core/AudioBuffer.h>
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
            vChannels           = NULL;
            vBuffer             = NULL;
            vEmptyBuffer        = NULL;

            // Pre-mixing ports
            sPremix.fInToSc     = GAIN_AMP_M_INF_DB;
            sPremix.fInToLink   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc   = GAIN_AMP_M_INF_DB;
            sPremix.fScToIn     = GAIN_AMP_M_INF_DB;
            sPremix.fScToLink   = GAIN_AMP_M_INF_DB;

            sPremix.pInToSc     = NULL;
            sPremix.pInToLink   = NULL;
            sPremix.pLinkToIn   = NULL;
            sPremix.pLinkToSc   = NULL;
            sPremix.pScToIn     = NULL;
            sPremix.pScToLink   = NULL;

            nType               = SC_TYPE_EXTERNAL;
            nSource             = SC_SRC_LEFT_RIGHT;
            nMode               = MODE_IIR;
            nLatency            = 0;
            fScGain             = GAIN_AMP_0_DB;

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

            bSyncFilters        = false;

            // Bind split ports
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX-1; ++i)
            {
                split_t *s          = &vSplits[i];

                s->pEnabled         = NULL;
                s->pFreq            = NULL;
            }

            // Bind split ports
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t *b           = &vBands[i];

                b->fFreqStart       = 0.0f;
                b->fFreqEnd         = 0.0f;
                b->fTauRelease      = 0.0f;
                b->nHold            = 0;
                b->nLatency         = 0;
                b->nDuck            = 0;
                b->fStereoLink      = 0.0f;

                b->bEnabled         = false;
                b->bOn              = false;

                b->pSolo            = NULL;
                b->pMute            = NULL;
                b->pEnable          = NULL;
                b->pLookahead       = NULL;
                b->pHold            = NULL;
                b->pRelease         = NULL;
                b->pDuck            = NULL;
                b->pAmount          = NULL;
                b->pFreqEnd         = NULL;
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
            size_t alloc            = szof_channels + // v_channels
                                      buf_sz + // vBuffer
                                      buf_sz + // vEmptyBuffer
                                      nChannels * ( // channel_t::
                                          buf_sz * 3 + // vTmpIn, vTmpLink, vTmpSc
                                          meta::mb_ringmod_sc::BANDS_MAX * ( // ch_band_t::
                                              buf_sz // vScData
                                          )
                                      );

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vBuffer                 = advance_ptr_bytes<float>(ptr, buf_sz);
            vEmptyBuffer            = advance_ptr_bytes<float>(ptr, buf_sz);

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

                    cb->sScDelay.construct();

                    cb->nHold               = 0;
                    cb->fPeak               = GAIN_AMP_M_INF_DB;

                    cb->vScData             = advance_ptr_bytes<float>(ptr, buf_sz);

                    cb->pReduction          = NULL;
                }

                c->vIn                  = NULL;
                c->vSc                  = NULL;
                c->vLink                = NULL;
                c->vOut                 = NULL;

                c->vInPtr               = NULL;
                c->vScPtr               = NULL;
                c->vOutPtr              = NULL;

                c->vTmpIn               = advance_ptr_bytes<float>(ptr, buf_sz);
                c->vTmpLink             = advance_ptr_bytes<float>(ptr, buf_sz);
                c->vTmpSc               = advance_ptr_bytes<float>(ptr, buf_sz);

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

            // Initialize buffers
            dsp::fill_zero(vEmptyBuffer, BUFFER_SIZE);
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

                    cb->sScDelay.init(sc_max_delay);

                    c->sCrossover.set_handler(j, process_band, this, c);
                    c->sScCrossover.set_handler(j, process_sc_band, this, c);
                }
            }
        }

        void mb_ringmod_sc::update_premix()
        {
            sPremix.fInToSc     = (sPremix.pInToSc != NULL)     ? sPremix.pInToSc->value()      : GAIN_AMP_M_INF_DB;
            sPremix.fInToLink   = (sPremix.pInToLink != NULL)   ? sPremix.pInToLink->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn   = (sPremix.pLinkToIn != NULL)   ? sPremix.pLinkToIn->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc   = (sPremix.pLinkToSc != NULL)   ? sPremix.pLinkToSc->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fScToIn     = (sPremix.pScToIn != NULL)     ? sPremix.pScToIn->value()      : GAIN_AMP_M_INF_DB;
            sPremix.fScToLink   = (sPremix.pScToLink != NULL)   ? sPremix.pScToLink->value()    : GAIN_AMP_M_INF_DB;
        }

        size_t mb_ringmod_sc::build_split_plan(band_t **plan)
        {
            size_t plan_size  = 0;

            // Make unordered list of enabled bands
            band_t *b               = &vBands[0];
            b->bEnabled             = true;
            b->fFreqStart           = 0.0f;
            plan[plan_size++]       = b;

            for (size_t i=1; i<meta::mb_ringmod_sc::BANDS_MAX-1; ++i)
            {
                split_t *s              = &vSplits[i-1];
                b                       = &vBands[i];
                b->bEnabled             = s->pEnabled->value() >= 0.5f;
                b->fFreqStart           = s->pFreq->value();

                if (b->bEnabled)
                    plan[plan_size++]       = b;
            }

            // Sort plan in frequency-ascending order
            // plan[0] is always associated with lowest band
            if (plan_size > 2)
            {
                for (size_t si=1; si < plan_size-1; ++si)
                    for (size_t sj=si+1; sj < plan_size; ++sj)
                    {
                        if (plan[si]->fFreqStart > plan[sj]->fFreqStart)
                            lsp::swap(plan[si], plan[sj]);
                    }
            }

            // Adjust end frequency for each band after sort
            for (size_t j=0; j<plan_size-1; ++j)
                plan[j]->fFreqEnd           = plan[j+1]->fFreqStart;
            plan[plan_size-1]->fFreqEnd         = fSampleRate * 0.5f;

            return plan_size;
        }

        size_t mb_ringmod_sc::decode_iir_slope(size_t slope)
        {
            switch (slope)
            {
                case 0: return dspu::CROSS_SLOPE_LR2;
                case 1: return dspu::CROSS_SLOPE_LR4;
                case 2: return dspu::CROSS_SLOPE_LR8;
                case 3: return dspu::CROSS_SLOPE_LR12;
                default: break;
            }
            return dspu::CROSS_SLOPE_OFF;
        }

        void mb_ringmod_sc::update_settings()
        {
            // TODO
//            bool bypass             = pBypass->value() >= 0.5f;

            // Update pre-mix matrix
            update_premix();

            // Update sidechain processing
            const uint32_t old_mode = nMode;

            nType                   = pType->value();
            nSource                 = (pSource != NULL) ? pSource->value() : SC_SRC_LEFT_RIGHT;
            nMode                   = pMode->value();

            if (nMode != old_mode)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    c->sDryDelay.clear();
                    c->sFFTCrossover.clear();
                    c->sFFTScCrossover.clear();
                }
            }

            // Build split plan
            band_t *plan[meta::mb_ringmod_sc::BANDS_MAX];
            build_split_plan(plan);

            // Update crossover split points
            if (nMode == MODE_IIR)
            {
                const size_t iir_slope  = decode_iir_slope(pSlope->value());

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c = &vChannels[i];

                    for (size_t j=1; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                    {
                        // Configure split point
                        band_t * const b        = &vBands[j];
                        const size_t slope      = (b->bEnabled) ? iir_slope : dspu::CROSS_SLOPE_OFF;
                        const size_t spi        = j - 1;
                        c->sCrossover.set_slope(spi, slope);
                        c->sCrossover.set_frequency(spi, b->fFreqStart);

                        c->sScCrossover.set_slope(spi, slope);
                        c->sScCrossover.set_frequency(spi, b->fFreqStart);
                    }

                    if (c->sCrossover.needs_reconfiguration())
                    {
                        bSyncFilters        = true;
                        c->sCrossover.reconfigure();
                    }
                    if (c->sScCrossover.needs_reconfiguration())
                    {
                        bSyncFilters        = true;
                        c->sScCrossover.reconfigure();
                    }
                }
            }
            else // nMode = MODE_SPM
            {
                const float  fft_slope  = (size_t(pSlope->value()) + 1) * -12.0f;

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c = &vChannels[i];

                    for (size_t j=0; j<meta::mb_ringmod_sc::BANDS_MAX; ++j)
                    {
                        band_t * const b    = &vBands[j];

                        c->sFFTCrossover.enable_band(j, b->bEnabled);
                        if (b->bEnabled)
                        {
                            const bool lpf_on   = b->fFreqEnd < fSampleRate * 0.5f;
                            const bool hpf_on   = b->fFreqStart > 0.0f;

                            c->sFFTCrossover.set_lpf(j, b->fFreqEnd, fft_slope, lpf_on);
                            c->sFFTCrossover.set_hpf(j, b->fFreqStart, fft_slope, hpf_on);

                            c->sFFTScCrossover.set_lpf(j, b->fFreqEnd, fft_slope, lpf_on);
                            c->sFFTScCrossover.set_hpf(j, b->fFreqStart, fft_slope, hpf_on);
                        }
                    }

                    if (c->sFFTCrossover.needs_update())
                    {
                        bSyncFilters        = true;
                        c->sFFTCrossover.update_settings();
                    }
                    if (c->sFFTScCrossover.needs_update())
                    {
                        bSyncFilters        = true;
                        c->sFFTScCrossover.update_settings();
                    }
                }
            }

            // Compute settings for each band
            bool has_solo       = false;
            nLatency            = 0;
            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t * const b    = &vBands[i];
                const float release = b->pRelease->value();

                b->fTauRelease      = 1.0f - expf(logf(1.0f - M_SQRT1_2) / (dspu::millis_to_samples(fSampleRate, release)));
                b->nHold            = dspu::millis_to_samples(fSampleRate, b->pHold->value());
                b->nLatency         = dspu::millis_to_samples(fSampleRate, b->pLookahead->value());
                b->nDuck            = nLatency + dspu::millis_to_samples(fSampleRate, b->pDuck->value());
                b->fStereoLink      = (b->pStereoLink != NULL) ? lsp_max(b->pStereoLink->value() * 0.01f, 0.0f) : 0.0f;

                if (!has_solo)
                    has_solo            = b->pSolo->value() >= 0.5f;

                nLatency            = lsp_max(nLatency, b->nLatency);
            }

            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t * const b    = &vBands[i];
                const bool solo     = b->pSolo->value() >= 0.5f;
                const bool mute     = b->pMute->value() >= 0.5f;

                b->bOn              = (b->bEnabled) && (!mute) && ((!has_solo) || (solo));
                b->nLatency         = nLatency - b->nLatency;
            }

            // Report latency
            set_latency(nLatency);
        }

        void mb_ringmod_sc::ui_activated()
        {
            bSyncFilters        = true;
        }

        void mb_ringmod_sc::process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t samples)
        {
            // TODO
        }

        void mb_ringmod_sc::premix_channels(size_t samples)
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c = &vChannels[i];

                // Get pointers to buffers and advance position
                float * const in_buf    = c->vIn;
                float * const sc_buf    = c->vSc;
                float * const link_buf  = c->vLink;
                float * const out_buf   = c->vOut;

                c->vInPtr               = in_buf;
                c->vScPtr               = sc_buf;
                c->vLinkPtr             = link_buf;
                c->vOutPtr              = out_buf;

                // Update pointers
                c->vIn                 += samples;
                c->vSc                 += samples;
                c->vLink                = (c->vLink != NULL) ? c->vLink + samples : NULL;
                c->vOut                += samples;

                // Perform transformation
                // (Sc, Link) -> In
                if ((sc_buf != NULL) && (sPremix.fScToIn > GAIN_AMP_M_INF_DB))
                {
                    c->vInPtr               = c->vTmpIn;
                    dsp::fmadd_k4(c->vInPtr, in_buf, sc_buf, sPremix.fScToIn, samples);

                    if ((link_buf != NULL) && (sPremix.fLinkToIn > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vInPtr, link_buf, sPremix.fLinkToIn, samples);
                }
                else if ((link_buf != NULL) && (sPremix.fLinkToIn > GAIN_AMP_M_INF_DB))
                {
                    c->vInPtr               = c->vTmpIn;
                    dsp::fmadd_k4(c->vInPtr, in_buf, link_buf, sPremix.fLinkToIn, samples);
                }

                // (In, Link) -> Sc
                if (sPremix.fInToSc > GAIN_AMP_M_INF_DB)
                {
                    c->vScPtr               = c->vTmpSc;
                    if (sc_buf != NULL)
                        dsp::fmadd_k4(c->vScPtr, sc_buf, in_buf, sPremix.fInToSc, samples);
                    else
                        dsp::mul_k3(c->vScPtr, in_buf, sPremix.fInToSc, samples);

                    if ((link_buf != NULL) && (sPremix.fLinkToSc > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vScPtr, link_buf, sPremix.fLinkToSc, samples);
                }
                else if ((link_buf != NULL) && (sPremix.fLinkToSc > GAIN_AMP_M_INF_DB))
                {
                    c->vScPtr               = c->vTmpSc;
                    if (sc_buf != NULL)
                        dsp::fmadd_k4(c->vScPtr, sc_buf, link_buf, sPremix.fLinkToSc, samples);
                    else
                        dsp::mul_k3(c->vScPtr, link_buf, sPremix.fLinkToSc, samples);
                }

                // (In, Sc) -> Link
                if (sPremix.fInToLink > GAIN_AMP_M_INF_DB)
                {
                    c->vLinkPtr             = c->vTmpLink;
                    if (link_buf != NULL)
                        dsp::fmadd_k4(c->vLinkPtr, link_buf, in_buf, sPremix.fInToLink, samples);
                    else
                        dsp::mul_k3(c->vLinkPtr, in_buf, sPremix.fInToLink, samples);

                    if ((sc_buf != NULL) && (sPremix.fScToLink > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vLinkPtr, sc_buf, sPremix.fScToLink, samples);
                }
                else if ((sc_buf != NULL) && (sPremix.fScToLink > GAIN_AMP_M_INF_DB))
                {
                    c->vLinkPtr             = c->vTmpLink;
                    if (link_buf != NULL)
                        dsp::fmadd_k4(c->vLinkPtr, link_buf, sc_buf, sPremix.fScToLink, samples);
                    else
                        dsp::mul_k3(c->vLinkPtr, sc_buf, sPremix.fScToLink, samples);
                }
            }
        }

        void mb_ringmod_sc::process_sidechain_type(size_t samples)
        {
            // Select the source for the specific type of sidechain
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c = &vChannels[i];
                float *buf          = (nType == SC_TYPE_EXTERNAL) ? c->vScPtr :
                                      (nType == SC_TYPE_SHM_LINK) ? c->vLinkPtr :
                                      c->vInPtr;

                c->vScPtr           = (buf != NULL) ? buf : vEmptyBuffer;
            }

            // Apply sidechain pre-processing depending on selected source (stereo only)
            if (nChannels <= 1)
                return;

            channel_t * const l = &vChannels[0];
            channel_t * const r = &vChannels[1];

            switch (nSource)
            {
                case SC_SRC_RIGHT_LEFT:
                    lsp::swap(l->vScPtr[0], l->vScPtr[1]);
                    break;

                case SC_SRC_LEFT:
                    r->vScPtr[1]    = l->vScPtr[0];
                    break;

                case SC_SRC_RIGHT:
                    l->vScPtr[0]    = r->vScPtr[1];
                    break;

                case SC_SRC_MID_SIDE:
                    dsp::lr_to_ms(l->vTmpSc, r->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = l->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_SIDE_MID:
                    dsp::lr_to_ms(r->vTmpSc, l->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = l->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_MIDDLE:
                    dsp::lr_to_mid(r->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = r->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_SIDE:
                    dsp::lr_to_side(r->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = r->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_MIN:
                    dsp::pamin3(r->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = r->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_MAX:
                    dsp::pamax3(r->vTmpSc, l->vScPtr, r->vScPtr, samples);
                    l->vScPtr   = r->vTmpSc;
                    r->vScPtr   = r->vTmpSc;
                    break;

                case SC_SRC_LEFT_RIGHT: // already properly mapped
                default:
                    break;
            }
        }

        void mb_ringmod_sc::process_sidechain_envelope(size_t samples)
        {
            // Process sidechain envelope for each band
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c        = &vChannels[i];

                if (nMode == MODE_IIR)
                    c->sScCrossover.process(c->vScPtr, samples);
                else
                    c->sFFTScCrossover.process(c->vScPtr, samples);
            }

            // Perform stereo linking between left and right channels for each band
            if (nChannels < 2)
                return;

            for (size_t i=0; i<meta::mb_ringmod_sc::BANDS_MAX; ++i)
            {
                band_t * const b        = &vBands[i];
                if (!b->bOn)
                    continue;

                ch_band_t * const clb   = &vChannels[0].vBands[i];
                ch_band_t * const crb   = &vChannels[1].vBands[i];

                const float slink   = b->fStereoLink;
                if (slink <= 0.0f)
                    continue;

                float *lbuf         = clb->vScData;
                float *rbuf         = crb->vScData;

                // For both channels: find the minimum one and try to raise to maximum one
                // proportionally to the stereo link setup
                for (size_t j=0; j<samples; ++j)
                {
                    const float ls      = lbuf[j];
                    const float rs      = rbuf[j];
                    if (ls < rs)
                        lbuf[j]             = ls + (rs - ls) * slink;
                    else
                        rbuf[j]             = rs + (ls - rs) * slink;
                }
            }
        }

        void mb_ringmod_sc::process_sc_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t samples)
        {
            mb_ringmod_sc *self     = static_cast<mb_ringmod_sc *>(object);
            channel_t *c            = static_cast<channel_t *>(subject);
            ch_band_t *cb           = &c->vBands[band];
            band_t *b               = &self->vBands[band];

            // Transform sidechain signal into envelope
            const float sc_gain     = self->fScGain;
            uint32_t hold           = cb->nHold;
            float peak              = cb->fPeak;
            float * const dst       = cb->vScData;

            for (size_t i=0; i<samples; ++i)
            {
                float s             = fabsf(data[i] * sc_gain);  // Rectify input
                if (peak > s)
                {
                    // Current rectified sample is below the peak value
                    if (hold > 0)
                    {
                        s                   = peak;             // Hold peak value
                        --hold;
                    }
                    else
                    {
                        s                   = peak + (s - peak) * b->fTauRelease;
                        peak                = s;
                    }
                }
                else
                {
                    peak                = s;
                    hold                = b->nHold;             // Reset hold counter
                }
                dst[i]              = s;
            }

            // Update parameters
            cb->nHold           = hold;
            cb->fPeak           = peak;

            // Now push the buffer contents to the ring buffer
            cb->sScDelay.append(dst, samples);
            if (!b->bOn)
                return;

            // Apply latency compensation, lookahead and ducking
            if (self->nLatency > 0)
                cb->sScDelay.get(dst, samples + self->nLatency, samples);

            if (b->nLatency < self->nLatency)
            {
                cb->sScDelay.get(self->vBuffer, samples + b->nLatency, samples);
                dsp::pmax2(dst, self->vBuffer, samples);
            }
            if (b->nDuck > self->nLatency)
            {
                cb->sScDelay.get(self->vBuffer, samples + b->nDuck, samples);
                dsp::pmax2(dst, self->vBuffer, samples);
            }
        }

        void mb_ringmod_sc::process(size_t samples)
        {
            // Prepare audio channels
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c        = &vChannels[i];

                core::AudioBuffer *buf = (c->pShmIn != NULL) ? c->pShmIn->buffer<core::AudioBuffer>() : NULL;

                // Initialize pointers
                c->vIn              = c->pIn->buffer<float>();
                c->vSc              = c->pSc->buffer<float>();
                c->vLink            = ((buf != NULL) && (buf->active())) ? buf->buffer() : NULL;
                c->vOut             = c->pOut->buffer<float>();
            }

            // Process data
            for (size_t offset = 0; offset < samples;)
            {
                const size_t to_process     = lsp_min(samples - offset, BUFFER_SIZE);

                // Do processing
                premix_channels(to_process);
                process_sidechain_type(to_process);
                process_sidechain_envelope(to_process);

                // Updte offset
                offset                     += to_process;
            }
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


