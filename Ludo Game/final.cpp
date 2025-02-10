#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>   // For sleep
#include <time.h>     // For random dice rolls
#include <stdbool.h>
#include <string.h>

#define BOARD_DIMENSION 15
#define MAX_PLAYERS 4
#define TOKENS_PER_PLAYER 4
#define HOME_PATH_LENGTH 5

typedef struct {
    int posX, posY;        // Current position on the board
    bool isInYard;         // Indicates if the token is still in the yard
    int startX, startY;    // Original starting position
    bool isInHome;         // Indicates if the token has entered the home path
    bool hasReachedHome;   // Indicates if the token has reached the final destination
} GameToken;

typedef struct {
    int playerID;                   // Unique Player ID
    GameToken tokens[TOKENS_PER_PLAYER]; // Player's tokens
    int killCount;                  // Number of opponents' tokens killed
    int sixesRolledConsecutively;   // Counter for consecutive sixes rolled
    bool isActive;                  // Status of the player in the game
    char *color;                    // Player's color representation
} PlayerInfo;

typedef struct {
    int currentTurn;           // ID of the player whose turn it is
    pthread_mutex_t mutexLock; // Mutex for synchronizing access to game state
} GameStatus;

typedef struct {
    int x, y; // Coordinates on the board
} BoardPosition;

// Global Variables
GameStatus gameStatus;
PlayerInfo playersList[MAX_PLAYERS];
BoardPosition boardPath[52];
char gameBoard[BOARD_DIMENSION][BOARD_DIMENSION];

// Home paths for each player
BoardPosition blueHomePath[HOME_PATH_LENGTH] = {
    {7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 5}
};

BoardPosition redHomePath[HOME_PATH_LENGTH] = {
    {13, 7}, {12, 7}, {11, 7}, {10, 7}, {9, 7}
};

BoardPosition greenHomePath[HOME_PATH_LENGTH] = {
    {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}
};

BoardPosition yellowHomePath[HOME_PATH_LENGTH] = {
    {7, 13}, {7, 12}, {7, 11}, {7, 10}, {7, 9}
};

// Ranking system
int playerRanks[MAX_PLAYERS] = {0};
int rankCounter = 1;
int activePlayerCount = MAX_PLAYERS;

// Starting positions for each player
const int initialPositions[MAX_PLAYERS][2] = {
    {13, 6}, // Blue
    {6, 1},  // Red
    {1, 8},  // Green
    {8, 13}  // Yellow
};

// Function to initialize the players
void setupPlayers() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        playersList[i].playerID = i + 1;
        playersList[i].killCount = 0;
        playersList[i].sixesRolledConsecutively = 0;
        playersList[i].isActive = true;
        playersList[i].color = NULL;

        // Initialize tokens in the yard
        for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
            playersList[i].tokens[j].isInYard = true;
            playersList[i].tokens[j].isInHome = false;
            playersList[i].tokens[j].hasReachedHome = false;
        }
    }

    // Assign colors to players
    playersList[0].color = "Blue";
    playersList[1].color = "Red";
    playersList[2].color = "Green";
    playersList[3].color = "Yellow";

    // Place tokens on the board for each player
    // Blue Player
    for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
        playersList[0].tokens[j].posX = 11 + (j / 2);
        playersList[0].tokens[j].posY = 2 + (j % 2);
        playersList[0].tokens[j].startX = playersList[0].tokens[j].posX;
        playersList[0].tokens[j].startY = playersList[0].tokens[j].posY;
        gameBoard[playersList[0].tokens[j].posX][playersList[0].tokens[j].posY] = '@';
    }

    // Red Player
    for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
        playersList[1].tokens[j].posX = 2 + (j / 2);
        playersList[1].tokens[j].posY = 2 + (j % 2);
        playersList[1].tokens[j].startX = playersList[1].tokens[j].posX;
        playersList[1].tokens[j].startY = playersList[1].tokens[j].posY;
        gameBoard[playersList[1].tokens[j].posX][playersList[1].tokens[j].posY] = '#';
    }

    // Green Player
    for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
        playersList[2].tokens[j].posX = 2 + (j / 2);
        playersList[2].tokens[j].posY = 11 + (j % 2);
        playersList[2].tokens[j].startX = playersList[2].tokens[j].posX;
        playersList[2].tokens[j].startY = playersList[2].tokens[j].posY;
        gameBoard[playersList[2].tokens[j].posX][playersList[2].tokens[j].posY] = '$';
    }

    // Yellow Player
    for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
        playersList[3].tokens[j].posX = 11 + (j / 2);
        playersList[3].tokens[j].posY = 11 + (j % 2);
        playersList[3].tokens[j].startX = playersList[3].tokens[j].posX;
        playersList[3].tokens[j].startY = playersList[3].tokens[j].posY;
        gameBoard[playersList[3].tokens[j].posX][playersList[3].tokens[j].posY] = '%';
    }
}

