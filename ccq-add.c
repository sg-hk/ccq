#include "ccq.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <termios.h>
#include <unistd.h>

// int height = DB_SIZE;
//
// b_search()
//
// lucene_search()


int get_width(wchar_t wc) {
    return wcwidth(wc); // 0 for zero-width, -1 for invalid, or the width of the character
}

wchar_t *interactive_select(const wchar_t *input)
{
    struct termios original;
    tcgetattr(STDIN_FILENO, &original);

    dis_raw(&original);

    int cursor_pos = 0;
    int start_select = -1; // -1 indicates no selection
    size_t len = wcslen(input);
    wchar_t *selection = NULL;

    while (1) {
        printf("\033[H\033[J"); // clear screen, reset cursor

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
        printf("\nUse arrow keys to move (Left/Right), 'v' to toggle selection,");
	printf(" Enter to confirm, 'q' to quit.");

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
                int start = start_select < cursor_pos ? start_select : cursor_pos;
                int end = start_select > cursor_pos ? start_select : cursor_pos;

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

    en_raw(&original);
    return selection;
}

Card add_card(wchar_t *sentence) 
{
	Card new_card = {{NULL, NULL}, {NULL, NULL, NULL},
				{0, 0.0, 0.0, 0.0, 0, 0}};	

	new_card.word.key = interactive_select(sentence);
	
	// duplicate check in deck

	new_card.word.definition = b_search(new_card.word.key).word.definition;
	new_card.context.recordings = b_search(new_card.word.key).context.recordings;

	// add malloc logic here
	int i = 0;
	while (new_card.context.sentences[i] != NULL) {
		++i;
	}
	new_card.context.sentences[i] = add_sentence(new_card.word.key);
	new_card.context.sentences[i+1] = NULL;

	// instead of returning the card, add skip list logic to find where to insert the new line
	// then fprintf etc.
	// and make this function void type
	return new_card;
}


wchar_t *add_sentence(wchar_t *key)
{
	wchar_t *full_hits = lucene_search(key);
	wchar_t *sentence = interactive_select(full_hits);
	return sentence;
}
