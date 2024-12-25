#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

// Disable canonical mode and echo
void disable_raw_mode(struct termios *orig_termios) {
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Restore terminal mode
void enable_raw_mode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

// Measure the display width of a wide character
int get_wchar_display_width(wchar_t wc) {
    return wcwidth(wc); // Returns 0 for zero-width, -1 for invalid, or the width of the character
}

// Interactive navigation and selection function
wchar_t *interactive_select(const wchar_t *input) {
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);

    // Enable raw mode for real-time input
    disable_raw_mode(&orig_termios);

    int cursor_pos = 0;
    int start_select = -1; // -1 indicates no selection
    int len = wcslen(input);

    wchar_t *selection = NULL;

    while (1) {
        // Clear screen and reset cursor position
        printf("\033[H\033[J");

        // Render the wide string
        for (int i = 0; i < len; i++) {
            if (start_select != -1 && ((start_select <= i && i <= cursor_pos) || 
                                       (cursor_pos <= i && i <= start_select))) {
                // Highlight selection
                printf("\033[42m%lc\033[0m", input[i]); // Green background for selection
            } else if (i == cursor_pos) {
                // Highlight cursor position
                printf("\033[7m%lc\033[0m", input[i]); // Reverse video for cursor
            } else {
                printf("%lc", input[i]);
            }
        }

        // Display instructions
        printf("\nUse arrow keys to move (Left/Right), 'v' to toggle selection, Enter to confirm, 'q' to quit.");

        // Read key press
        char c = getchar();
        if (c == '\033') { // Handle arrow keys
            getchar(); // Skip '['
            switch (getchar()) {
                case 'C': // Right arrow
                    if (cursor_pos < len - 1) cursor_pos++;
                    break;
                case 'D': // Left arrow
                    if (cursor_pos > 0) cursor_pos--;
                    break;
            }
        } else if (c == 'v') { // Toggle selection mode
            if (start_select == -1) {
                start_select = cursor_pos; // Start selection
            } else {
                start_select = -1; // Exit selection mode
            }
        } else if (c == '\n') { // Confirm selection
            if (start_select != -1) {
                // Calculate selection bounds
                int start = start_select < cursor_pos ? start_select : cursor_pos;
                int end = start_select > cursor_pos ? start_select : cursor_pos;

                // Allocate and copy selection
                size_t selection_len = end - start + 1;
                selection = malloc((selection_len + 1) * sizeof(wchar_t));
                if (!selection) {
                    fprintf(stderr, "Memory allocation error for selection\n");
                    break;
                }
                wcsncpy(selection, &input[start], selection_len);
                selection[selection_len] = L'\0'; // Null-terminate
                break;
            }
        } else if (c == 'q') { // Exit
            break;
        }
    }

    // Restore terminal settings
    enable_raw_mode(&orig_termios);

    return selection;
}

int main() {
    // Set locale for proper wide character handling
    setlocale(LC_ALL, "");

    // Example Chinese text
    const wchar_t *sample_text = L"导航这个文本，使用箭头键。";
    wchar_t *selected_text = interactive_select(sample_text);

    if (selected_text) {
        printf("\nSelected text: %ls\n", selected_text);
        free(selected_text);
    } else {
        printf("\nNo selection made.\n");
    }

    return 0;
}
