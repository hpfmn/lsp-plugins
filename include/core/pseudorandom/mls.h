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
#include <core/IStateDumper.h>

namespace lsp
{

    class MLS
    {
        public:
            typedef uint64_t mls_t;

        private:
            MLS & operator = (const MLS &);

        private:
            uint8_t     nMaxBits;
            uint8_t     nBits;
            uint8_t     nFeedbackBit;
            mls_t       nFeedbackMask;
            mls_t       nActiveMask;
            mls_t       nTapsMask;
            mls_t       nOutputMask;
            mls_t       nState;

            bool        bSync;

        public:
            explicit MLS();
            ~MLS();

            void construct();
            void destroy();

        protected:
            void assign_taps_mask();
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
                nBits = nbits;
                bSync = true;
            }

            /** Set the state (seed). This causes reset. States must be non-zero. If 0 is passed, all the active bits will be flipped to 1.
             *
             * @param targetstate state to be set.
             */
            inline void set_state(mls_t targetstate)
            {
                nState = targetstate;
                bSync = true;
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

            /**
             * Dump the state
             * @param dumper dumper
             */
            void dump(IStateDumper *v) const;
    };

}

#endif /* CORE_PSEUDORANDOM_MLS_H_ */
