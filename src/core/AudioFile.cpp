/*
 * AudioFile.cpp
 *
 *  Created on: 28 янв. 2016 г.
 *      Author: sadko
 */

#include <core/types.h>
#include <core/debug.h>
#include <core/dsp.h>
#include <core/AudioFile.h>
#include <core/alloc.h>

#include <sndfile.h>

#define TMP_BUFFER_SIZE         1024
#define RESAMPLING_PERIODS      8

namespace lsp
{
    static size_t gcd_euclid(size_t a, size_t b)
    {
        while (b)
        {
            size_t c = a % b;
            a = b;
            b = c;
        }
        return a;
    }

    static status_t decode_sf_error(SNDFILE *fd)
    {
        switch (sf_error(NULL))
        {
            case SF_ERR_NO_ERROR:
                return STATUS_OK;
            case SF_ERR_UNRECOGNISED_FORMAT:
                return STATUS_BAD_FORMAT;
            case SF_ERR_MALFORMED_FILE:
                return STATUS_CORRUPTED_FILE;
            case SF_ERR_UNSUPPORTED_ENCODING:
                return STATUS_BAD_FORMAT;
            default:
                return STATUS_UNKNOWN_ERR;
        }
    }

    AudioFile::AudioFile()
    {
        pData       = NULL;
    }

    AudioFile::~AudioFile()
    {
        destroy();
    }

    AudioFile::file_content_t *AudioFile::create_file_content(size_t channels, size_t samples)
    {
        // Make number of samples multiple of 0x20 bytes
        size_t buffer_len   = (samples + 0x03) & (~size_t(0x03));
        size_t buffer_size  = (buffer_len * sizeof(float) + 0x1f) & (~size_t(0x1f)); // +3 - for convolution alignment
        // Make header size multiple of 0x20 bytes
        size_t header_size  = (sizeof(file_content_t) + sizeof(float *) * channels + 0x1f) & (~size_t(0x1f));
        // Calculate total size
        size_t total_size   = header_size + buffer_size * channels;

        // Allocate structure
        uint8_t *ptr        = lsp_tmalloc(uint8_t, total_size);
        if (ptr == NULL)
            return NULL;

        // Initialize content
        file_content_t *ct  = reinterpret_cast<file_content_t *>(ptr);
        ct->nChannels       = channels;
        ct->nSamples        = buffer_len;
        ct->nSampleRate     = 0;
        ptr                += header_size;

        for (size_t i=0; i < channels; ++i)
        {
            ct->vChannels[i]    = reinterpret_cast<float *>(ptr);
            dsp::fill_zero(ct->vChannels[i], buffer_len);

            ptr                += buffer_size;
        }

        return ct;
    }

    void AudioFile::destroy_file_content(file_content_t *content)
    {
        if (content == NULL)
            return;
        lsp_free(content);
    }

    AudioFile::temporary_buffer_t *AudioFile::create_temporary_buffer(file_content_t *content)
    {
        // Make number of samples multiple of 0x20 bytes
        size_t buffer_samples   = content->nChannels * TMP_BUFFER_SIZE;
        size_t buffer_size      = ALIGN_SIZE(buffer_samples * sizeof(float), 0x20);
        // Make header size multiple of 0x20 bytes
        size_t header_size      = ALIGN_SIZE(sizeof(temporary_buffer_t) + sizeof(float *) * content->nChannels, 0x20);
        // Calculate total size
        size_t total_size       = header_size + buffer_size;

        // Allocate structure
        uint8_t *ptr            = lsp_tmalloc(uint8_t, total_size);
        if (ptr == NULL)
            return NULL;

        // Initialize structure
        temporary_buffer_t *tb  = reinterpret_cast<temporary_buffer_t *>(ptr);
        ptr                    += header_size;

        tb->nSize               = 0;
        tb->nChannels           = content->nChannels;
        tb->nCapacity           = TMP_BUFFER_SIZE;
        tb->vData               = reinterpret_cast<float *>(ptr);
        for (size_t i=0; i<content->nChannels; ++i)
            tb->vChannels[i]        = content->vChannels[i];

        return tb;
    }

