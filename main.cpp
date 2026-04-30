#include <ncurses.h>
#include "memory.hpp"

int main() {
    // Initialize curses.
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
    }

    clear();
    mvprintw(1, 2, "Testing Memory Match Game");
    mvprintw(2, 2, "Press any key to start...");
    refresh();
    getch();

    MemoryMatchResult result = playMemoryMatchMinigame("TESTER", has_colors());

    clear();
    mvprintw(5, 2, "=== MEMORY MATCH RESULTS ===");
    mvprintw(7, 4, "Won: %s", result.won ? "YES" : "NO");
    mvprintw(8, 4, "Pairs matched: %d/8", result.pairsMatched);
    mvprintw(9, 4, "Lives remaining: %d", result.livesRemaining);
    mvprintw(10, 4, "Abandoned: %s", result.abandoned ? "YES" : "NO");
    mvprintw(12, 2, "Press any key to exit...");
    refresh();
    getch();

    endwin();
    return 0;
}

