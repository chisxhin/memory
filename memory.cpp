#include "memory.hpp"

#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>
#include <random>

// Color constants
enum MemoryColorPair {
    MEMORY_GOLD_BLACK = 1,
    MEMORY_GOLD_SAND = 8,
    MEMORY_BLACK_FOREST = 13,
    MEMORY_GOLD_TERRA = 15
};

namespace {

const int GRID_SIZE = 4;
const int TOTAL_PAIRS = 8;
const int TOTAL_CELLS = GRID_SIZE * GRID_SIZE;
const int STARTING_LIVES = 20;
const int MAX_HELP_USES = 5;
const int HELP_REVEAL_SECONDS = 2;
const int CELL_WIDTH = 5;
const int CELL_HEIGHT = 3;
const int TOTAL_GRID_WIDTH = GRID_SIZE * CELL_WIDTH;

const std::vector<char> SYMBOLS = {
    '~', '@', '#', '$', '%', '&', '+', '*'
};

struct Cell {
    char symbol;
    bool revealed;
    bool matched;
};

void drawBoxAtSafe(WINDOW* window, int y, int x, int height, int width) {
    for (int i = 0; i < width; ++i) {
        mvwaddch(window, y, x + i, ACS_HLINE);
        mvwaddch(window, y + height - 1, x + i, ACS_HLINE);
    }
    for (int i = 0; i < height; ++i) {
        mvwaddch(window, y + i, x, ACS_VLINE);
        mvwaddch(window, y + i, x + width - 1, ACS_VLINE);
    }
    mvwaddch(window, y, x, ACS_ULCORNER);
    mvwaddch(window, y, x + width - 1, ACS_URCORNER);
    mvwaddch(window, y + height - 1, x, ACS_LLCORNER);
    mvwaddch(window, y + height - 1, x + width - 1, ACS_LRCORNER);
}

void shuffleGrid(std::vector<Cell>& grid) {
    std::vector<char> symbols;
    for (int i = 0; i < TOTAL_PAIRS; ++i) {
        symbols.push_back(SYMBOLS[i]);
        symbols.push_back(SYMBOLS[i]);
    }

    std::shuffle(symbols.begin(), symbols.end(), std::mt19937(std::random_device()()));

    for (int i = 0; i < TOTAL_CELLS; ++i) {
        grid[i].symbol = symbols[i];
        grid[i].revealed = false;
        grid[i].matched = false;
    }
}

void initMemoryColors() {
    if (has_colors()) {
        init_pair(MEMORY_GOLD_BLACK, COLOR_YELLOW, COLOR_BLACK);
        init_pair(MEMORY_GOLD_SAND, COLOR_YELLOW, COLOR_BLACK);
        init_pair(MEMORY_BLACK_FOREST, COLOR_GREEN, COLOR_BLACK);
        init_pair(MEMORY_GOLD_TERRA, COLOR_RED, COLOR_BLACK);
    }
}

void drawCell(WINDOW* win, int y, int x, char symbol, bool isRevealed, bool isMatched, 
              bool isSelected, bool revealAll) {
    // Draw border
    if (isSelected) {
        wattron(win, A_REVERSE);
    }
    for (int i = 0; i < CELL_WIDTH; ++i) {
        mvwaddch(win, y, x + i, ACS_HLINE);
        mvwaddch(win, y + CELL_HEIGHT - 1, x + i, ACS_HLINE);
    }
    for (int i = 0; i < CELL_HEIGHT; ++i) {
        mvwaddch(win, y + i, x, ACS_VLINE);
        mvwaddch(win, y + i, x + CELL_WIDTH - 1, ACS_VLINE);
    }
    mvwaddch(win, y, x, ACS_ULCORNER);
    mvwaddch(win, y, x + CELL_WIDTH - 1, ACS_URCORNER);
    mvwaddch(win, y + CELL_HEIGHT - 1, x, ACS_LLCORNER);
    mvwaddch(win, y + CELL_HEIGHT - 1, x + CELL_WIDTH - 1, ACS_LRCORNER);
    if (isSelected) {
        wattroff(win, A_REVERSE);
    }

    // Draw content
    if (isMatched || revealAll) {
        wattron(win, A_BOLD);
        mvwprintw(win, y + 1, x + 2, "%c", symbol);
        wattroff(win, A_BOLD);
    } else if (isRevealed) {
        mvwprintw(win, y + 1, x + 2, "%c", symbol);
    } else {
        mvwprintw(win, y + 1, x + 2, "?");
    }
}

} // anonymous namespace

