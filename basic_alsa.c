#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <math.h>

#define PCM_DEVICE "default"

int main() {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int pcm, dir;
    unsigned int rate = 44250; // Sample rate
    unsigned int channels = 2; // Stereo
    int16_t *buffer;
    int size;
    int loops, period_time;

    // Open PCM device for playback
    if (snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "ERROR: Can't open \"%s\" PCM device.\n", PCM_DEVICE);
        return -1;
    }

    // Allocate parameters object and fill it with default values
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    // Set parameters
    if (snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        fprintf(stderr, "ERROR: Can't set interleaved mode.\n");
        return -1;
    }
    if (snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE) < 0) {
        fprintf(stderr, "ERROR: Can't set format.\n");
        return -1;
    }
    if (snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0) {
        fprintf(stderr, "ERROR: Can't set channels number.\n");
        return -1;
    }
    if (snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir) < 0) {
        fprintf(stderr, "ERROR: Can't set rate.\n");
        return -1;
    }

    // Write parameters
    if (snd_pcm_hw_params(pcm_handle, params) < 0) {
        fprintf(stderr, "ERROR: Can't set harware parameters.\n");
        return -1;
    }

    // Prepare PCM for use
    if (snd_pcm_prepare(pcm_handle) < 0) {
        fprintf(stderr, "ERROR: Can't prepare audio interface for use.\n");
        return -1;
    }

    // Allocate buffer to hold single period
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * channels * 2 /* 2 -> sample size */; 
    buffer = (int16_t *)malloc(size);

    snd_pcm_hw_params_get_period_time(params, &period_time, &dir);

    // Generate and write the signal
    for (loops = (5 * 1000000) / period_time; loops > 0; loops--) {
        for (int i = 0; i < frames * channels; i += 2) {
            buffer[i] = buffer[i + 1] = (int16_t)(sin((double)i / frames * M_PI * 2) * 32767.0);
        }
        if (snd_pcm_writei(pcm_handle, buffer, frames) == -EPIPE) {
            fprintf(stderr, "XRUN.\n");
            snd_pcm_prepare(pcm_handle);
        } else if (pcm < 0) {
            fprintf(stderr, "ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
        }
    }

    // Clean up
    free(buffer);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    return 0;
}