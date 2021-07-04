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
#ifndef CORE_PSEUDORANDOM_NOISEGENERATOR_H_
#define CORE_PSEUDORANDOM_NOISEGENERATOR_H_

#include <core/pseudorandom/Randomizer.h>
#include <core/pseudorandom/mls.h>
#include <core/pseudorandom/lcg.h>

namespace lsp
{
    enum ng_core_t
    {
        NG_CORE_MLS,
        NG_CORE_LCG,
        NG_CORE_MAX
    };

    enum ng_sparsity_t
    {
        NG_SPARSITY_DENSE,
        NG_SPARSITY_VELVET,
        NG_SPARSITY_MAX
    };

    /** As in GENERALIZATIONS OF VELVET NOISE AND THEIR USE IN 1-BIT MUSIC by Kurt James Werner
     * Link: https://dafx2019.bcu.ac.uk/papers/DAFx2019_paper_53.pdf
     */

    enum ng_velvet_type_t
    {
        NG_VELVET_OVN,
        NG_VELVET_ARN,
        NG_VELVET_TRN,
        NG_VELVET_COVN,
        NG_VELVET_CARN,
        NG_VELVET_CTRN,
        NG_VELVET_MAX
    };

    enum ng_color_t
    {
        NG_COLOR_WHITE,
        NG_COLOR_PINK,
        NG_COLOR_RED, // AKA Brownian or Brown
        NG_COLOR_MAX
    };

    class NoiseGenerator
    {
        protected:
            typedef struct rand_params_t
            {
                uint32_t            nSeed;
                random_function_t   enFunc;
            }rand_params_t;

            typedef struct mls_params_t
            {
                bool                bSync;
                uint8_t             nBits;
                MLS::mls_t          nSeed;
            } mls_params_t;

            typedef struct lcg_params_t
            {
//                bool                bSync;  // LCG never needs sync.
                uint32_t            nSeed;
                lcg_dist_t          enDistribution;
            } lcg_params_t;

            typedef struct velvet_params_t
            {
                ng_velvet_type_t    enVelvetType;
                float               fWindowWidth;
            } velvet_params_t;

        private:
            Randomizer          sRandomizer;
            MLS                 sMLS;
            LCG                 sLCG;

            rand_params_t       sRandParams;
            mls_params_t        sMLSParams;
            lcg_params_t        sLCGParams;
            velvet_params_t     sVelvetParams;

            ng_core_t           enCoreGenerator;
            ng_sparsity_t       enSparsity;
            ng_color_t          enColor;

            float               fAmplitude;
            float               fOffset;

            uint8_t            *pData;
            float              *vBuffer;

            bool                bSync;

        public:
            explicit NoiseGenerator();
            ~NoiseGenerator();

            void construct();
            void destroy();

        protected:
            void init_buffers();
            void dense_processor(float *dst, size_t count);
            void sparse_processor(float *dst, size_t count);
            void color_processor(float *dst, size_t count);
            void do_process(float *dst, size_t count);

        public:

            /** Initialize random generator.
             *
             * @param rand_seed seed for the Randomizer generator.
             * @param lcg_seed seed for the LCG generator.
             */
            void init(uint32_t rand_seed, uint32_t lcg_seed);

            /** Initialize random generator, take current time as seed
             */
            void init();

            /** Check that NoiseGenerator needs settings update.
             *
             * @return true if NoiseGenerator needs settings update.
             */
            inline bool needs_update() const
            {
                return bSync;
            }

            /** This method should be called if needs_update() returns true.
             * before calling processing methods.
             *
             */
            void update_settings();

            /** Set the number of bits of the MLS sequence generator.
             *
             * @param nbits number of bits.
             */
            inline void set_mls_n_bits(uint8_t nbits)
            {
                if (nbits == sMLSParams.nBits)
                    return;

                sMLSParams.nBits    = nbits;
                sMLSParams.bSync    = true;
                bSync               = true;
            }