void drawAsciiTitle(WINDOW* win, int screenW, bool hasColor) {
    const std::vector<std::string> MEMORY_TITLE = {
        R"(___  ___ ________  ________________   __  _____   ___  ___  ___ _____ )",
        R"(|  \/  ||  ___|  \/  |  _  | ___ \ \ / / |  __ \ / _ \ |  \/  ||  ___|)",
        R"(| .  . || |__ | .  . | | | | |_/ /\ V /  | |  \// /_\ \| .  . || |__  )",
        R"(| |\/| ||  __|| |\/| | | | |    /  \ /   | | __ |  _  || |\/| ||  __| )",
        R"(| |  | || |___| |  | \ \_/ / |\ \  | |   | |_\ \| | | || |  | || |___ )",
        R"(\_|  |_/\____/\_|  |_/\___/\_| \_| \_/    \____/\_| |_/\_|  |_/\____/ )",
        R"(                                                                       )"
    };
    
    if (hasColor) {
        wattron(win, COLOR_PAIR(MEMORY_GOLD_SAND) | A_BOLD);
    }
    
    for (size_t i = 0; i < MEMORY_TITLE.size(); i++) {
        int titleLen = MEMORY_TITLE[i].length();
        mvwprintw(win, 1 + i, (screenW - titleLen) / 2, "%s", MEMORY_TITLE[i].c_str());
    }
    
    if (hasColor) {
        wattroff(win, COLOR_PAIR(MEMORY_GOLD_SAND) | A_BOLD);
    }
}


