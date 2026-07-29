#ifdef _WIN32

int run_tui(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    fprintf(stderr, "sowon: TUI mode is not supported on Windows yet\n");
    return 1;
}

#else

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TUI_FPS 10
#define TUI_DEFAULT_SCALE 2
#define TUI_MIN_SCALE 1
#define TUI_MAX_SCALE 4
#define TUI_GLYPH_WIDTH 3
#define TUI_GLYPH_HEIGHT 5
#define TUI_GLYPH_GAP 1

typedef struct {
    struct termios original_termios;
    int termios_saved;
    int requested_scale;
    int prev_cols;
    int prev_rows;
} Tui;

static volatile sig_atomic_t tui_interrupted = 0;

static const char *tui_glyphs[11][TUI_GLYPH_HEIGHT] = {
    {"###", "# #", "# #", "# #", "###"},
    {"  #", "  #", "  #", "  #", "  #"},
    {"###", "  #", "###", "#  ", "###"},
    {"###", "  #", "###", "  #", "###"},
    {"# #", "# #", "###", "  #", "  #"},
    {"###", "#  ", "###", "  #", "###"},
    {"###", "#  ", "###", "# #", "###"},
    {"###", "  #", "  #", "  #", "  #"},
    {"###", "# #", "###", "# #", "###"},
    {"###", "# #", "###", "  #", "###"},
    {" ",   "#",   " ",   "#",   " "  },
};

static void tui_handle_signal(int signum)
{
    (void) signum;
    tui_interrupted = 1;
}

static uint64_t tui_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t) ts.tv_sec * 1000000000LLU + (uint64_t) ts.tv_nsec;
}

static void tui_sleep_ns(uint64_t ns)
{
    struct timespec time;
    time.tv_sec = (time_t) (ns / 1000000000LLU);
    time.tv_nsec = (long) (ns % 1000000000LLU);

    while (!tui_interrupted && nanosleep(&time, &time) < 0 && errno == EINTR) {
    }
}

static int tui_enter(Tui *tui)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "sowon: TUI mode requires an interactive terminal\n");
        return 0;
    }

    if (tcgetattr(STDIN_FILENO, &tui->original_termios) < 0) {
        perror("tcgetattr");
        return 0;
    }

    struct termios raw = tui->original_termios;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t) ~(IXON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr");
        return 0;
    }

    tui->termios_saved = 1;

    signal(SIGINT, tui_handle_signal);
    signal(SIGTERM, tui_handle_signal);
    signal(SIGHUP, tui_handle_signal);

    printf("\033[?1049h\033[?25l\033[2J\033[H");
    fflush(stdout);

    return 1;
}

static void tui_leave(Tui *tui)
{
    if (tui->termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tui->original_termios);
        tui->termios_saved = 0;
    }

    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);

    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
}

static void tui_get_size(int *cols, int *rows)
{
    struct winsize ws;

    *cols = 80;
    *rows = 24;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) *cols = ws.ws_col;
        if (ws.ws_row > 0) *rows = ws.ws_row;
    }
}

static void tui_restart(State *state, int argc, char **argv)
{
    parse_state_from_args(state, argc, argv);
}

static void tui_handle_key(unsigned char key, Tui *tui, State *state, int argc, char **argv)
{
    switch (key) {
    case 'q':
    case 'Q':
        state->quit = 1;
        break;

    case ' ':
        state->paused = !state->paused;
        break;

    case 'r':
    case 'R':
        tui_restart(state, argc, argv);
        break;

    case '+':
    case '=':
        if (tui->requested_scale < TUI_MAX_SCALE) {
            tui->requested_scale++;
        }
        break;

    case '-':
        if (tui->requested_scale > TUI_MIN_SCALE) {
            tui->requested_scale--;
        }
        break;

    case '0':
        tui->requested_scale = TUI_DEFAULT_SCALE;
        break;
    }
}

static void tui_handle_escape(const char *sequence, State *state, int argc, char **argv)
{
    if (strcmp(sequence, "[15~") == 0) {
        tui_restart(state, argc, argv);
    }
}

static void tui_handle_input(Tui *tui, State *state, int argc, char **argv)
{
    unsigned char input[64];
    ssize_t n;

    while ((n = read(STDIN_FILENO, input, sizeof(input))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            if (input[i] == '\033') {
                char sequence[16];
                size_t sequence_size = 0;

                while (i + 1 < n && sequence_size < sizeof(sequence) - 1) {
                    unsigned char next = input[i + 1];
                    if (next == '\033') {
                        break;
                    }

                    sequence[sequence_size++] = (char) next;
                    i++;

                    if (next == '~') {
                        break;
                    }
                }

                sequence[sequence_size] = '\0';
                tui_handle_escape(sequence, state, argc, argv);
            } else {
                tui_handle_key(input[i], tui, state, argc, argv);
            }
        }
    }
}

static int tui_glyph_index(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    return COLON_INDEX;
}

static int tui_text_width(const char *text, int scale)
{
    int length = (int) strlen(text);
    if (length <= 0) return 0;
    return ((TUI_GLYPH_WIDTH * length) + (TUI_GLYPH_GAP * (length - 1))) * scale;
}