// Function to initialize the game board
void initializeBoard() {
    // Clear the board
    for (int i = 0; i < BOARD_DIMENSION; i++) {
        for (int j = 0; j < BOARD_DIMENSION; j++) {
            gameBoard[i][j] = ' ';
        }
    }

    // Define home zones
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            gameBoard[i][j] = 'R'; // Red home zone
            gameBoard[i][j + 9] = 'G'; // Green home zone
            gameBoard[i + 9][j + 9] = 'Y'; // Yellow home zone
            gameBoard[i + 9][j] = 'B'; // Blue home zone
        }
    }

    // Central area
    for (int i = 6; i <= 8; i++) {
        for (int j = 6; j <= 8; j++) {
            gameBoard[i][j] = '*';
        }
    }

    // Horizontal paths
    for (int j = 1; j < 14; j++) {
        gameBoard[7][j] = '-';
    }

    // Vertical paths
    for (int i = 1; i < 6; i++) {
        gameBoard[i][7] = '|';
    }
    for (int i = 9; i < 14; i++) {
        gameBoard[i][7] = '|';
    }

    // Safe spaces
    gameBoard[2][6] = 'S';
    gameBoard[1][8] = 'S';
    gameBoard[6][12] = 'S';
    gameBoard[8][13] = 'S';
    gameBoard[6][1] = 'S';
    gameBoard[8][2] = 'S';
    gameBoard[13][6] = 'S';
    gameBoard[12][8] = 'S';
}

// Function to display the Ludo board with colors
void displayBoard() {
    printf("\n=== LUDO GAME BOARD ===\n");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < BOARD_DIMENSION; i++) {
        for (int j = 0; j < BOARD_DIMENSION; j++) {
            if (j == 0) printf(" | ");
            switch (gameBoard[i][j]) {
                case 'R':
                    printf("\033[1;31m%2c \033[0m| ", gameBoard[i][j]); // Red
                    break;
                case 'G':
                    printf("\033[1;32m%2c \033[0m| ", gameBoard[i][j]); // Green
                    break;
                case 'Y':
                    printf("\033[1;33m%2c \033[0m| ", gameBoard[i][j]); // Yellow
                    break;
                case 'B':
                    printf("\033[1;34m%2c \033[0m| ", gameBoard[i][j]); // Blue
                    break;
                case 'S':
                    printf("\033[1;37m%2c \033[0m| ", gameBoard[i][j]); // Safe
                    break;
                case '*':
                    printf("\033[1;35m%2c \033[0m| ", gameBoard[i][j]); // Center
                    break;
                case '-' :
                case '|' :
                    printf("\033[1;36m%2c \033[0m| ", gameBoard[i][j]); // Paths
                    break;
                default:
                    printf("%2c | ", gameBoard[i][j]); // Empty
            }
        }
        printf("\n--------------------------------------------------------------\n");
    }

    // Re-mark safe spaces if they are empty
    BoardPosition safeSpots[] = {
        {2, 6}, {1, 8}, {6, 12}, {8, 13},
        {6, 1}, {8, 2}, {13, 6}, {12, 8}
    };
    int totalSafeSpots = sizeof(safeSpots) / sizeof(safeSpots[0]);
    for (int i = 0; i < totalSafeSpots; i++) {
        if (gameBoard[safeSpots[i].x][safeSpots[i].y] == ' ') {
            gameBoard[safeSpots[i].x][safeSpots[i].y] = 'S';
        }
    }
}