    void AudioFile::flush_temporary_buffer(temporary_buffer_t *tb)
    {
        // Decode frames, buffer->nSize = number of frames
        for (size_t j=0; j<tb->nChannels; ++j)
        {
            const float *src    = &tb->vData[j];
            float *dst          = tb->vChannels[j];

            // Copy frame data
            for (size_t i=0; i<tb->nSize; ++i)
            {
                *dst            = *src;

                // Move pointers
                src             += tb->nChannels;
                dst             ++;
            }

            // Update data pointer
            tb->vChannels[j]    = dst;
        }

        // Clear buffer size
        tb->nSize           = 0;
    }

    size_t AudioFile::fill_temporary_buffer(temporary_buffer_t *tb, size_t max_samples)
    {
        size_t count    = tb->nCapacity - tb->nSize;
        if (count > max_samples)
            count = max_samples;

        // Decode frames, buffer->nSize = number of frames
        for (size_t j=0; j<tb->nChannels; ++j)
        {
            float *src          = tb->vChannels[j];
            float *dst          = &tb->vData[j];

            // Copy frame data
            for (size_t i=0; i < count; ++i)
            {
                *dst            = *src;

                // Move pointers
                src             ++;
                dst             += tb->nChannels;
            }

            // Update data pointer
            tb->vChannels[j]    = src;
        }
        tb->nSize          += count;

        return count;
    }

    void AudioFile::destroy_temporary_buffer(temporary_buffer_t *buffer)
    {
        if (buffer == NULL)
            return;
        lsp_free(buffer);
    }

    status_t AudioFile::create(size_t channels, size_t sample_rate, float duration)
    {
        // Calculate the file length (in samples)
        size_t samples          = sample_rate * duration;

        // Allocate content
        file_content_t *fc      = create_file_content(channels, samples);
        if (fc == NULL)
            return STATUS_NO_MEM;

        // Cleanup content
        fc->nSampleRate         = sample_rate;
        for (size_t i=0; i<channels; ++i)
            dsp::fill_zero(fc->vChannels[i], samples);

        // Destroy previously used content and store new
        if (pData != NULL)
            destroy_file_content(pData);
        pData               = fc;
        return STATUS_OK;
    }

    status_t AudioFile::load(const char *path, float max_duration)
    {
        // Load sound file
        SNDFILE *sf_obj;
        SF_INFO sf_info;

        // Open sound file
        lsp_trace("loading file: %s\n", path);
        if ((sf_obj = sf_open(path, SFM_READ, &sf_info)) == NULL)
            return decode_sf_error(sf_obj);

        // Read sample file
        ssize_t max_samples     = (max_duration >= 0.0f) ? seconds_to_samples(sf_info.samplerate, max_duration) : -1;
        lsp_trace("file parameters: frames=%d, channels=%d, sample_rate=%d max_duration=%.3f\n, max_samples=%d",
            int(sf_info.frames), int(sf_info.channels), int(sf_info.samplerate), max_duration, int(max_samples));

        // Patch sf_info
        if ((max_samples >= 0) && (sf_info.frames > sf_count_t(max_samples)))
            sf_info.frames  = max_samples;

        // Create file content
        file_content_t *fc      = create_file_content(sf_info.channels, sf_info.frames);
        if (fc == NULL)
        {
            sf_close(sf_obj);
            return STATUS_NO_MEM;
        }
        fc->nSampleRate         = sf_info.samplerate;

        // Allocate temporary buffer
        temporary_buffer_t *tb  = create_temporary_buffer(fc);
        if (tb == NULL)
        {
            destroy_file_content(fc);
            sf_close(sf_obj);
            return STATUS_NO_MEM;
        }

        size_t count = sf_info.frames;
        while (count > 0)
        {
            // Determine how many data is available to read
            size_t can_read     = tb->nCapacity - tb->nSize;
            if (can_read <= 0)
            {
                flush_temporary_buffer(tb);
                can_read            = tb->nCapacity - tb->nSize;
            }

            // Calculate amount of samples to read
            size_t to_read      = (count > can_read) ? can_read : count;
            sf_count_t amount   = sf_readf_float(sf_obj, &tb->vData[tb->nSize], to_read);
            if (amount <= 0)
            {
                status_t status     = decode_sf_error(sf_obj);

                destroy_temporary_buffer(tb);
                destroy_file_content(fc);
                sf_close(sf_obj);

                return status;
            }

            // Update counters
            tb->nSize          += amount;
            count              -= amount;
        }

        // Flush last read data (if present)
        flush_temporary_buffer(tb);

        // Free allocated resources
        destroy_temporary_buffer(tb);
        sf_close(sf_obj);

        // Destroy previously used content and store new
        if (pData != NULL)
            destroy_file_content(pData);
        pData               = fc;

        return STATUS_OK;
    }

