/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 20 May 2021
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

#include <test/mtest.h>
#include <core/pseudorandom/mls.h>

using namespace lsp;

#define MAX_N_BITS 32

MTEST_BEGIN("core.pseudorandom", mls)

    void write_buffer(const char *filePath, const char *description, const float *buf, size_t count)
    {
        printf("Writing %s to file %s\n", description, filePath);

        FILE *fp = NULL;
        fp = fopen(filePath, "w");

        if (fp == NULL)
            return;

        while (count--)
            fprintf(fp, "%.30f\n", *(buf++));

        if(fp)
            fclose(fp);
    }

    MTEST_MAIN
    {
        uint8_t nBits = 24;
        nBits = lsp_min(nBits, MAX_N_BITS);
        MLS::mls_t nState = 0;  // Use 0 to force default state.

        MLS mls;
        mls.set_n_bits(nBits);
        mls.set_state(nState);
        mls.update_settings();
        MLS::mls_t nPeriod = mls.get_period();

        float *vBuffer = new float[nPeriod];
        for (size_t n = 0; n < nPeriod; ++n)
            vBuffer[n] = mls.single_sample_processor();

        write_buffer("tmp/mls.csv", "MLS Period", vBuffer, nPeriod);

        delete [] vBuffer;

        mls.destroy();
    }

MTEST_END
