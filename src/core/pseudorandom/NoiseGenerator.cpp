/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 31 May 2021
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

#include <core/pseudorandom/NoiseGenerator.h>
#include <core/debug.h>

#define BUF_LIM_SIZE 2048

namespace lsp
{

    NoiseGenerator::NoiseGenerator()
    {
        construct();
    }

    NoiseGenerator::~NoiseGenerator()
    {
        destroy();
    }

    void NoiseGenerator::construct()
    {
        sRandParams.nSeed           =0;
        sRandParams.enFunc          = RND_LINEAR;

        sMLSParams.bSync            = true;
        sMLSParams.nBits            = sMLS.maximum_number_of_bits();
        sMLSParams.nSeed            = 0;

        sLCGParams.nSeed            = 0;
        sLCGParams.enDistribution   = LCG_UNIFORM;

        sVelvetParams.enVelvetType  = NG_VELVET_OVN;
        sVelvetParams.fWindowWidth  = 0.1f;

        enCoreGenerator             = NG_CORE_MLS;
        enSparsity                  = NG_SPARSITY_DENSE;
        enColor                     = NG_COLOR_WHITE;

        pData                       = NULL;
        vBuffer                     = NULL;

        bSync                       = true;
    }

    void NoiseGenerator::destroy()
    {
        sMLS.destroy();
        sLCG.destroy();

        free_aligned(pData);
        pData = NULL;

        if (vBuffer != NULL)
        {
            delete [] vBuffer;
            vBuffer = NULL;
        }
    }

    void NoiseGenerator::init_buffers()
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

    void NoiseGenerator::init(uint32_t rand_seed, uint32_t lcg_seed)
    {
        sRandParams.nSeed = rand_seed;
        sRandomizer.init(sRandParams.nSeed);

        sLCGParams.nSeed = lcg_seed;
        sLCG.init(sLCGParams.nSeed);

        init_buffers();
    }

    void NoiseGenerator::init()
    {
        sRandomizer.init();
        sLCG.init();

        init_buffers();
    }

    void NoiseGenerator::update_settings()
    {
        if (sMLSParams.bSync)
        {
            sMLS.set_n_bits(sMLSParams.nBits);
            sMLS.set_state(sMLSParams.nSeed);

            sMLS.update_settings();

            sMLSParams.bSync = false;
        }
        // These can be updated anytime, no need to call update_settings after.
        sMLS.set_amplitude(fAmplitude);
        sMLS.set_offset(fOffset);

        sLCG.set_distribution(sLCGParams.enDistribution);
        sLCG.set_amplitude(fAmplitude);
        sLCG.set_offset(fOffset);

        bSync = true;
    }

    void NoiseGenerator::do_process(float *dst, size_t count)
    {

    }
}