// Function to simulate a dice roll
int diceRoll() {
    return (rand() % 6) + 1;
}

// Function to move a token out of the yard
void releaseToken(PlayerInfo *player, int tokenIdx) {
    if (player->tokens[tokenIdx].isInYard) {
        int startX = initialPositions[player->playerID - 1][0];
        int startY = initialPositions[player->playerID - 1][1];

        // Clear the yard position
        gameBoard[player->tokens[tokenIdx].posX][player->tokens[tokenIdx].posY] = '?';

        // Update token position
        player->tokens[tokenIdx].posX = startX;
        player->tokens[tokenIdx].posY = startY;
        player->tokens[tokenIdx].isInYard = false;

        // Determine symbol based on player color
        char symbol;
        switch (player->playerID) {
            case 1: symbol = '@'; break; // Blue
            case 2: symbol = '#'; break; // Red
            case 3: symbol = '$'; break; // Green
            case 4: symbol = '%'; break; // Yellow
            default: symbol = '?'; break;
        }

        // Place token on the board
        gameBoard[startX][startY] = symbol;

        printf("Player %d released a token to position (%d, %d)\n", player->playerID, startX, startY);
    }
}

// Forward declarations for movement functions
void moveToken(PlayerInfo *player, int diceValue);

// Function to handle the dice roll outcome
void processDiceRoll(PlayerInfo *player, int roll) {
    if (roll == 6) {
        printf("Player %d rolled a 6! Attempting to release a token...\n", player->playerID);
        
        // Attempt to release a token from the yard
        for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
            if (player->tokens[i].isInYard) {
                releaseToken(player, i);
                return; // Only release one token per six
            }
        }

        // If no tokens are in the yard, grant another turn
        printf("No tokens in the yard for Player %d. They get another turn!\n", player->playerID);
        player->sixesRolledConsecutively++;
        if (player->sixesRolledConsecutively >= 3) {
            printf("Player %d rolled three consecutive sixes! Their turn is forfeited.\n", player->playerID);
            player->sixesRolledConsecutively = 0;
        }
    } else {
        printf("Player %d rolled a %d.\n", player->playerID, roll);
        player->sixesRolledConsecutively = 0; // Reset consecutive sixes
        moveToken(player, roll);
    }
}

// Thread function for each player
void *playerRoutine(void *arg) {
    PlayerInfo *player = (PlayerInfo *)arg;

    while (player->isActive) {
        pthread_mutex_lock(&gameStatus.mutexLock);

        if (gameStatus.currentTurn == player->playerID) {
            int roll = diceRoll();
            printf("Player %d's turn. Rolled: %d\n", player->playerID, roll);
            processDiceRoll(player, roll);
            displayBoard();

            // Pass turn to the next player
            gameStatus.currentTurn = (gameStatus.currentTurn % MAX_PLAYERS) + 1;
        }

        pthread_mutex_unlock(&gameStatus.mutexLock);
        usleep(30000); // Simulate turn duration
    }

    return NULL;
}

// Function to initialize the board path
void initializeBoardPath() {
    // Define the sequential path around the board
    BoardPosition tempPath[52] = {
        {0, 6}, {0, 7}, {0, 8}, {1, 8}, {2, 8}, {3, 8}, {4, 8}, {5, 8},
        {6, 9}, {6, 10}, {6, 11}, {6, 12}, {6, 13}, {6, 14}, {7, 14},
        {8, 14}, {8, 13}, {8, 12}, {8, 11}, {8, 10}, {8, 9}, {9, 8},
        {10, 8}, {11, 8}, {12, 8}, {13, 8}, {14, 8}, {14, 7}, {14, 6},
        {13, 6}, {12, 6}, {11, 6}, {10, 6}, {9, 6}, {8, 5}, {8, 4},
        {8, 3}, {8, 2}, {8, 1}, {8, 0}, {7, 0}, {6, 0}, {6, 1},
        {6, 2}, {6, 3}, {6, 4}, {6, 5}, {5, 6}, {4, 6}, {3, 6},
        {2, 6}, {1, 6}
    };
    memcpy(boardPath, tempPath, sizeof(tempPath));
}