static void tui_print_spaces(int count)
{
    for (int i = 0; i < count; ++i) {
        putchar(' ');
    }
}

static void tui_print_centered(int cols, const char *text)
{
    int length = (int) strlen(text);
    int padding = 0;

    if (cols > length) {
        padding = (cols - length) / 2;
    }

    tui_print_spaces(padding);

    for (int i = 0; i < length && i + padding < cols; ++i) {
        putchar(text[i]);
    }
}

static void tui_print_time_row(const char *text, int glyph_row, int scale)
{
    size_t length = strlen(text);

    for (size_t i = 0; i < length; ++i) {
        int glyph_index = tui_glyph_index(text[i]);
        const char *row = tui_glyphs[glyph_index][glyph_row];

        for (int x = 0; row[x] != '\0'; ++x) {
            char pixel = row[x] == ' ' ? ' ' : '#';
            for (int repeat = 0; repeat < scale; ++repeat) {
                putchar(pixel);
            }
        }

        if (i + 1 < length) {
            tui_print_spaces(TUI_GLYPH_GAP * scale);
        }
    }
}

static void tui_render_compact(const State *state, int cols, int rows, const char *time_text)
{
    char line[128];
    snprintf(line, sizeof(line), "sowon %s %s%s",
             mode_as_cstr(state->mode),
             time_text,
             state->paused ? " paused" : "");

    for (int row = 0; row < rows; ++row) {
        if (row == rows / 2) {
            tui_print_centered(cols, line);
        }
        printf("\033[K");
        if (row + 1 < rows) putchar('\n');
    }
}

static void tui_render(Tui *tui, const State *state)
{
    int cols, rows;
    char time_text[16];

    tui_get_size(&cols, &rows);
    format_displayed_time(time_text, sizeof(time_text), state->displayed_time);

    if (cols != tui->prev_cols || rows != tui->prev_rows) {
        printf("\033[2J");
        tui->prev_cols = cols;
        tui->prev_rows = rows;
    }

    printf("\033[H");

    int status_rows = rows >= 8 ? 2 : 0;
    int scale = tui->requested_scale;
    while (scale > TUI_MIN_SCALE &&
           (tui_text_width(time_text, scale) > cols ||
            TUI_GLYPH_HEIGHT * scale + status_rows > rows)) {
        scale--;
    }

    if (tui_text_width(time_text, scale) > cols ||
        TUI_GLYPH_HEIGHT * scale > rows) {
        tui_render_compact(state, cols, rows, time_text);
        fflush(stdout);
        return;
    }

    int block_width = tui_text_width(time_text, scale);
    int block_height = TUI_GLYPH_HEIGHT * scale;
    int content_rows = rows - status_rows;
    int start_row = (content_rows - block_height) / 2;
    int start_col = (cols - block_width) / 2;

    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;

    printf(state->paused
           ? "\033[38;2;220;120;120m"
           : "\033[38;2;220;220;220m");

    for (int row = 0; row < rows; ++row) {
        if (row >= start_row && row < start_row + block_height) {
            int glyph_row = (row - start_row) / scale;
            tui_print_spaces(start_col);
            tui_print_time_row(time_text, glyph_row, scale);
        } else if (status_rows && row == rows - 2) {
            char status[128];
            snprintf(status, sizeof(status), "%s%s",
                     mode_as_cstr(state->mode),
                     state->paused ? " paused" : "");
            printf("\033[2m");
            tui_print_centered(cols, status);
            printf(state->paused
                   ? "\033[22m\033[38;2;220;120;120m"
                   : "\033[22m\033[38;2;220;220;220m");
        } else if (status_rows && row == rows - 1) {
            printf("\033[2m");
            tui_print_centered(cols, "Space pause | R restart | +/- zoom | 0 reset | Q quit");
            printf(state->paused
                   ? "\033[22m\033[38;2;220;120;120m"
                   : "\033[22m\033[38;2;220;220;220m");
        }

        printf("\033[K");
        if (row + 1 < rows) putchar('\n');
    }

    printf("\033[0m");
    fflush(stdout);
}

int run_tui(int argc, char **argv)
{
    State state = {0};
    Tui tui = {0};
    uint64_t last_time;

    tui.requested_scale = TUI_DEFAULT_SCALE;
    tui.prev_cols = -1;
    tui.prev_rows = -1;

    parse_state_from_args(&state, argc, argv);

    if (!tui_enter(&tui)) {
        return 1;
    }

    last_time = tui_now_ns();
    while (!state.quit && !tui_interrupted) {
        uint64_t frame_start = tui_now_ns();
        float dt = (float) (frame_start - last_time) / 1000000000.0f;
        last_time = frame_start;

        tui_handle_input(&tui, &state, argc, argv);
        tui_render(&tui, &state);
        state_update(&state, dt);

        {
            uint64_t frame_end = tui_now_ns();
            uint64_t frame_ns = 1000000000LLU / TUI_FPS;
            uint64_t frame_time = frame_end - frame_start;
            if (frame_time < frame_ns) {
                tui_sleep_ns(frame_ns - frame_time);
            }
        }
    }

    tui_leave(&tui);

    return tui_interrupted ? 130 : 0;
}

#endif
