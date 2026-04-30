#pragma once

#include <string>

struct MemoryMatchResult {
    int pairsMatched;   // Number of pairs correctly matched (0-8)
    int livesRemaining; // Lives left (0-20)
    bool abandoned;     // Did player quit?
    bool won;           // Did they match all pairs?
};

MemoryMatchResult playMemoryMatchMinigame(const std::string& playerName, bool hasColor);