// Function to validate if a token is on the board path
bool isOnPath(PlayerInfo *player, int tokenIdx) {
    for(int i = 0; i < 52; i++) {
        if(player->tokens[tokenIdx].posX == boardPath[i].x && 
           player->tokens[tokenIdx].posY == boardPath[i].y) {
            return true;
        }
    }
    return false;
}

// Helper functions to get token positions
int getTokenX(PlayerInfo *player, int idx) {
    return player->tokens[idx].posX;
}

int getTokenY(PlayerInfo *player, int idx) {
    return player->tokens[idx].posY;
}

// Function to update the game board when a token moves
void refreshBoard(char *playerColor, int oldX, int oldY, int newX, int newY) {
    gameBoard[oldX][oldY] = ' ';
    if (strcmp(playerColor, "Blue") == 0) {
        gameBoard[newX][newY] = '@';
    } else if (strcmp(playerColor, "Red") == 0) {
        gameBoard[newX][newY] = '#';
    } else if (strcmp(playerColor, "Green") == 0) {
        gameBoard[newX][newY] = '$';
    } else if (strcmp(playerColor, "Yellow") == 0) {
        gameBoard[newX][newY] = '%';
    }
}

// Function to validate a token's new position
bool validateTokenPosition(PlayerInfo *player, int tokenIdx, int x, int y) {
    for(int i = 0; i < 52; i++) {
        if(player->tokens[tokenIdx].posX == boardPath[i].x && 
           player->tokens[tokenIdx].posY == boardPath[i].y) {
            return true;
        }
    }
    return false;
}

// Function prototypes for home path management
bool enterHomePath(PlayerInfo *player, int tokenIdx, int diceValue);
bool isInHomePath(PlayerInfo *player, int tokenIdx);
bool canAdvanceInHomePath(PlayerInfo *player, int tokenIdx, int diceValue);

// Check if a token is in the home path
bool isInHomePath(PlayerInfo *player, int tokenIdx) {
    BoardPosition *homePath;
    switch(player->playerID) {
        case 1: homePath = blueHomePath; break;
        case 2: homePath = redHomePath; break;
        case 3: homePath = greenHomePath; break;
        case 4: homePath = yellowHomePath; break;
        default: return false;
    }

    for (int i = 0; i < HOME_PATH_LENGTH; i++) {
        if (player->tokens[tokenIdx].posX == homePath[i].x && 
            player->tokens[tokenIdx].posY == homePath[i].y) {
            return true;
        }
    }
    return false;
}

// Check if a token can move within the home path
bool canAdvanceInHomePath(PlayerInfo *player, int tokenIdx, int diceValue) {
    BoardPosition *homePath;
    switch(player->playerID) {
        case 1: homePath = blueHomePath; break;
        case 2: homePath = redHomePath; break;
        case 3: homePath = greenHomePath; break;
        case 4: homePath = yellowHomePath; break;
        default: return false;
    }

    int currentIndex = -1;
    for (int i = 0; i < HOME_PATH_LENGTH; i++) {
        if (player->tokens[tokenIdx].posX == homePath[i].x && 
            player->tokens[tokenIdx].posY == homePath[i].y) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == -1 || currentIndex + diceValue >= HOME_PATH_LENGTH) {
        return false;
    }

    int newIndex = currentIndex + diceValue;
    player->tokens[tokenIdx].posX = homePath[newIndex].x;
    player->tokens[tokenIdx].posY = homePath[newIndex].y;

    refreshBoard(player->color, homePath[currentIndex].x, homePath[currentIndex].y, 
                homePath[newIndex].x, homePath[newIndex].y);

    printf("Player %d's token moved within home path to (%d, %d)\n", 
           player->playerID, homePath[newIndex].x, homePath[newIndex].y);

    if (newIndex == HOME_PATH_LENGTH - 1) {
        player->tokens[tokenIdx].isInHome = true;
        player->tokens[tokenIdx].hasReachedHome = true;
        gameBoard[player->tokens[tokenIdx].posX][player->tokens[tokenIdx].posY] = ' ';
        printf("Player %d's token has reached home!\n", player->playerID);
    }

    return true;
}