    status_t AudioFile::store(const char *path, float max_duration)
    {
        if (pData == NULL)
            return STATUS_NO_DATA;

        // Load sound file
        SNDFILE *sf_obj;
        SF_INFO sf_info;

        size_t count        = (max_duration < 0) ? pData->nSamples : max_duration * pData->nSampleRate;

        sf_info.frames      = count;
        sf_info.samplerate  = pData->nSampleRate;
        sf_info.channels    = pData->nChannels;
        sf_info.format      = SF_FORMAT_WAV | SF_FORMAT_FLOAT | SF_ENDIAN_LITTLE;
        sf_info.sections    = 0;
        sf_info.seekable    = 0;

        if (sf_info.frames > sf_count_t(pData->nSamples))
            sf_info.frames      = pData->nSamples;

        // Open sound file
        lsp_trace("storing file: %s\n", path);
        if ((sf_obj = sf_open(path, SFM_WRITE, &sf_info)) == NULL)
        {
            lsp_trace("Error: %s", sf_strerror(sf_obj));
            return decode_sf_error(NULL);
        }

        // Allocate temporary buffer
        temporary_buffer_t *tb  = create_temporary_buffer(pData);
        if (tb == NULL)
            return STATUS_NO_MEM;

        while ((count > 0) || (tb->nSize > 0))
        {
            // Fill buffer
            count   -=  fill_temporary_buffer(tb, count);

            // Flush buffer
            if (tb->nSize > 0)
            {
                // Write buffer to file
                size_t offset = 0;
                while (offset < tb->nSize)
                {
                    sf_count_t written  = sf_writef_float(sf_obj, tb->vData, tb->nSize - offset);
                    if (written < 0)
                    {
                        status_t status     = decode_sf_error(sf_obj);
                        sf_close(sf_obj);
                        destroy_temporary_buffer(tb);
                        return status;
                    }
                    offset +=  written;
                }

                // Clear buffer size
                tb->nSize   = 0;
            }
        }

        // Free allocated resources
        sf_close(sf_obj);
        destroy_temporary_buffer(tb);

        return STATUS_OK;
    }

    bool AudioFile::reverse(ssize_t track_id)
    {
        if (pData == NULL)
            return false;

        if (track_id >= 0)
        {
            if (size_t(track_id) >= pData->nChannels)
                return false;
            lsp_trace("reverse %p, %d", pData->vChannels[track_id], int(pData->nSamples));
            dsp::reverse1(pData->vChannels[track_id], pData->nSamples);
        }
        else
        {
            size_t count = pData->nChannels;
            if (count <= 0)
                return false;
            for (size_t i=0; i<count; ++i)
            {
                lsp_trace("reverse %p, %d", pData->vChannels[i], int(pData->nSamples));
                dsp::reverse1(pData->vChannels[i], pData->nSamples);
            }
        }

        return true;
    }

    size_t AudioFile::channels() const
    {
        return (pData != NULL) ? pData->nChannels : 0;
    }

    size_t AudioFile::samples() const
    {
        return (pData != NULL) ? pData->nSamples : 0;
    }

    size_t AudioFile::sample_rate() const
    {
        return (pData != NULL) ? pData->nSampleRate : 0;
    }

    status_t AudioFile::resample(size_t new_sample_rate)
    {
        // Check that resampling is actually needed
        if (new_sample_rate > pData->nSampleRate)
        {
            // Need to up-sample data
            if ((new_sample_rate % pData->nSampleRate) == 0)
                return fast_upsample(new_sample_rate);
            else
                return complex_upsample(new_sample_rate);
        }
        else if (new_sample_rate < pData->nSampleRate)
        {
            // Need to down-sample data
            if ((pData->nSampleRate % new_sample_rate) == 0)
                return fast_downsample(new_sample_rate);
            else
                return complex_downsample(new_sample_rate);
        }

        // Return OK status
        return STATUS_OK;
    }

