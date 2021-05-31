/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 16 May 2021
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

#include <core/pseudorandom/mls.h>

#define MAX_SUPPORTED_BITS 64

/** Basic MLS Theory at:
 *
 * http://www.kempacoustics.com/thesis/node83.html
 * https://dspguru.com/dsp/tutorials/a-little-mls-tutorial/
 * http://in.ncu.edu.tw/ncume_ee/digilogi/prbs.htm
 */

namespace lsp
{
    MLS::MLS()
    {
        construct();
    }

    MLS::~MLS()
    {
        destroy();
    }

    void MLS::construct()
    {
        nMaxBits        = sizeof(mls_t) * 8;
        nBits           = sizeof(mls_t) * 8;
        nFeedbackBit    = 0;
        nFeedbackMask   = 0;
        nActiveMask     = 0;
        nTapsMask       = 0;
        nOutputMask     = 1;
        nState          = 0;

        bSync           = true;
    }

    void MLS::destroy()
    {
    }

    void MLS::assign_taps_mask()
    {
        /** From the table "Exponent of Terms of Primitive Binary Polynomials"
         *  in Primitive Binary Polynomials by Wayne Stahnke,
         *  Mathematics of Computation, Volume 27, Number 124, October 1973
         *  link: https://www.ams.org/journals/mcom/1973-27-124/S0025-5718-1973-0327722-7/S0025-5718-1973-0327722-7.pdf
         */

        mls_t shift_me = 1;

        switch (nBits)
        {
            case 1:
                return;
            case 2:
            case 3:
            case 4:
            case 6:
            case 7:
            case 15:
            case 22:
            case 60:
            case 63:
                nTapsMask = shift_me | (shift_me << 1);
                return;
            case 5:
            case 11:
            case 21:
            case 29:
            case 35:
                nTapsMask = shift_me | (shift_me << 2);
                return;
            case 8:
            case 19:
            case 38:
            case 43:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 5) | (shift_me << 6);
                return;
            case 9:
            case 39:
                nTapsMask = shift_me | (shift_me << 4);
                return;
            case 10:
            case 17:
            case 20:
            case 25:
            case 28:
            case 31:
            case 41:
            case 52:
                nTapsMask = shift_me | (shift_me << 3);
                return;
            case 12:
                nTapsMask = shift_me | (shift_me << 3) | (shift_me << 4) | (shift_me << 7);
                return;
            case 13:
            case 24:
            case 45:
            case 64:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 3) | (shift_me << 4);
                return;
            case 14:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 11) | (shift_me << 12);
                return;
            case 16:
                nTapsMask = shift_me | (shift_me << 2) | (shift_me << 3) | (shift_me << 5);
                return;
            case 18:
            case 57:
                nTapsMask = shift_me | (shift_me << 7);
                return;
            case 23:
            case 47:
                nTapsMask = shift_me | (shift_me << 5);
                return;
            case 26:
            case 27:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 7) | (shift_me << 8);
                return;
            case 30:
            case 51:
            case 53:
            case 61:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 15) | (shift_me << 16);
                return;
            case 32:
            case 48:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 27) | (shift_me << 28);
                return;
            case 33:
                nTapsMask = shift_me | (shift_me << 13);
                return;
            case 34:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 14) | (shift_me << 15);
                return;
            case 36:
                nTapsMask = shift_me | (shift_me << 11);
                return;
            case 37:
                nTapsMask = shift_me | (shift_me << 2) | (shift_me << 10) | (shift_me << 12);
                return;
            case 40:
                nTapsMask = shift_me | (shift_me << 2) | (shift_me << 19) | (shift_me << 21);
                return;
            case 42:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 22) | (shift_me << 23);
                return;
            case 44:
            case 50:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 26) | (shift_me << 27);
                return;
            case 46:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 20) | (shift_me << 21);
                return;
            case 49:
                nTapsMask = shift_me | (shift_me << 9);
                return;
            case 54:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 36) | (shift_me << 37);
                return;
            case 55:
                nTapsMask = shift_me | (shift_me << 24);
                return;
            case 56:
            case 59:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 21) | (shift_me << 22);
                return;
            case 58:
                nTapsMask = shift_me | (shift_me << 19);
                return;
            case 62:
                nTapsMask = shift_me | (shift_me << 1) | (shift_me << 56) | (shift_me << 57);
                return;
        }
    }

    void MLS::update_settings()
    {
        nMaxBits = lsp_min(nMaxBits, MAX_SUPPORTED_BITS);

        nBits = lsp_max(nBits, 1);
        nBits = lsp_min(nBits, nMaxBits);

        nFeedbackBit = nBits - 1;
        nFeedbackMask = mls_t(1) << nFeedbackBit;

        // Switch on all the first nBits bits.
        nActiveMask = ~(~mls_t(0) << nBits);

        assign_taps_mask();

        nState &= nActiveMask;

        // State cannot be 0. If that happens, flip all the active bits to 1.
        if (nState == 0)
                nState |= nActiveMask;

        bSync = false;
    }

    // Compute the xor of all the bits in value
    MLS::mls_t MLS::xor_gate(mls_t value)
    {
        mls_t nXorValue;
        for (nXorValue = 0; value; nXorValue ^= 1)
        {
            value &= value - 1;
        }

        return nXorValue;
    }

    MLS::mls_t MLS::get_period()
    {
        if (nBits == nMaxBits)
        {
            return -1;
        }
        else
        {
            mls_t period = 1;
            return (period << nBits) - 1;
        }

    }

    MLS::mls_t MLS::progress()
    {
        mls_t nOutput = nState & nOutputMask;

        mls_t nFeedbackValue = xor_gate(nState & nTapsMask);

        nState = nState >> 1;
        nState = (nState & ~nFeedbackMask) | (nFeedbackValue << nFeedbackBit);

        return nOutput;
    }

    float MLS::single_sample_processor()
    {
        if (progress())
            return 1.0f;
        else
            return -1.0f;
    }

    void MLS::dump(IStateDumper *v) const
    {
        v->write("nMaxBits", nMaxBits);
        v->write("nBits", nBits);
        v->write("nFeedbackBit", nFeedbackBit);
        v->write("nFeedbackMask", nFeedbackMask);
        v->write("nActiveMask", nActiveMask);
        v->write("nTapsMask", nTapsMask);
        v->write("nOutputMask", nOutputMask);
        v->write("nState", nState);
        v->write("bSync", bSync);
    }
}