// Check if a token can enter the home path
bool canEnterHomePath(PlayerInfo *player, int tokenIdx) {
    int currentX = player->tokens[tokenIdx].posX;
    int currentY = player->tokens[tokenIdx].posY;

    // Define home entry points for each player
    BoardPosition homeEntries[MAX_PLAYERS] = {
        {8, 7},   // Blue
        {6, 7},   // Red
        {7, 6},   // Green
        {7, 8}    // Yellow
    };

    BoardPosition entryPoint = homeEntries[player->playerID - 1];

    bool isNearEntry = (
        abs(currentX - entryPoint.x) <= 1 && 
        abs(currentY - entryPoint.y) <= 1
    );

    return isNearEntry;
}

// Function to move a token into the home path
bool enterHomePath(PlayerInfo *player, int tokenIdx, int diceValue) {
    BoardPosition *homePath;
    switch(player->playerID) {
        case 1: homePath = blueHomePath; break;
        case 2: homePath = redHomePath; break;
        case 3: homePath = greenHomePath; break;
        case 4: homePath = yellowHomePath; break;
        default: return false;
    }

    int oldX = player->tokens[tokenIdx].posX;
    int oldY = player->tokens[tokenIdx].posY;

    BoardPosition newPos = homePath[0];
    player->tokens[tokenIdx].posX = newPos.x;
    player->tokens[tokenIdx].posY = newPos.y;
    player->tokens[tokenIdx].isInHome = true;
    player->tokens[tokenIdx].isInYard = false;

    refreshBoard(player->color, oldX, oldY, newPos.x, newPos.y);

    printf("Player %d's token entered home path at (%d, %d)\n", 
           player->playerID, newPos.x, newPos.y);

    return true;
}

// Function to eliminate an opponent's token
bool eliminateOpponent(PlayerInfo *currentPlayer, int newX, int newY) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == currentPlayer->playerID - 1) continue; // Skip self

        for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
            if (!playersList[i].tokens[j].isInYard && 
                playersList[i].tokens[j].posX == newX && 
                playersList[i].tokens[j].posY == newY) {
                
                // Determine the symbol based on opponent's color
                char opponentSymbol;
                switch (i) {
                    case 0: opponentSymbol = '@'; break; // Blue
                    case 1: opponentSymbol = '#'; break; // Red
                    case 2: opponentSymbol = '$'; break; // Green
                    case 3: opponentSymbol = '%'; break; // Yellow
                    default: opponentSymbol = '?'; break;
                }

                // Remove opponent's token from the board
                gameBoard[newX][newY] = ' ';

                // Reset opponent's token to the yard
                playersList[i].tokens[j].isInYard = true;
                playersList[i].tokens[j].posX = playersList[i].tokens[j].startX;
                playersList[i].tokens[j].posY = playersList[i].tokens[j].startY;

                // Place the token back in the yard on the board
                gameBoard[playersList[i].tokens[j].posX][playersList[i].tokens[j].posY] = opponentSymbol;

                // Increment current player's kill count
                currentPlayer->killCount++;

                printf("Player %d has eliminated a token of Player %d!\n", 
                       currentPlayer->playerID, playersList[i].playerID);
                return true;
            }
        }
    }
    return false;
}

