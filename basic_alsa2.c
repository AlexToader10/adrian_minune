#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <termios.h>


#define PCM_DEVICE "default"

// WAV header structure
typedef struct WAVHeader {
    char riff_header[4]; // Contains "RIFF"
    int wav_size;        // Size of the WAV file
    char wave_header[4]; // Contains "WAVE"
    char fmt_header[4];  // Contains "fmt " (includes trailing space)
    int fmt_chunk_size;  // Should be 16 for PCM
    short audio_format;  // Should be 1 for PCM
    short num_channels;
    int sample_rate;
    int byte_rate;       // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    short sample_alignment; // num_channels * Bytes Per Sample
    short bit_depth;        // Number of bits per sample
    char data_header[4];    // Contains "data"
    int data_bytes;         // Number of bytes in data. Number of samples * num_channels * sample byte size
} WAVHeader;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wav file>\n", argv[0]);
        return -1;
    }
    printf("Press space to pause/resume playback\n");

    char *wav_filename = argv[1];
    FILE *wav_file = fopen(wav_filename, "rb");
    if (!wav_file) {
        fprintf(stderr, "Can't open file: %s\n", wav_filename);
        return -1;
    }

    WAVHeader wav_header;
    if (fread(&wav_header, sizeof(WAVHeader), 1, wav_file) < 1) {
        fprintf(stderr, "Can't read WAV file header\n");
        fclose(wav_file);
        return -1;
    }


    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int pcm;

    // Open PCM device for playback
    if (snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "ERROR: Can't open \"%s\" PCM device.\n", PCM_DEVICE);
        fclose(wav_file);
        return -1;
    }

    // Allocate parameters object and fill it with default values
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    // Set parameters based on WAV header
    if (snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
        snd_pcm_hw_params_set_format(pcm_handle, params, (wav_header.bit_depth == 16) ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8) < 0 ||
        snd_pcm_hw_params_set_channels(pcm_handle, params, wav_header.num_channels) < 0 ||
        snd_pcm_hw_params_set_rate_near(pcm_handle, params, (unsigned int *)&wav_header.sample_rate, 0) < 0 ||
        snd_pcm_hw_params(pcm_handle, params) < 0) {
        fprintf(stderr, "ERROR: Can't set hardware parameters.\n");
        fclose(wav_file);
        snd_pcm_close(pcm_handle);
        return -1;
    }

    // Prepare PCM for use
    if (snd_pcm_prepare(pcm_handle) < 0) {
        fprintf(stderr, "ERROR: Can't prepare audio interface for use.\n");
        fclose(wav_file);
        snd_pcm_close(pcm_handle);
        return -1;
    }

    // Calculate buffer size
    snd_pcm_hw_params_get_period_size(params, &frames, 0);
    int buffer_size = frames * wav_header.num_channels * (wav_header.bit_depth / 8);
    char *buffer = (char *)malloc(buffer_size);

    // Playback loop

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Get the current terminal settings
    newt = oldt; // Copy the settings to modify them
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply the new settings immediately

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        fclose(wav_file);
        snd_pcm_close(pcm_handle);
        return -1;
    }

    struct epoll_event ev, events[1];
    ev.events = EPOLLIN; // Monitor for input
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
        perror("epoll_ctl: stdin");
        close(epoll_fd);
        fclose(wav_file);
        snd_pcm_close(pcm_handle);
        return -1;
    }

    // Make stdin non-blocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    // Your existing playback loop with modifications for epoll...
    int pause = 0;
    while (1) {
        snd_pcm_pause(pcm_handle, pause);
        if (!pause) {
            // Read data from file
            ssize_t read_bytes = fread(buffer, 1, buffer_size, wav_file);
            if (read_bytes == 0) {
                // End of file
                break;
            }
            if (snd_pcm_writei(pcm_handle, buffer, frames) < 0) {
                snd_pcm_prepare(pcm_handle);
            }
        }

        // Check for input using epoll
        int nfds = epoll_wait(epoll_fd, events, 1, 0); // 0 timeout for non-blocking
        if (nfds > 0) {
            // We have input
            char input;
            int rc = read(STDIN_FILENO, &input, 1); // Read a single character without waiting for Enter
            // Process input
            if (rc < 0) {
                fprintf(stderr, "Error reading from stdin\n");
            }
            if (input == ' ') {
                pause = !pause;
            }
            // Add your input handling code here
            
        }
    }

    // Your existing cleanup code...
    close(epoll_fd); // Close the epoll file descriptor

    // Clean up
    free(buffer);
    fclose(wav_file);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    return 0;
}