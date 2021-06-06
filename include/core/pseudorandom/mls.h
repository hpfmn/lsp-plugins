/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 13 May 2021
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
#ifndef CORE_PSEUDORANDOM_MLS_H_
#define CORE_PSEUDORANDOM_MLS_H_

#include <core/types.h>
#include <common/types.h>
#include <core/IStateDumper.h>

namespace lsp
{

    class MLS
    {
        /** MLS is a type of pseudorandom sequence with a number of desirable properties:
         *
         * * Smallest crest factor;
         * * 2^N - 1 period length;
         * * MLS sequences are ideally decorrelated from themselves;
         * * MLS spectrum is flat;
         *
         * Where N is the number of bits.
         *
         * MLS is implemented with a linear shift feedback register of N bits. At each step:
         * * The leftmost bit is taken as output.
         * * The values of certain bits in the register are passed through an XOR gate.
         * * The values in the registers are shifted by one to the left.
         * * The XOR gate output is now put into the rightmost bit.
         *
         * If the bits that feed the XOR gate (taps) are chosen appropriately, the resulting sequence will be an MLS of period 2^N -1.
         * Typically, if the output bit is 1 the value of the sequence will be 1. -1 otherwise.
         * The register can be initialised (seeded) with any non-zero value.
         *
         * This class supports MLS generation with registers up to 32 bits on 32 platforms and 64 bits on 64 platforms.
         *
         * Basic MLS Theory at:
         *
         * http://www.kempacoustics.com/thesis/node83.html
         * https://dspguru.com/dsp/tutorials/a-little-mls-tutorial/
         * http://in.ncu.edu.tw/ncume_ee/digilogi/prbs.htm
         */

        public:
            #ifdef ARCH_32_BIT
            typedef uint32_t mls_t;
            #else
            typedef uint64_t mls_t;
            #endif

        private:
            MLS & operator = (const MLS &);

        private:
            mls_t      *vTapsMaskTable;

            uint8_t     nMaxBits;
            uint8_t     nBits;
            uint8_t     nFeedbackBit;
            mls_t       nFeedbackMask;
            mls_t       nActiveMask;
            mls_t       nTapsMask;
            mls_t       nOutputMask;
            mls_t       nState;

            float       fAmplitude;

            bool        bSync;

        public:
            explicit MLS();
            ~MLS();

            void construct();
            void destroy();

        protected:
            void construct_taps_mask_table();
            mls_t xor_gate(mls_t value);
            mls_t progress();

        public:

            /** Check that MLS needs settings update.
             *
             * @return true if MLS needs settings update.
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

            /** Set the number of bits of the generator. This causes reset.
             *
             * @param nbits number of bits
             */
            inline void set_n_bits(uint8_t nbits)
            {
                if (nbits == nBits)
                    return;

                nBits = nbits;
                bSync = true;
            }

            /** Set the state (seed). This causes reset. States must be non-zero. If 0 is passed, all the active bits will be flipped to 1.
             *
             * @param targetstate state to be set.
             */
            inline void set_state(mls_t targetstate)
            {
                if (targetstate == nState)
                    return;

                nState = targetstate;
                bSync = true;
            }

            /** Set the amplitude of the MLS sequence.
             *
             * @param amplitude amplitude value for the sequence.
             */
            inline void set_amplitude(float amplitude)
            {
                if (amplitude == fAmplitude)
                    return;

                fAmplitude = amplitude;
            }

            /** Get the sequence period
             *
             * @return sequence period
             */
            mls_t get_period();


            /** Get a sample from the MLS generator.
             *
             * @return the next sample in the MLS sequence.
             */
            float single_sample_processor();

            /** Output sequence to the destination buffer in additive mode
             *
             * @param dst output wave destination
             * @param src input source, allowed to be NULL
             * @param count number of samples to synthesise
             */
            void process_add(float *dst, const float *src, size_t count);

            /** Output sequence to the destination buffer in multiplicative mode
             *
             * @param dst output wave destination
             * @param src input source, allowed to be NULL
             * @param count number of samples to process
             */
            void process_mul(float *dst, const float *src, size_t count);

            /** Output sequence to a destination buffer overwriting its content
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

#endif /* CORE_PSEUDORANDOM_MLS_H_ */