// Function to move a token based on dice value
void moveToken(PlayerInfo *player, int diceValue) {
    int movableTokens[TOKENS_PER_PLAYER];
    int movableCount = 0;

    // Identify movable tokens
    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (!player->tokens[i].isInYard && !player->tokens[i].isInHome) {
            movableTokens[movableCount++] = i;
        }
    }

    // Prioritize moving tokens in the home path
    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (isInHomePath(player, i)) {
            if (canAdvanceInHomePath(player, i, diceValue)) {
                return;
            }
        }
    }

    if (movableCount == 0) {
        printf("Player %d has no tokens available to move.\n", player->playerID);
        return;
    }

    // Select a random token to move
    int selectedToken = -1;
    for (int attempts = 0; attempts < TOKENS_PER_PLAYER; attempts++) {
        selectedToken = rand() % TOKENS_PER_PLAYER;
        if (!player->tokens[selectedToken].isInYard && !player->tokens[selectedToken].isInHome) {
            break;
        }
    }

    if (selectedToken == -1 || player->tokens[selectedToken].isInYard || player->tokens[selectedToken].isInHome) {
        printf("No valid tokens found for Player %d to move.\n", player->playerID);
        return;
    }

    int currentX = player->tokens[selectedToken].posX;
    int currentY = player->tokens[selectedToken].posY;

    // Find current position in the path
    int pathIndex = -1;
    for (int i = 0; i < 52; i++) {
        if (boardPath[i].x == currentX && boardPath[i].y == currentY) {
            pathIndex = i;
            break;
        }
    }

    if (pathIndex == -1) {
        printf("Error: Player %d's token not found on the path.\n", player->playerID);
        return;
    }

    // Calculate new position
    int newPathIndex = (pathIndex + diceValue) % 52;
    int newX = boardPath[newPathIndex].x;
    int newY = boardPath[newPathIndex].y;

    // Check if token can enter the home path
    if (canEnterHomePath(player, selectedToken)) {
        if (enterHomePath(player, selectedToken, diceValue)) {
            return;
        }
    }

    // Check and eliminate any opponent's token at the new position
    eliminateOpponent(player, newX, newY);

    // Update the board and token's position
    refreshBoard(player->color, currentX, currentY, newX, newY);
    player->tokens[selectedToken].posX = newX;
    player->tokens[selectedToken].posY = newY;

    printf("Player %d moved a token to (%d, %d)\n", player->playerID, newX, newY);
    printf("Player %d's kill count: %d\n", player->playerID, player->killCount);
}

// Master thread to monitor game status and rankings
void *gameMonitor(void *arg) {
    while (1) {
        pthread_mutex_lock(&gameStatus.mutexLock);

        int activePlayers = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (playersList[i].isActive) {
                activePlayers++;

                // Check if all tokens have reached home
                bool allTokensHome = true;
                for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
                    if (!playersList[i].tokens[j].hasReachedHome) {
                        allTokensHome = false;
                        break;
                    }
                }

                // Assign ranking if all tokens are home
                if (allTokensHome && playerRanks[i] == 0) {
                    playerRanks[i] = rankCounter++;
                    activePlayerCount--;
                }
            }
        }

        // End the game if only one player remains
        if (activePlayerCount <= 1) {
            printf("=== GAME OVER ===\n");
            printf("Final Rankings:\n");
            for (int r = 1; r <= MAX_PLAYERS; r++) {
                for (int p = 0; p < MAX_PLAYERS; p++) {
                    if (playerRanks[p] == r) {
                        printf("%d. Player %d\n", r, playersList[p].playerID);
                    }
                }
            }

            // Display kill counts
            for (int i = 0; i < MAX_PLAYERS; i++) {
                printf("Player %d's total kills: %d\n", playersList[i].playerID, playersList[i].killCount);
            }

            exit(0);
        }

        pthread_mutex_unlock(&gameStatus.mutexLock);
        sleep(1);
    }

    return NULL;
}

int main() {
    srand(time(NULL));

    // Initialize game components
    initializeBoardPath();
    initializeBoard();
    setupPlayers();
    displayBoard();

    // Initialize threading
    pthread_t playerThreads[MAX_PLAYERS];
    pthread_t monitorThread;
    pthread_mutex_init(&gameStatus.mutexLock, NULL);

    gameStatus.currentTurn = 1;

    // Start the game monitor thread
    pthread_create(&monitorThread, NULL, gameMonitor, NULL);

    // Start player threads
    for (int i = 0; i < MAX_PLAYERS; i++) {
        pthread_create(&playerThreads[i], NULL, playerRoutine, &playersList[i]);
    }

    // Wait for all player threads to finish
    for (int i = 0; i < MAX_PLAYERS; i++) {
        pthread_join(playerThreads[i], NULL);
    }

    // Clean up
    pthread_cancel(monitorThread);
    pthread_mutex_destroy(&gameStatus.mutexLock);

    return 0;
}