    status_t AudioFile::fast_downsample(size_t new_sample_rate)
    {
        size_t rkf          = pData->nSampleRate / new_sample_rate;
        size_t new_samples  = pData->nSamples / rkf;

        // Prepare new data structure to store resampled data
        file_content_t *fc  = create_file_content(pData->nChannels, new_samples);
        if (fc == NULL)
            return STATUS_NO_MEM;
        fc->nSampleRate     = new_sample_rate;

        // Iterate each channel
        for (size_t c=0; c<fc->nChannels; ++c)
        {
            const float *src    = pData->vChannels[c];
            float *dst          = fc->vChannels[c];

            for (size_t i=0, p=0; i < pData->nSamples; i += rkf, p++)
                dst[p]              = src[i];
        }

        // Destroy old data content
        destroy_file_content(pData);

        // Store new file content
        pData       = fc;

        return STATUS_OK;
    }

    status_t AudioFile::fast_upsample(size_t new_sample_rate)
    {
        // Calculate parameters of transformation
        ssize_t kf          = new_sample_rate / pData->nSampleRate;
        float rkf           = 1.0f / kf;

        // Prepare kernel for resampling
        ssize_t k_periods   = RESAMPLING_PERIODS; // * (kf >> 1);
        ssize_t k_base      = k_periods * kf;
        ssize_t k_center    = k_base + 1;
        ssize_t k_len       = (k_center << 1) + 1;
        ssize_t k_size      = ALIGN_SIZE(k_len + 1, 4); // Additional sample for time offset
        float *k            = lsp_tmalloc(float, k_size);
        if (k == NULL)
            return STATUS_NO_MEM;

        // Prepare temporary buffer for resampling
        size_t new_samples  = kf * pData->nSamples;
        size_t b_len        = new_samples + k_size;
        size_t b_size       = ALIGN_SIZE(b_len, 4);
        float *b            = lsp_tmalloc(float, b_size);
        if (b == NULL)
        {
            lsp_free(k);
            return STATUS_NO_MEM;
        }

        // Prepare new data structure to store resampled data
        file_content_t *fc  = create_file_content(pData->nChannels, new_samples);
        if (fc == NULL)
        {
            lsp_free(b);
            lsp_free(k);
            return STATUS_NO_MEM;
        }
        fc->nSampleRate     = new_sample_rate;

        // Generate Lanczos kernel
        for (ssize_t j=0; j<k_size; ++j)
        {
            float t         = (j - k_center) * rkf;

            if ((t > -k_periods) && (t < k_periods))
            {
                if (t != 0)
                {
                    float t2    = M_PI * t;
                    k[j]        = k_periods * sinf(t2) * sinf(t2 / k_periods) / (t2 * t2);
                }
                else
                    k[j]        = 1.0f;
            }
            else
                k[j]        = 0.0f;
        }

        // Output dump
//        printf("----------------------\n");
//        printf("j;t;k(j);\n");
//        for (ssize_t j=0; j<k_len; ++j)
//        {
//            float t         = (j - k_center) * rkf;
//            printf("%d;%f;%f;\n", int(j), t, k[j]);
//        }
//        printf("----------------------\n");

        // Iterate each channel
        for (size_t c=0; c<fc->nChannels; ++c)
        {
            const float *src    = pData->vChannels[c];
            dsp::fill_zero(b, b_size);  // Clear the temporary buffer

            // Perform convolutions
            for (size_t i=0, p=0; i<pData->nSamples; i++, p += kf)
                dsp::scale_add3(&b[p], k, src[i], k_size);

            // Copy the data to the file content
            dsp::copy(fc->vChannels[c], &b[k_center], fc->nSamples);
        }

        // Delete  temporary buffers
        destroy_file_content(pData);
        lsp_free(b);
        lsp_free(k);

        // Store new file content
        pData       = fc;

        return STATUS_OK;
    }

