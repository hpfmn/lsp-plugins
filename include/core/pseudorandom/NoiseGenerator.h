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
            typedef struct mls_params_t
            {
                uint8_t             nBits;
                MLS::mls_t          nSeed;
            } mls_params_t;

            typedef struct lcg_params_t
            {
                uint32_t            nSeed;
                random_function_t   enFunction;
            } lcg_params_t;

            typedef struct velvet_params_t
            {
                ng_velvet_type_t    enVelvetType;
                float               fWindowWidth;
            } velvet_params_t;

        private:
            MLS                 sMLS;
            LCG                 sLCG;

            mls_params_t        sMLSParams;
            lcg_params_t        sLCGParams;
            velvet_params_t     sVelvetParams;

            ng_core_t           enCoreGenerator;
            ng_sparsity_t       enSparsity;
            ng_color_t          enColor;

            bool                bSync;

        public:
            explicit NoiseGenerator();
            ~NoiseGenerator();

            void construct();
            void destroy();

        protected:
            void do_process(float *dst, size_t count);

        public:

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

            inline void set_mls_n_bits(uint8_t nbits)
            {
                if (nbits == sMLSParams.nBits)
                    return;

                sMLSParams.nBits = nbits;
                bSync = true;
            }

            inline void set_mls_seed(MLS::mls_t seed)
            {
                if (seed == sMLSParams.nSeed)
                    return;

                sMLSParams.nSeed = seed;
                bSync = true;
            }

            inline void set_lcg_seed(uint32_t seed)
            {
                if (seed == sLCGParams.nSeed)
                    return;

                sLCGParams.nSeed = seed;
                bSync = true;
            }

            inline void set_lcg_function(random_function_t func)
            {
                if ((func < RND_LINEAR) || (func >= RND_MAX))
                    return;

                if (func == sLCGParams.enFunction)
                    return;

                sLCGParams.enFunction = func;
            }

            inline void set_velvet_type(ng_velvet_type_t type)
            {
                if ((type < NG_VELVET_OVN) || (type >= NG_VELVET_MAX))
                    return;

                if (type == sVelvetParams.enVelvetType)
                    return;

                sVelvetParams.enVelvetType = type;
            }

            inline void set_velvet_window_width(float width)
            {
                if (width == sVelvetParams.fWindowWidth)
                    return;

                sVelvetParams.fWindowWidth = width;
            }

            inline void set_core_generator(ng_core_t core)
            {
                if ((core < NG_CORE_MLS) || (core >= NG_CORE_MAX))
                    return;

                if (core == enCoreGenerator)
                    return;

                enCoreGenerator = core;
            }

            inline void set_noise_sparsity(ng_sparsity_t sparsity)
            {
                if ((sparsity < NG_SPARSITY_DENSE) || (sparsity >= NG_SPARSITY_MAX))
                    return;

                if (sparsity == enSparsity)
                    return;

                enSparsity = sparsity;
            }

            inline void set_noise_color(ng_color_t color)
            {
                if ((color < NG_COLOR_WHITE) || (color >= NG_COLOR_MAX))
                    return;

                if (color == enColor)
                    return;

                enColor = color;
            }

            /** Output wave to the destination buffer in additive mode
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
