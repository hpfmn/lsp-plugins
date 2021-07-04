/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 27 Jun 2021
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#include <core/pseudorandom/VelvetNoise.h>
#include <core/debug.h>
#include <dsp/dsp.h>

#define MIN_WINDOW_WIDTH 1.0f
#define DFL_WINDOW_WIDTH 10.0f

#define BUF_LIM_SIZE 2048

namespace lsp

{
    VelvetNoise::VelvetNoise()
    {
        construct();
    }

    void VelvetNoise::construct()
    {
        enCore                      = VN_CORE_MLS;

        enVelvetType                = VN_VELVET_OVN;

        sCrushParams.bCrush         = false;
        sCrushParams.fCrushProb     = 0.5f;

        fWindowWidth                = DFL_WINDOW_WIDTH;
        fARNdelta                   = 0.5f;
        fAmplitude                  = 1.0f;
        fOffset                     = 0.0f;

        pData                       = NULL;
        vBuffer                     = NULL;
    }

    VelvetNoise::~VelvetNoise()
    {
        destroy();
    }

    void VelvetNoise::destroy()
    {
        free_aligned(pData);
        pData = NULL;

        if (vBuffer != NULL)
        {
            delete [] vBuffer;
            vBuffer = NULL;
        }
    }

    void VelvetNoise::init_buffers()
    {
        // 1X buffer for processing.
        size_t samples = BUF_LIM_SIZE;

        float *ptr = alloc_aligned<float>(pData, samples);
        if (ptr == NULL)
            return;

        lsp_guard_assert(float *save = ptr);

        vBuffer = ptr;
        ptr += BUF_LIM_SIZE;

        lsp_assert(ptr <= &save[samples]);
    }

    void VelvetNoise::init(uint32_t randseed, uint8_t mlsnbits, MLS::mls_t mlsseed)
    {
        sRandomizer.init(randseed);

        sMLS.set_n_bits(mlsnbits);
        sMLS.set_state(mlsseed);
        sMLS.update_settings();

        init_buffers();
    }

    void VelvetNoise::init()
    {
        sRandomizer.init();

        // Simply use defaults in the class.
        sMLS.update_settings();

        init_buffers();
    }

    void VelvetNoise::update_settings()
    {
        if (!needs_update())
            return;

        sMLS.set_amplitude(fAmplitude);
        sMLS.set_offset(fOffset);

        sMLS.update_settings();
    }

    float VelvetNoise::get_random_value()
    {
        return sRandomizer.random(RND_LINEAR);
    }

    float VelvetNoise::get_spike()
    {
        switch (enCore)
        {
            case VN_CORE_MLS:
                return sMLS.single_sample_processor();
            default:
            case VN_CORE_LCG:
                return fAmplitude * 2.0f * roundf(get_random_value()) - 1.0f + fOffset;
        }
    }

    float VelvetNoise::get_crushed_spike()
    {
        switch (enCore)
        {
            // Needs improvement
            case VN_CORE_MLS:
            {
                if (get_random_value() > sCrushParams.fCrushProb)
                    return sMLS.single_sample_processor();
                else
                    return 0.0f;
            }

            default:
            case VN_CORE_LCG:
            {
                float spike = 0.0f;

                if (spike > sCrushParams.fCrushProb)
                    spike = 1.0f;

                return fAmplitude * 2.0f * spike - 1.0f + fOffset;
            }
        }
    }

    void VelvetNoise::do_process(float *dst, size_t count)
    {

        switch (enVelvetType)
        {

            case VN_VELVET_OVN:
            {
                dsp::fill_zero(dst, count);

                size_t idx = 0;
                size_t scan = 0;

                while (idx < count)
                {
                    idx = scan * fWindowWidth + get_random_value() * (fWindowWidth - 1.0f);

                    if (sCrushParams.bCrush)
                        dst[idx] = get_crushed_spike();
                    else
                        dst[idx] = get_spike();
                }

                ++scan;
            }
            break;

            case VN_VELVET_OVNA:
            {
                dsp::fill_zero(dst, count);

                size_t idx = 0;
                size_t scan = 0;

                while (idx < count)
                {
                    idx = scan * fWindowWidth + get_random_value() * fWindowWidth;

                    if (sCrushParams.bCrush)
                        dst[idx] = get_crushed_spike();
                    else
                        dst[idx] = get_spike();
                }

                ++scan;
            }
            break;

            case VN_VELVET_ARN:
            {
                dsp::fill_zero(dst, count);

                size_t idx = 0;
                size_t scan = 0;

                while (idx < count)
                {
                    idx += 1.0f + (1.0f - fARNdelta) * (fWindowWidth - 1.0f) + 2.0f * fARNdelta * (fWindowWidth - 1.0f) * get_random_value();

                    if (sCrushParams.bCrush)
                        dst[idx] = get_crushed_spike();
                    else
                        dst[idx] = get_spike();
                }

                ++scan;
            }
            break;

            case VN_VELVET_TRN:
            {
                while (count--)
                {
                    float value = roundf(fWindowWidth * (get_random_value() - 0.5f) / (fWindowWidth - 1.0f));

                    // Needs Improvement
                    if (sCrushParams.bCrush)
                    {
                        float multiplier = 0.0f;

                        if (get_random_value() > sCrushParams.fCrushProb)
                            multiplier = 1.0f;

                        *(dst++) = fAmplitude * multiplier * abs(value) - fOffset;
                    }
                    else
                    {
                        *(dst++) = fAmplitude * value - fOffset;
                    }
                }
            }
            break;

            default:
            case VN_VELVET_MAX:
            {
                dsp::fill_zero(dst, count);
            }
            break;
        }
    }

    void VelvetNoise::process_add(float *dst, const float *src, size_t count)
    {
        if (src != NULL)
            dsp::copy(dst, src, count);
        else
            dsp::fill_zero(dst, count);

        while (count > 0)
        {
            size_t to_do = (count > BUF_LIM_SIZE) ? BUF_LIM_SIZE : count;

            do_process(vBuffer, to_do);
            dsp::add2(dst, vBuffer, to_do);

            dst     += to_do;
            count   -= to_do;
        }
    }

    void VelvetNoise::process_mul(float *dst, const float *src, size_t count)
    {
        if (src != NULL)
            dsp::copy(dst, src, count);
        else
            dsp::fill_zero(dst, count);

        while (count > 0)
        {
            size_t to_do = (count > BUF_LIM_SIZE) ? BUF_LIM_SIZE : count;

            do_process(vBuffer, to_do);
            dsp::mul2(dst, vBuffer, to_do);

            dst     += to_do;
            count   -= to_do;
        }
    }

    void VelvetNoise::process_overwrite(float *dst, size_t count)
    {
        while (count > 0)
        {
            size_t to_do = (count > BUF_LIM_SIZE) ? BUF_LIM_SIZE : count;

            do_process(vBuffer, to_do);
            dsp::copy(dst, vBuffer, to_do);

            dst     += to_do;
            count   -= to_do;
        }
    }
}