    status_t AudioFile::complex_upsample(size_t new_sample_rate)
    {
        // Calculate parameters of transformation
        ssize_t gcd         = gcd_euclid(new_sample_rate, pData->nSampleRate);
        ssize_t src_step    = pData->nSampleRate / gcd;
        ssize_t dst_step    = new_sample_rate / gcd;
        float kf            = float(dst_step) / float(src_step);
        float rkf           = float(src_step) / float(dst_step);

        // Prepare kernel for resampling
        ssize_t k_periods   = RESAMPLING_PERIODS; // Number of periods
        ssize_t k_base      = k_periods * kf;
        ssize_t k_center    = k_base + 1;
        ssize_t k_len       = (k_center << 1) + 1; // Centered impulse response
        ssize_t k_size      = ALIGN_SIZE(k_len + 1, 4); // Additional sample for time offset
        float *k            = lsp_tmalloc(float, k_size);
        if (k == NULL)
            return STATUS_NO_MEM;

        // Prepare temporary buffer for resampling
        size_t new_samples  = kf * pData->nSamples;
        size_t b_len        = new_samples + k_size;
        size_t b_size       = ALIGN_SIZE(b_len, 4);
        float *b            = lsp_tmalloc(float, b_size);
        if (b == NULL)
        {
            lsp_free(k);
            return STATUS_NO_MEM;
        }

        // Prepare new data structure to store resampled data
        file_content_t *fc  = create_file_content(pData->nChannels, new_samples);
        if (fc == NULL)
        {
            lsp_free(b);
            lsp_free(k);
            return STATUS_NO_MEM;
        }
        fc->nSampleRate     = new_sample_rate;

        // Iterate each channel
        for (size_t c=0; c<fc->nChannels; ++c)
        {
            const float *src    = pData->vChannels[c];
            dsp::fill_zero(b, b_size);  // Clear the temporary buffer

            for (ssize_t i=0; i<src_step; ++i)
            {
                // calculate the offset between nearest samples
                ssize_t p       = kf * i;
                float dt        = i*kf - p;

                // Generate Lanczos kernel
                for (ssize_t j=0; j<k_size; ++j)
                {
                    float t         = (j - k_center - dt) * rkf;

                    if ((t > -k_periods) && (t < k_periods))
                    {
                        if (t != 0.0f)
                        {
                            float t2    = M_PI * t;
                            k[j]        = k_periods * sinf(t2) * sinf(t2 / k_periods) / (t2 * t2);
                        }
                        else
                            k[j]        = 1.0f;
                    }
                    else
                        k[j]        = 0.0f;
                }

                // Output dump
//                printf("----------------------\n");
//                printf("j;t;k(j);\n");
//                for (ssize_t j=0; j<k_len; ++j)
//                {
//                    float t         = (j - k_center - dt) * kt;
//                    printf("%d;%f;%f;\n", int(j), t, k[j]);
//                }
//                printf("----------------------\n");

                // Perform convolutions
                for (size_t j=i; j<pData->nSamples; j += src_step)
                {
                    dsp::scale_add3(&b[p], k, src[j], k_size);
                    p   += dst_step;
                }
            }

            // Copy the data to the file content
            dsp::copy(fc->vChannels[c], &b[k_center], fc->nSamples);
        }

        // Delete  temporary buffers
        destroy_file_content(pData);
        lsp_free(b);
        lsp_free(k);

        // Store new file content
        pData       = fc;

        return STATUS_OK;
    }