MemoryMatchResult playMemoryMatchMinigame(const std::string& playerName, bool hasColor) {
    MemoryMatchResult result;
    result.pairsMatched = 0;
    result.livesRemaining = STARTING_LIVES;
    result.abandoned = false;
    result.won = false;

    WINDOW* overlay = newwin(0, 0, 0, 0);
    keypad(overlay, TRUE);

    if (hasColor) {
        initMemoryColors();
    }

    std::vector<Cell> grid(TOTAL_CELLS);
    shuffleGrid(grid);

    int currentRow = 0;
    int currentCol = 0;

    int helpUses = 0;
    bool waitingForSecondMatch = false;
    int firstMatchIdx = -1;
    bool memorizationPhase = true;
    auto memorizationStart = std::time(nullptr);

    while (result.livesRemaining > 0 && result.pairsMatched < TOTAL_PAIRS) {
        // Get current terminal size
        int screenH, screenW;
        getmaxyx(stdscr, screenH, screenW);

        // Center the grid
        // Center the grid, but leave room for the title
        int gridStartY = (screenH - (GRID_SIZE * CELL_HEIGHT)) / 2;
        if (gridStartY < 15) gridStartY = 15;  // Start below the title and status
        int gridStartX = (screenW - TOTAL_GRID_WIDTH) / 2;
        if (gridStartX < 4) gridStartX = 4;

        // Resize overlay to fill screen
        wresize(overlay, screenH, screenW);
        mvwin(overlay, 0, 0);

        werase(overlay);

        if (hasColor) {
            wbkgd(overlay, COLOR_PAIR(MEMORY_GOLD_BLACK));
        }

        // Define arena dimensions
        const int arenaWidth = 70;
        const int arenaHeight = 24;
        const int arenaLeft = (screenW - arenaWidth) / 2;
        const int arenaTop = gridStartY - 2;

        // Draw ASCII title (centered)
        drawAsciiTitle(overlay, screenW, hasColor);

        // Status line - below the title
        char status[100];
        snprintf(status, sizeof(status), "Player: %s  |  Pairs: %d/8  |  Lives: %d",
                playerName.c_str(), result.pairsMatched, result.livesRemaining);
        int statusLen = strlen(status);
        if (hasColor) {
            wattron(overlay, COLOR_PAIR(MEMORY_GOLD_SAND) | A_BOLD);
        }
        mvwprintw(overlay, 9, (screenW - statusLen) / 2, "%s", status);

        // Help text
        char helpText[100];
        snprintf(helpText, sizeof(helpText), "Help uses: %d/5 (press H)", MAX_HELP_USES - helpUses);
        int helpLen = strlen(helpText);
        mvwprintw(overlay, 10, (screenW - helpLen) / 2, "%s", helpText);

        // Calculate box dimensions based on title width
        int titleWidth = 75;  // Width of the ASCII title
        int boxWidth = titleWidth + 4;  // Add padding
        int boxHeight = (GRID_SIZE * CELL_HEIGHT) + 8;  // Taller box with more padding
        int boxStartX = (screenW - boxWidth) / 2;
        int boxStartY = 12;  // Start below status text

        // Draw the big box
        if (hasColor) {
            wattron(overlay, COLOR_PAIR(MEMORY_GOLD_SAND));
        }
        drawBoxAtSafe(overlay, boxStartY, boxStartX, boxHeight, boxWidth);

        // Draw inner box border for depth
        drawBoxAtSafe(overlay, boxStartY + 1, boxStartX + 2, boxHeight - 2, boxWidth - 4);
        if (hasColor) {
            wattroff(overlay, COLOR_PAIR(MEMORY_GOLD_SAND));
        }

        // Calculate grid position inside the box
        gridStartY = boxStartY + (boxHeight - (GRID_SIZE * CELL_HEIGHT)) / 2;
        gridStartX = boxStartX + (boxWidth - TOTAL_GRID_WIDTH) / 2;

        // Instructions (below the box)
        const char* instructions = "Arrow Keys: Move  |  ENTER/Space: Select  |  H: Help  |  Q: Quit";
        int instrLen = strlen(instructions);
        if (hasColor) {
            wattron(overlay, COLOR_PAIR(MEMORY_GOLD_SAND) | A_BOLD);
        }
        mvwprintw(overlay, boxStartY + boxHeight + 2, (screenW - instrLen) / 2, "%s", instructions);
        if (hasColor) {
            wattroff(overlay, COLOR_PAIR(MEMORY_GOLD_SAND) | A_BOLD);
        }

        // Draw a decorative line above the instructions
        const char* divider = "════════════════════════════════════════════════════════════════════════════";
        int divLen = strlen(divider);
        if (screenW > divLen) {
            mvwprintw(overlay, boxStartY + boxHeight + 1, (screenW - divLen) / 2, "%s", divider);
        }

        // Memorization phase
        if (memorizationPhase) {
            // Draw all cells revealed
            for (int row = 0; row < GRID_SIZE; ++row) {
                for (int col = 0; col < GRID_SIZE; ++col) {
                    int idx = row * GRID_SIZE + col;
                    int x = gridStartX + col * CELL_WIDTH;
                    int y = gridStartY + row * CELL_HEIGHT;
                    drawCell(overlay, y, x, grid[idx].symbol, true, false, false, true);
                }
            }

            mvwprintw(overlay, gridStartY + (GRID_SIZE * CELL_HEIGHT) + 3, (screenW - 30) / 2, 
                      "Memorize the positions! 5 seconds...");
            wrefresh(overlay);

            if (std::time(nullptr) - memorizationStart >= 5) {
                memorizationPhase = false;
                for (int i = 0; i < TOTAL_CELLS; ++i) {
                    if (!grid[i].matched) {
                        grid[i].revealed = false;
                    }
                }
            }
            napms(50);
            continue;
        }

        // Draw grid
        for (int row = 0; row < GRID_SIZE; ++row) {
            for (int col = 0; col < GRID_SIZE; ++col) {
                int idx = row * GRID_SIZE + col;
                int x = gridStartX + col * CELL_WIDTH;
                int y = gridStartY + row * CELL_HEIGHT;
                bool isSelected = (row == currentRow && col == currentCol && 
                                  !grid[idx].matched && !grid[idx].revealed);
                drawCell(overlay, y, x, grid[idx].symbol, grid[idx].revealed, 
                        grid[idx].matched, isSelected, false);
            }
        }

        wrefresh(overlay);

        // Handle input
        int ch = wgetch(overlay);

        if (ch == 'q' || ch == 'Q') {
            result.abandoned = true;
            break;
        }

        if (ch == 'h' || ch == 'H') {
            if (helpUses < MAX_HELP_USES) {
                helpUses++;
                // Reveal all cells
                for (int row = 0; row < GRID_SIZE; ++row) {
                    for (int col = 0; col < GRID_SIZE; ++col) {
                        int idx = row * GRID_SIZE + col;
                        int x = gridStartX + col * CELL_WIDTH;
                        int y = gridStartY + row * CELL_HEIGHT;
                        drawCell(overlay, y, x, grid[idx].symbol, true, false, false, true);
                    }
                }
                wrefresh(overlay);
                napms(HELP_REVEAL_SECONDS * 1000);
                continue;
            }
        }

        // Movement
        if (ch == KEY_UP) {
            currentRow = (currentRow - 1 + GRID_SIZE) % GRID_SIZE;
        } else if (ch == KEY_DOWN) {
            currentRow = (currentRow + 1) % GRID_SIZE;
        } else if (ch == KEY_LEFT) {
            currentCol = (currentCol - 1 + GRID_SIZE) % GRID_SIZE;
        } else if (ch == KEY_RIGHT) {
            currentCol = (currentCol + 1) % GRID_SIZE;
        } 
        // Selection
        else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ') {
            int cellIdx = currentRow * GRID_SIZE + currentCol;

            if (!grid[cellIdx].matched && !grid[cellIdx].revealed) {
                if (!waitingForSecondMatch) {
                    //Reveal first tile
                    firstMatchIdx = cellIdx;
                    grid[cellIdx].revealed = true;
                    waitingForSecondMatch = true;
                    //(debug)
                    //mvwprintw(overlay, gridStartY - 3, (screenW - 40) / 2, "First tile selected!     ");
                    
                    //Force immediate redraw to show the first revealed tile
                    wrefresh(overlay);
                }
                else if (cellIdx != firstMatchIdx) {
                    //Reveal second tile and check for match
                    grid[cellIdx].revealed = true;
                     
                    //debug
                    //mvwprintw(overlay, gridStartY - 3, (screenW - 40) / 2, "Second tile selected! Checking...");
                    
                    //Force immediate redraw to show BOTH tiles
                    wrefresh(overlay);

                    //Force a complete redraw of the grid
                    //Redraw all cells to ensure second tile is visible
                    for (int row = 0; row < GRID_SIZE; ++row) {
                        for (int col = 0; col < GRID_SIZE; ++col) {
                            int idx = row * GRID_SIZE + col;
                            int x = gridStartX + col * CELL_WIDTH;
                            int y = gridStartY + row * CELL_HEIGHT;
                            bool isSelected = (row == currentRow && col == currentCol && 
                                            !grid[idx].matched && !grid[idx].revealed);
                            drawCell(overlay, y, x, grid[idx].symbol, grid[idx].revealed, 
                                    grid[idx].matched, isSelected, false);
                        }
                    }
                    wrefresh(overlay);

                    //Give player time to see BOTH tiles clearly
                    napms(1500);

                    if (grid[firstMatchIdx].symbol == grid[cellIdx].symbol) {
                        //Match found - keep both tiles open and mark as matched
                        grid[firstMatchIdx].matched = true;
                        grid[cellIdx].matched = true;
                        result.pairsMatched++;
                        //debug
                        //mvwprintw(overlay, gridStartY - 3, (screenW - 40) / 2, "MATCH! +1 pair            ");
                        wrefresh(overlay);
                    } 
                    else {
                        //Doesn't match - hide both tiles and lose a life
                        //debug
                        //mvwprintw(overlay, gridStartY - 3, (screenW - 40) / 2, "NO MATCH! -1 life         ");
                        wrefresh(overlay);
                        napms(800); //So players can see it for a bit more after a mismatch

                        result.livesRemaining--;
                        grid[firstMatchIdx].revealed = false;
                        grid[cellIdx].revealed = false;

                        mvwprintw(overlay, gridStartY - 3, (screenW - 40) / 2, "                         ");
                    }
                    waitingForSecondMatch = false;
                    firstMatchIdx = -1;

                    //Force another redraw after the mismatch to update the grid state
                    wrefresh(overlay);
                }
            }
        }
    }

    result.won = (result.pairsMatched == TOTAL_PAIRS);

    // End screen
    int screenH, screenW;
    getmaxyx(stdscr, screenH, screenW);

    werase(overlay);
    if (hasColor) {
        wattron(overlay, result.won ? COLOR_PAIR(MEMORY_BLACK_FOREST) : COLOR_PAIR(MEMORY_GOLD_TERRA) | A_BOLD);
    }

    if (result.won) {
        mvwprintw(overlay, screenH/2 - 2, (screenW - 30) / 2, "VICTORY!");
        mvwprintw(overlay, screenH/2 - 1, (screenW - 50) / 2, "You matched all %d pairs!", TOTAL_PAIRS);
        mvwprintw(overlay, screenH/2, (screenW - 40) / 2, "Lives remaining: %d", result.livesRemaining);
    } else if (result.abandoned) {
        mvwprintw(overlay, screenH/2 - 1, (screenW - 30) / 2, "Game abandoned.");
    } else {
        mvwprintw(overlay, screenH/2 - 2, (screenW - 30) / 2, "GAME OVER!");
        mvwprintw(overlay, screenH/2 - 1, (screenW - 45) / 2, "You ran out of lives!");
        mvwprintw(overlay, screenH/2, (screenW - 40) / 2, "Pairs matched: %d/8", result.pairsMatched);
    }

    mvwprintw(overlay, screenH/2 + 2, (screenW - 30) / 2, "Press ENTER to continue.");
    if (hasColor) {
        wattroff(overlay, COLOR_PAIR(MEMORY_BLACK_FOREST) | A_BOLD);
        wattroff(overlay, COLOR_PAIR(MEMORY_GOLD_TERRA) | A_BOLD);
    }
    wrefresh(overlay);

    int ch;
    do {
        ch = wgetch(overlay);
    } while (ch != '\n' && ch != '\r' && ch != KEY_ENTER);

    delwin(overlay);
    touchwin(stdscr);
    refresh();

    return result;
}