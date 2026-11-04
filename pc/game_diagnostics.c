/* Opt-in diagnostics at the SDK's presentation boundary. Input uses the same
 * SDL event path as a keyboard; captures contain the composed pair of screens. */
#include <simulator/sim.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long frame;
static unsigned long limit;
static unsigned long interval;
static const char *capture;
static struct {
    unsigned long frame;
    SDL_Keycode key;
    unsigned long duration;
    int x, y;
    int touch;
} inputs[512];
static size_t inputCount;

static void Fail(const char *message) {
    fprintf(stderr, "game diagnostics: %s\n", message);
    exit(1);
}

static unsigned long Number(const char *text) {
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (!*text || *end || *text == '-') {
        Fail("expected a nonnegative frame number");
    }
    return value;
}

static SDL_Keycode Key(const char *name) {
    const SIM_config_pad_settings_type *pad = &SIM_GetConfigPtr()->padSettings;
    if (!strcmp(name, "A")) return pad->aKey;
    if (!strcmp(name, "B")) return pad->bKey;
    if (!strcmp(name, "X")) return pad->xKey;
    if (!strcmp(name, "Y")) return pad->yKey;
    if (!strcmp(name, "L")) return pad->lKey;
    if (!strcmp(name, "R")) return pad->rKey;
    if (!strcmp(name, "SELECT")) return pad->selectKey;
    if (!strcmp(name, "START")) return pad->startKey;
    if (!strcmp(name, "UP")) return pad->upKey;
    if (!strcmp(name, "DOWN")) return pad->downKey;
    if (!strcmp(name, "LEFT")) return pad->leftKey;
    if (!strcmp(name, "RIGHT")) return pad->rightKey;
    Fail("unknown input button");
    return 0;
}

static void LoadSchedule(const char *value);

static void Init(void) {
    const char *value = getenv("HG_PC_GAME_FRAMES");
    if (value) limit = Number(value);
    value = getenv("HG_PC_GAME_CAPTURE_EVERY");
    interval = value ? Number(value) : 60;
    capture = getenv("HG_PC_GAME_CAPTURE");
    value = getenv("HG_PC_GAME_INPUT");
    LoadSchedule(value);
}

static void LoadSchedule(const char *value) {
    inputCount = 0;
    memset(inputs, 0, sizeof(inputs));
    if (value) {
        char *schedule = strdup(value);
        if (!schedule) Fail("input allocation failed");
        for (char *token = strtok(schedule, ","); token; token = strtok(NULL, ",")) {
            char *colon = strchr(token, ':');
            if (!colon || inputCount == sizeof(inputs) / sizeof(inputs[0])) {
                Fail("input schedule must be frame:button pairs (at most 512)");
            }
            *colon++ = '\0';
            inputs[inputCount].frame = Number(token);
            char *extra = strchr(colon, ':');
            if (extra) *extra++ = '\0';
            inputs[inputCount].duration = 2;
            if (!strcmp(colon, "TOUCH")) {
                if (!extra) Fail("touch requires frame:TOUCH:x:y[:duration]");
                char *y = strchr(extra, ':');
                if (!y) Fail("touch requires x and y");
                *y++ = '\0';
                char *duration = strchr(y, ':');
                if (duration) *duration++ = '\0';
                inputs[inputCount].x = Number(extra);
                inputs[inputCount].y = Number(y);
                inputs[inputCount].touch = 1;
                if (inputs[inputCount].x > 255 || inputs[inputCount].y > 191) Fail("touch is outside DS screen");
                if (duration) inputs[inputCount].duration = Number(duration);
            } else {
                inputs[inputCount].key = Key(colon);
                if (extra) inputs[inputCount].duration = Number(extra);
            }
            if (!inputs[inputCount].frame || !inputs[inputCount].duration) Fail("input frame and duration must be positive");
            inputCount++;
        }
        free(schedule);
    }
}

static void Capture(SDL_Window *window) {
    typedef void (*BindFramebuffer)(GLenum, GLuint);
    typedef void (*ReadBuffer)(GLenum);
    typedef void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
    BindFramebuffer bind = (BindFramebuffer)SDL_GL_GetProcAddress("glBindFramebuffer");
    ReadBuffer readBuffer = (ReadBuffer)SDL_GL_GetProcAddress("glReadBuffer");
    ReadPixels readPixels = (ReadPixels)SDL_GL_GetProcAddress("glReadPixels");
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    if (!bind || !readBuffer || !readPixels || width <= 0 || height <= 0) Fail("capture unavailable");
    unsigned char *pixels = malloc((size_t)width * height * 4);
    char path[4096];
    if (snprintf(path, sizeof(path), "%s-%06lu.ppm", capture, frame) >= sizeof(path)) Fail("capture path too long");
    FILE *output = fopen(path, "wb");
    if (!pixels || !output) Fail("could not create capture");
    bind(GL_FRAMEBUFFER, 0);
    readBuffer(GL_BACK);
    readPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    fprintf(output, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            if (fwrite(pixels + ((size_t)y * width + x) * 4, 1, 3, output) != 3) Fail("capture write failed");
        }
    }
    if (fclose(output)) Fail("capture close failed");
    free(pixels);
    fprintf(stderr, "game diagnostics: frame %lu captured\n", frame);
}

void __real_SDL_GL_SwapWindow(SDL_Window *window);
void __wrap_SDL_GL_SwapWindow(SDL_Window *window) {
    if (!frame) Init();
    frame++;
    const char *inputFile = getenv("HG_PC_GAME_INPUT_FILE");
    if (inputFile && (frame == 1 || frame % 30 == 0)) {
        char schedule[16384];
        FILE *file = fopen(inputFile, "rb");
        if (!file) Fail("could not open input file");
        size_t size = fread(schedule, 1, sizeof(schedule) - 1, file);
        if (ferror(file) || !feof(file)) Fail("input file too large or unreadable");
        fclose(file);
        schedule[size] = '\0';
        // Newlines allow appending events during a running diagnostic.
        for (size_t i = 0; i < size; i++) if (schedule[i] == '\n') schedule[i] = ',';
        LoadSchedule(schedule);
    }
    if (capture && ((interval && frame % interval == 0) || frame == limit)) Capture(window);
    __real_SDL_GL_SwapWindow(window);
    for (size_t i = 0; i < inputCount; i++) {
        if (frame == inputs[i].frame || frame == inputs[i].frame + inputs[i].duration) {
            SDL_Event event = {0};
            int down = frame == inputs[i].frame;
            if (inputs[i].touch) {
                if (SIM_GetConfigPtr()->screenLayout != SIM_CONFIG_SCREEN_LAYOUT_VERTICAL) Fail("touch diagnostics require vertical screen layout");
                int width, height;
                SDL_GetWindowSize(window, &width, &height);
                SDL_WarpMouseInWindow(window, inputs[i].x * width / 256,
                                     height / 2 + inputs[i].y * height / 384);
                event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                event.button.button = SDL_BUTTON_LEFT;
                event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
            } else {
                event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
                event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
                event.key.keysym.sym = inputs[i].key;
            }
            if (SDL_PushEvent(&event) != 1) Fail("input event failed");
            fprintf(stderr, "game diagnostics: frame %lu %s %d %s\n", frame,
                    inputs[i].touch ? "touch" : "key", inputs[i].key, down ? "down" : "up");
        }
    }
    if (limit && frame >= limit) {
        fprintf(stderr, "game diagnostics: completed %lu presented frames\n", frame);
        exit(0); /* Same shutdown path as closing the SDK window. */
    }
}