    status_t AudioFile::complex_downsample(size_t new_sample_rate)
    {
        // Calculate parameters of transformation
        ssize_t gcd         = gcd_euclid(new_sample_rate, pData->nSampleRate);
        ssize_t src_step    = pData->nSampleRate / gcd;
        ssize_t dst_step    = new_sample_rate / gcd;
        float kf            = float(dst_step) / float(src_step);
        float rkf           = float(src_step) / float(dst_step);

        // Prepare kernel for resampling
        ssize_t k_base      = RESAMPLING_PERIODS;
        ssize_t k_periods   = k_base * rkf; // Number of periods
        ssize_t k_center    = k_base + 1;
        ssize_t k_len       = (k_center << 1) + rkf + 1; // Centered impulse response
        ssize_t k_size      = ALIGN_SIZE(k_len + 1, 4); // Additional sample for time offset
        float *k            = lsp_tmalloc(float, k_size);
        if (k == NULL)
            return STATUS_NO_MEM;

        // Prepare temporary buffer for resampling
        size_t new_samples  = kf * pData->nSamples;
        size_t b_len        = new_samples + k_size;
        size_t b_size       = ALIGN_SIZE(b_len, 4);
        float *b            = lsp_tmalloc(float, b_size);
        if (b == NULL)
        {
            lsp_free(k);
            return STATUS_NO_MEM;
        }

        // Prepare new data structure to store resampled data
        file_content_t *fc  = create_file_content(pData->nChannels, new_samples);
        if (fc == NULL)
        {
            lsp_free(b);
            lsp_free(k);
            return STATUS_NO_MEM;
        }
        fc->nSampleRate     = new_sample_rate;

        // Iterate each channel
        for (size_t c=0; c<fc->nChannels; ++c)
        {
            const float *src    = pData->vChannels[c];
            dsp::fill_zero(b, b_size);  // Clear the temporary buffer

            for (ssize_t i=0; i<src_step; ++i)
            {
                // calculate the offset between nearest samples
                ssize_t p       = kf * i;
                float dt        = i*kf - p; // Always positive, in range of [0..1]

                // Generate Lanczos kernel
                for (ssize_t j=0; j<k_size; ++j)
                {
                    float t         = (j - k_center - dt) * rkf;

                    if ((t > -k_periods) && (t < k_periods))
                    {
                        if (t != 0.0f)
                        {
                            float t2    = M_PI * t;
                            k[j]        = k_periods * sinf(t2) * sinf(t2 / k_periods) / (t2 * t2);
                        }
                        else
                            k[j]        = 1.0f;
                    }
                    else
                        k[j]        = 0.0f;
                }

                // Output dump
//                printf("----------------------\n");
//                printf("j;t;k(j);\n");
//                for (ssize_t j=0; j<k_size; ++j)
//                {
//                    float t         = (j - k_center - dt) * kt;
//                    printf("%d;%f;%f;\n", int(j), t, k[j]);
//                }
//                printf("----------------------\n");

                // Perform convolutions
                for (size_t j=i; j<pData->nSamples; j += src_step)
                {
                    dsp::scale_add3(&b[p], k, src[j], k_size);
                    p   += dst_step;
                }
            }

            // Copy the data to the file content
            dsp::copy(fc->vChannels[c], &b[k_center], fc->nSamples);
        }

        // Delete  temporary buffers
        destroy_file_content(pData);
        lsp_free(b);
        lsp_free(k);

        // Store new file content
        pData       = fc;

        return STATUS_OK;
    }

    float *AudioFile::channel(size_t track)
    {
        if (pData == NULL)
            return NULL;
        if (track >= pData->nChannels)
            return NULL;
        return pData->vChannels[track];
    }

    void AudioFile::destroy()
    {
        if (pData != NULL)
        {
            destroy_file_content(pData);
            pData       = NULL;
        }
    }

    LoadAudioFileTask::LoadAudioFileTask()
    {
        sPath[0]        = '\0';
        pAF             = NULL;
        bTaken          = false;
    }

    LoadAudioFileTask::~LoadAudioFileTask()
    {
        sPath[0]        = '\0';
        pAF             = NULL;
        bTaken          = false;
    }

    void LoadAudioFileTask::configure(const char *filename)
    {
        strncpy(sPath, filename, PATH_MAX);
        sPath[PATH_MAX-1] ='\0';
    }

    AudioFile *LoadAudioFileTask::file()
    {
        if (!completed())
            return NULL;

        bTaken      = true;
        return pAF;
    }

    int LoadAudioFileTask::run()
    {
        lsp_trace("path=%s, af=%p, taken=%s", sPath, pAF, (bTaken) ? "true" : "false");

        // Drop previous data
        if (pAF != NULL)
        {
            if (!bTaken)
            {
                pAF->destroy();
                delete pAF;
            }
            pAF     = NULL;
        }

        // Load new file
        AudioFile *af = new AudioFile();
        if (!af->load(sPath))
        {
            af->destroy();
            delete af;
            return -1;
        }

        // Store file
        pAF     = af;
        return 0;
    }

    void LoadAudioFileTask::destroy()
    {
        // Drop stored data
        if (pAF != NULL)
        {
            if (!bTaken)
            {
                pAF->destroy();
                delete pAF;
            }
            pAF     = NULL;
        }
    }
} /* namespace lsp */
