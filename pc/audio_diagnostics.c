/* Opt-in capture of the samples actually delivered to SDL's audio device. */
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *capture;
static SDL_AudioCallback originalCallback;
static void *originalUserdata;
static SDL_AudioSpec captureSpec;
static uint32_t capturedBytes;
static int captureFailed;

static void Write32(unsigned char *dst, uint32_t value) {
    for (int i = 0; i < 4; i++) dst[i] = value >> (i * 8);
}

static void FinishCapture(void) {
    unsigned char header[44] = "RIFF\0\0\0\0WAVEfmt \20\0\0\0\1\0";
    Write32(header + 4, capturedBytes + 36);
    header[22] = captureSpec.channels;
    Write32(header + 24, captureSpec.freq);
    Write32(header + 28, captureSpec.freq * captureSpec.channels * 2);
    header[32] = captureSpec.channels * 2;
    header[34] = 16;
    SDL_memcpy(header + 36, "data", 4);
    Write32(header + 40, capturedBytes);
    if (fseek(capture, 0, SEEK_SET) || fwrite(header, 1, sizeof(header), capture) != sizeof(header)) captureFailed = 1;
    if (fclose(capture)) captureFailed = 1;
    fprintf(stderr, "audio diagnostics: %s %u bytes of PCM\n", captureFailed ? "failed to capture" : "captured", capturedBytes);
}

static void CaptureAudio(void *userdata, Uint8 *stream, int length) {
    originalCallback(originalUserdata, stream, length);
    if (capturedBytes > UINT32_MAX - 36u - (uint32_t)length ||
        fwrite(stream, 1, length, capture) != (size_t)length) {
        captureFailed = 1;
        return;
    }
    capturedBytes += length;
}

int __real_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
int __wrap_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
    const char *path = getenv("HG_PC_GAME_AUDIO_CAPTURE");
    if (!path || !*path) return __real_SDL_OpenAudio(desired, obtained);
    if (desired->format != AUDIO_S16LSB || !desired->callback || obtained) {
        fputs("audio diagnostics: expected converted signed 16-bit SDL output\n", stderr);
        exit(1);
    }
    capture = fopen(path, "wb");
    if (!capture || fseek(capture, 44, SEEK_SET)) {
        fputs("audio diagnostics: cannot open WAV output\n", stderr);
        exit(1);
    }
    captureSpec = *desired;
    originalCallback = desired->callback;
    originalUserdata = desired->userdata;
    SDL_AudioSpec hooked = *desired;
    hooked.callback = CaptureAudio;
    hooked.userdata = NULL;
    /* The SDK registers SDL_CloseAudio after opening the device, so audio is
     * stopped and joined before this earlier atexit handler finalizes the WAV. */
    atexit(FinishCapture);
    return __real_SDL_OpenAudio(&hooked, obtained);
}