            /** Set MLS generator seed.
             *
             * @param seed MLS seed.
             */
            inline void set_mls_seed(MLS::mls_t seed)
            {
                if (seed == sMLSParams.nSeed)
                    return;

                sMLSParams.nSeed    = seed;
                sMLSParams.bSync    = true;
                bSync               = true;
            }

            /** Set LCG distribution.
             *
             * @param dist LCG distribution.
             */
            inline void set_lcg_distribution(lcg_dist_t dist)
            {
                if ((dist < RND_LINEAR) || (dist >= RND_MAX))
                    return;

                if (dist == sLCGParams.enDistribution)
                    return;

                sLCGParams.enDistribution = dist;
                bSync = true;
            }

            /** Set the lcg_dist_telvet noise type. Velvet noise is emitted only if Sparsity is Velvet.
             *
             * @param type velvet type.
             */
            inline void set_velvet_type(ng_velvet_type_t type)
            {
                if ((type < NG_VELVET_OVN) || (type >= NG_VELVET_MAX))
                    return;

                if (type == sVelvetParams.enVelvetType)
                    return;

                sVelvetParams.enVelvetType = type;
            }

            /** Set the Velvet noise window width in samples. Velvet noise is emitted only if Sparsity is Velvet.
             *
             * @param width velvet noise width.
             */
            inline void set_velvet_window_width(float width)
            {
                if (width == sVelvetParams.fWindowWidth)
                    return;

                sVelvetParams.fWindowWidth = width;
            }

            /** Set which core generator to use.
             *
             * @param core core generator specification.
             */
            inline void set_core_generator(ng_core_t core)
            {
                if ((core < NG_CORE_MLS) || (core >= NG_CORE_MAX))
                    return;

                if (core == enCoreGenerator)
                    return;

                enCoreGenerator = core;
            }

            /** Set the noise sparsity.
             *
             * @param sparsity noise sparsity specification.
             */
            inline void set_noise_sparsity(ng_sparsity_t sparsity)
            {
                if ((sparsity < NG_SPARSITY_DENSE) || (sparsity >= NG_SPARSITY_MAX))
                    return;

                if (sparsity == enSparsity)
                    return;

                enSparsity = sparsity;
            }

            /** Set the noise color.
             *
             * @param noise color specification.
             */
            inline void set_noise_color(ng_color_t color)
            {
                if ((color < NG_COLOR_WHITE) || (color >= NG_COLOR_MAX))
                    return;

                if (color == enColor)
                    return;

                enColor = color;
            }

            /** Set the noise amplitude.
             *
             * @param amplitude noise amplitude.
             */
            inline void set_amplitude(float amplitude)
            {
                if (amplitude == fAmplitude)
                    return;

                fAmplitude = amplitude;
                bSync = true;
            }

            /** Set the noise offset.
             *
             * @param offset noise offset.
             */
            inline void set_offset(float offset)
            {
                if (offset == fOffset)
                    return;

                fOffset = offset;
                bSync = true;
            }

            /** Output noise to the destination buffer in additive mode
             *
             * @param dst output wave destination
             * @param src input source, allowed to be NULL
             * @param count number of samples to synthesise
             */
            void process_add(float *dst, const float *src, size_t count);

            /** Output noise to the destination buffer in multiplicative mode
             *
             * @param dst output wave destination
             * @param src input source, allowed to be NULL
             * @param count number of samples to process
             */
            void process_mul(float *dst, const float *src, size_t count);

            /** Output noise to a destination buffer overwriting its content
             *
             * @param dst output wave destination
             * @param src input source, allowed to be NULLL
             * @param count number of samples to process
             */
            void process_overwrite(float *dst, size_t count);

            /**
             * Dump the state
             * @param dumper dumper
             */
            void dump(IStateDumper *v) const;
    };
}

#endif /* CORE_PSEUDORANDOM_NOISEGENERATOR_H_ */
