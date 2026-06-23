/* ============================================================
   QUORIDOR PAC-MAN — LP1 Trabajo Práctico
   Versión "todo en un archivo".

   El archivo está organizado en secciones, de abajo hacia
   arriba en nivel de abstracción:

     1. Constantes y tipos        (las "fichas" y el tablero)
     2. Tablero y muros           (canMove, placeWall, línea de vista)
     3. Mapas                     (cargar, guardar, listar)
     4. Estado del juego          (GameState: todo en un lugar)
     5. Lógica del juego          (turnos, capturas, victoria)
     6. IA de los fantasmas       (BFS + dificultad)
     7. Dibujado                  (una función por pantalla)
     8. Input                     (una función por pantalla)
     9. main                      (loop principal)

   Compilar en Linux:
     gcc quoridor_pacman.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o quoridor_pacman
   ============================================================ */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <dirent.h>   /* para listar la carpeta maps/ (Linux y MinGW) */

/* ============================================================
   1. CONSTANTES Y TIPOS
   ============================================================ */

/* ----- IDs de los fantasmas (usados como índice) ----- */
#define GHOST_BLINKY  0   /* rojo  */
#define GHOST_INKY    1   /* cian  */
#define GHOST_PINKY   2   /* rosa  */
#define GHOST_CLYDE   3   /* naranja */
#define NUM_GHOSTS    4

#define NUM_BALLS     4
#define PACMAN_INITIAL_LIVES 3

/* ----- Modos de juego ----- */
#define MODE_AI_VS_PLAYER  0   /* IA maneja fantasmas */
#define MODE_PVP           1   /* segundo jugador maneja fantasmas */

/* ----- Niveles de dificultad de la IA ----- */
#define DIFFICULTY_EASY    1   /* 30% óptimo */
#define DIFFICULTY_MEDIUM  2   /* 60% óptimo */
#define DIFFICULTY_HARD    3   /* 100% óptimo + usa muros */

/* ----- Tamaño máximo del tablero (límite de los arreglos) ----- */
#define MAX_ROWS 20
#define MAX_COLS 20

/* ----- Direcciones de movimiento ----- */
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

/* ----- Pantallas posibles ----- */
#define SCREEN_MENU       0
#define SCREEN_CONFIG     1
#define SCREEN_MAP_SELECT 2
#define SCREEN_EDITOR     3
#define SCREEN_GAME       4
#define SCREEN_GAMEOVER   5

/* ----- A quién le toca mover ----- */
#define TURN_PACMAN  0
#define TURN_GHOST   1

/* ----- Modos de input ----- */
#define INPUT_MOVE       0   /* modo normal: flechas para moverse */
#define INPUT_PLACE_WALL 1   /* modo colocación de muro */

#define MAP_NAME_LEN 256
#define MAX_PERM_WALLS 200
#define MAX_EXPIRED_WALLS 50

/* ----- Pac-Man: posición, vidas y muros en mano ----- */
typedef struct {
    int row, col;       /* posición actual (0,0 = arriba a la izquierda) */
    int startRow, startCol; /* posición inicial — se usa al resetear */
    int lives;
    int wallsInHand;    /* muros temporales disponibles para colocar */
} PacMan;

/* ----- Fantasma: posición, estado y dificultad propia ----- */
typedef struct {
    int row, col;
    int startRow, startCol;
    int id;             /* GHOST_BLINKY / INKY / PINKY / CLYDE */
    bool enabled;       /* false = no aparece en el tablero */
    bool alive;         /* false = fue comido por Pac-Man este round */
    int difficulty;
} Ghost;

/* ----- Pac-bola: las comidas no vuelven al resetear ----- */
typedef struct {
    int row, col;
    bool eaten;
} PacBall;

/* ----- Tipos de muro ----- */
typedef enum {
    WALL_NONE = 0,    /* no hay muro */
    WALL_PERMANENT,   /* muro del mapa, no desaparece */
    WALL_TEMP         /* colocado por un jugador, tiene tiempo de vida */
} WallType;

/* Cada slot entre casillas puede tener un muro. Si es temporal,
   guardamos cuántos turnos le quedan y quién lo puso (para
   devolverlo a la mano correcta cuando expire). */
typedef struct {
    WallType type;
    int turnsLeft;  /* solo importa si type == WALL_TEMP */
    int owner;      /* 0 = equipo Pac-Man, 1 = equipo fantasmas */
} Wall;

/* El tablero: los muros NO están dentro de las celdas sino
   ENTRE ellas. hWalls[i][j] = muro entre fila i e i+1.
   vWalls[i][j] = muro entre columna j y j+1. */
typedef struct {
    int rows, cols;
    Wall hWalls[MAX_ROWS - 1][MAX_COLS];
    Wall vWalls[MAX_ROWS][MAX_COLS - 1];
} Board;

/* ----- Datos de un mapa (lo que se guarda en el archivo .map) ----- */
typedef struct {
    int rows, cols;
    int pacmanRow, pacmanCol;
    int ghostRow[NUM_GHOSTS], ghostCol[NUM_GHOSTS];
    int ballRow[NUM_BALLS], ballCol[NUM_BALLS];
    int wallRow[MAX_PERM_WALLS];
    int wallCol[MAX_PERM_WALLS];
    char wallDir[MAX_PERM_WALLS]; /* 'H' o 'V' */
    int wallCount;
} MapData;

/* ----- Configuración elegida antes de empezar ----- */
typedef struct {
    int gameMode;
    bool ghostEnabled[NUM_GHOSTS];
    int ghostDifficulty[NUM_GHOSTS];
    int pacmanWalls;
    int ghostWallsTotal;
    int wallLifetime;              /* turnos globales que dura un muro */
    char mapFile[MAP_NAME_LEN];
} GameConfig;

/* ----- Estado del editor de mapas ----- */
typedef struct {
    MapData draft;          /* mapa que se está construyendo */
    int cursorRow, cursorCol;
    char saveName[MAP_NAME_LEN];
    bool typing;            /* true cuando se escribe el nombre */
} EditorState;

/* ----- TODO el estado del juego en un solo struct ----- */
typedef struct {
    Board board;
    PacMan pacman;
    Ghost ghosts[NUM_GHOSTS];
    PacBall balls[NUM_BALLS];
    GameConfig config;

    int currentTurn;      /* TURN_PACMAN o TURN_GHOST */
    int actionsLeft;
    int activeGhostIdx;   /* qué fantasma está moviendo ahora */
    int globalTurn;       /* contador de rondas completas */
    int ghostWallsInHand; /* mano de muros compartida del equipo fantasmas */

    int inputMode;        /* INPUT_MOVE o INPUT_PLACE_WALL */
    int wallCursorRow, wallCursorCol, wallCursorDir;

    int ballsEaten;
    bool gameOver;
    bool pacmanWins;

    /* true desde que come una pac-bola hasta el fin de su turno:
       mientras dura, pisar un fantasma lo come en vez de perder vida */
    bool poweredUp;

    bool wantExit;        /* true cuando el jugador eligió salir */

    int screen;
    EditorState editor;

    char availableMaps[10][MAP_NAME_LEN];
    int mapCount;
    int selectedMapIdx;
} GameState;

/* ----- Prototipos de funciones que se llaman entre sí ----- */
void game_endPacmanTurn(GameState *gs);
void game_endGhostTurn(GameState *gs);
void game_tickWalls(GameState *gs);
bool game_ghostPlaceWall(GameState *gs, int ghostIdx, int row, int col, int dir);

/* ============================================================
   2. TABLERO Y MUROS
   ============================================================ */

/* Pone todo en cero: sin muros, tamaño definido */
void board_init(Board *b, int rows, int cols) {
    b->rows = rows;
    b->cols = cols;
    /* memset a 0 equivale a poner todos los muros en WALL_NONE */
    memset(b->hWalls, 0, sizeof(b->hWalls));
    memset(b->vWalls, 0, sizeof(b->vWalls));
}

/* Verifica si se puede mover desde (row,col) en la dirección dada.
   Moverse hacia arriba implica cruzar hWalls[row-1][col];
   hacia la derecha, vWalls[row][col]. */
bool board_canMove(const Board *b, int row, int col, int dir) {
    /* primero: que el destino esté dentro del tablero */
    if (dir == DIR_UP    && row <= 0)            return false;
    if (dir == DIR_DOWN  && row >= b->rows - 1)  return false;
    if (dir == DIR_LEFT  && col <= 0)            return false;
    if (dir == DIR_RIGHT && col >= b->cols - 1)  return false;

    /* segundo: que no haya muro en ese lado */
    switch (dir) {
        case DIR_UP:    return b->hWalls[row - 1][col].type == WALL_NONE;
        case DIR_DOWN:  return b->hWalls[row][col].type     == WALL_NONE;
        case DIR_LEFT:  return b->vWalls[row][col - 1].type == WALL_NONE;
        case DIR_RIGHT: return b->vWalls[row][col].type     == WALL_NONE;
    }
    return false;
}

/* Coloca un muro entre (row,col) y el vecino en dir.
   No se puede colocar donde ya hay otro: sobrescribirlo haría
   desaparecer el anterior y descuadraría el conteo de manos. */
bool board_placeWall(Board *b, int row, int col, int dir,
                     WallType type, int owner, int lifetime) {
    Wall w;
    w.type      = type;
    w.turnsLeft = lifetime;
    w.owner     = owner;

    switch (dir) {
        case DIR_UP:
            if (row <= 0) return false;
            if (b->hWalls[row - 1][col].type != WALL_NONE) return false;
            b->hWalls[row - 1][col] = w;
            return true;
        case DIR_DOWN:
            if (row >= b->rows - 1) return false;
            if (b->hWalls[row][col].type != WALL_NONE) return false;
            b->hWalls[row][col] = w;
            return true;
        case DIR_LEFT:
            if (col <= 0) return false;
            if (b->vWalls[row][col - 1].type != WALL_NONE) return false;
            b->vWalls[row][col - 1] = w;
            return true;
        case DIR_RIGHT:
            if (col >= b->cols - 1) return false;
            if (b->vWalls[row][col].type != WALL_NONE) return false;
            b->vWalls[row][col] = w;
            return true;
    }
    return false;
}

/* Resta 1 turno a cada muro temporal. Si llega a 0, el muro
   desaparece y se anota su dueño en expiredOwners para que la
   lógica del juego le devuelva el muro a la mano.
   Devuelve cuántos muros expiraron. */
int board_tickWalls(Board *b, int *expiredOwners, int maxExpired) {
    int count = 0;

    for (int i = 0; i < b->rows - 1; i++) {
        for (int j = 0; j < b->cols; j++) {
            Wall *w = &b->hWalls[i][j];
            if (w->type == WALL_TEMP) {
                w->turnsLeft--;
                if (w->turnsLeft <= 0) {
                    if (count < maxExpired) expiredOwners[count] = w->owner;
                    count++;
                    w->type = WALL_NONE;
                }
            }
        }
    }
    for (int i = 0; i < b->rows; i++) {
        for (int j = 0; j < b->cols - 1; j++) {
            Wall *w = &b->vWalls[i][j];
            if (w->type == WALL_TEMP) {
                w->turnsLeft--;
                if (w->turnsLeft <= 0) {
                    if (count < maxExpired) expiredOwners[count] = w->owner;
                    count++;
                    w->type = WALL_NONE;
                }
            }
        }
    }
    return count;
}

/* True si el fantasma en (fRow,fCol) ve a Pac-Man en (pRow,pCol)
   en línea recta sin muros en el medio. Se usa para el modo
   frenético. Los otros fantasmas no tapan la vista (la regla lo
   dice explícitamente), por eso solo se chequean muros. */
bool board_lineOfSight(const Board *b, int fRow, int fCol, int pRow, int pCol) {
    if (fRow != pRow && fCol != pCol) return false;

    if (fRow == pRow) {
        /* misma fila: revisar muros verticales entre los dos */
        int cMin = fCol < pCol ? fCol : pCol;
        int cMax = fCol > pCol ? fCol : pCol;
        for (int c = cMin; c < cMax; c++)
            if (b->vWalls[fRow][c].type != WALL_NONE) return false;
        return true;
    } else {
        /* misma columna: revisar muros horizontales entre los dos */
        int rMin = fRow < pRow ? fRow : pRow;
        int rMax = fRow > pRow ? fRow : pRow;
        for (int r = rMin; r < rMax; r++)
            if (b->hWalls[r][fCol].type != WALL_NONE) return false;
        return true;
    }
}

/* ============================================================
   3. MAPAS — carga, guardado y listado

   Formato del archivo .map (texto plano):
     ROWS COLS
     PM_ROW PM_COL
     ... 4 líneas de fantasmas ...
     ... 4 líneas de pac-bolas ...
     NUM_WALLS
     ROW COL H|V   (un muro permanente por línea)
   ============================================================ */

bool map_load(const char *filename, MapData *out) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;

    if (fscanf(f, "%d %d", &out->rows, &out->cols) != 2) { fclose(f); return false; }
    if (fscanf(f, "%d %d", &out->pacmanRow, &out->pacmanCol) != 2) { fclose(f); return false; }

    for (int i = 0; i < NUM_GHOSTS; i++)
        if (fscanf(f, "%d %d", &out->ghostRow[i], &out->ghostCol[i]) != 2) { fclose(f); return false; }

    for (int i = 0; i < NUM_BALLS; i++)
        if (fscanf(f, "%d %d", &out->ballRow[i], &out->ballCol[i]) != 2) { fclose(f); return false; }

    if (fscanf(f, "%d", &out->wallCount) != 1) { fclose(f); return false; }
    for (int i = 0; i < out->wallCount && i < MAX_PERM_WALLS; i++) {
        char dir;
        if (fscanf(f, "%d %d %c", &out->wallRow[i], &out->wallCol[i], &dir) != 3) {
            fclose(f); return false;
        }
        out->wallDir[i] = dir;
    }

    fclose(f);
    return true;
}

bool map_save(const char *filename, const MapData *data) {
    FILE *f = fopen(filename, "w");
    if (!f) return false;

    fprintf(f, "%d %d\n", data->rows, data->cols);
    fprintf(f, "%d %d\n", data->pacmanRow, data->pacmanCol);
    for (int i = 0; i < NUM_GHOSTS; i++)
        fprintf(f, "%d %d\n", data->ghostRow[i], data->ghostCol[i]);
    for (int i = 0; i < NUM_BALLS; i++)
        fprintf(f, "%d %d\n", data->ballRow[i], data->ballCol[i]);
    fprintf(f, "%d\n", data->wallCount);
    for (int i = 0; i < data->wallCount; i++)
        fprintf(f, "%d %d %c\n", data->wallRow[i], data->wallCol[i], data->wallDir[i]);

    fclose(f);
    return true;
}

/* Copia los datos del mapa al estado real del juego */
void map_apply(const MapData *data, Board *board,
               PacMan *pacman, Ghost ghosts[], PacBall balls[]) {

    board_init(board, data->rows, data->cols);

    pacman->row      = data->pacmanRow;
    pacman->col      = data->pacmanCol;
    pacman->startRow = data->pacmanRow;
    pacman->startCol = data->pacmanCol;

    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghosts[i].row      = data->ghostRow[i];
        ghosts[i].col      = data->ghostCol[i];
        ghosts[i].startRow = data->ghostRow[i];
        ghosts[i].startCol = data->ghostCol[i];
        ghosts[i].id       = i;
        ghosts[i].alive    = true;
    }

    for (int i = 0; i < NUM_BALLS; i++) {
        balls[i].row   = data->ballRow[i];
        balls[i].col   = data->ballCol[i];
        balls[i].eaten = false;
    }

    /* H = muro horizontal (bloquea arriba/abajo)
       V = muro vertical   (bloquea izquierda/derecha) */
    for (int i = 0; i < data->wallCount; i++) {
        int dir = (data->wallDir[i] == 'H') ? DIR_DOWN : DIR_RIGHT;
        board_placeWall(board, data->wallRow[i], data->wallCol[i],
                        dir, WALL_PERMANENT, -1, 0);
    }
}

/* Recorre la carpeta maps/ y devuelve los archivos .map.
   Así los mapas guardados con el editor aparecen automáticamente
   en la pantalla de selección. */
int map_listAvailable(char names[][MAP_NAME_LEN], int maxCount) {
    int count = 0;

    DIR *dir = opendir("maps");
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < maxCount) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 5) continue;                     /* mínimo "x.map" */
        if (strcmp(name + len - 4, ".map") != 0) continue;

        snprintf(names[count], MAP_NAME_LEN, "maps/%s", name);
        count++;
    }

    closedir(dir);
    return count;
}

/* ============================================================
   4 y 5. ESTADO Y LÓGICA DEL JUEGO
   ============================================================ */

/* Valores de inicio razonables para una partida 9×9 */
void config_setDefaults(GameConfig *cfg) {
    cfg->gameMode        = MODE_AI_VS_PLAYER;
    cfg->pacmanWalls     = 3;
    cfg->ghostWallsTotal = 1;
    cfg->wallLifetime    = 4;

    for (int i = 0; i < NUM_GHOSTS; i++) {
        cfg->ghostEnabled[i]    = true;
        cfg->ghostDifficulty[i] = DIFFICULTY_MEDIUM;
    }

    strncpy(cfg->mapFile, "maps/map1.map", MAP_NAME_LEN - 1);
}

/* Carga el mapa, aplica la configuración y deja todo listo
   para el primer turno de Pac-Man */
void game_init(GameState *gs) {
    MapData mapData;
    if (!map_load(gs->config.mapFile, &mapData)) {
        /* si falla la carga, usar el mapa 1 por defecto */
        map_load("maps/map1.map", &mapData);
    }
    map_apply(&mapData, &gs->board, &gs->pacman, gs->ghosts, gs->balls);

    for (int i = 0; i < NUM_GHOSTS; i++) {
        gs->ghosts[i].enabled    = gs->config.ghostEnabled[i];
        gs->ghosts[i].difficulty = gs->config.ghostDifficulty[i];
        gs->ghosts[i].alive      = gs->config.ghostEnabled[i];
    }

    gs->pacman.lives       = PACMAN_INITIAL_LIVES;
    gs->pacman.wallsInHand = gs->config.pacmanWalls;
    gs->ghostWallsInHand   = gs->config.ghostWallsTotal;

    gs->currentTurn    = TURN_PACMAN;
    gs->actionsLeft    = 2;
    gs->activeGhostIdx = 0;
    gs->globalTurn     = 0;
    gs->inputMode      = INPUT_MOVE;
    gs->ballsEaten     = 0;
    gs->gameOver       = false;
    gs->pacmanWins     = false;
    gs->poweredUp      = false;

    gs->wallCursorRow = gs->pacman.row;
    gs->wallCursorCol = gs->pacman.col;
    gs->wallCursorDir = DIR_DOWN;

    gs->mapCount = map_listAvailable(gs->availableMaps, 10);
    gs->selectedMapIdx = 0;
}

/* Reinicia la partida con la misma configuración y mapa */
void game_restart(GameState *gs) {
    game_init(gs);
    gs->screen = SCREEN_GAME;
}

/* Verifica si Pac-Man comparte casilla con algún fantasma.
   Si Pac-Man está "powered" (comió pac-bola este turno) y es su
   turno, se come al fantasma. Si no, pierde una vida y se
   resetean las posiciones (los muros temporales se quedan). */
static void _checkCapture(GameState *gs) {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (!gs->ghosts[i].enabled || !gs->ghosts[i].alive) continue;

        if (gs->ghosts[i].row == gs->pacman.row &&
            gs->ghosts[i].col == gs->pacman.col) {

            /* regla 2.5.3: con power-up, pisar un fantasma lo come */
            if (gs->poweredUp && gs->currentTurn == TURN_PACMAN) {
                gs->ghosts[i].alive = false;
                continue;
            }

            gs->pacman.lives--;

            if (gs->pacman.lives <= 0) {
                gs->gameOver   = true;
                gs->pacmanWins = false;
                return;
            }

            /* reset: Pac-Man y todos los fantasmas habilitados
               (incluso los comidos) vuelven a su posición inicial */
            gs->pacman.row = gs->pacman.startRow;
            gs->pacman.col = gs->pacman.startCol;
            for (int j = 0; j < NUM_GHOSTS; j++) {
                if (gs->ghosts[j].enabled) {
                    gs->ghosts[j].row   = gs->ghosts[j].startRow;
                    gs->ghosts[j].col   = gs->ghosts[j].startCol;
                    gs->ghosts[j].alive = true;
                }
            }
            return;
        }
    }
}

/* Si Pac-Man está sobre una pac-bola: la come, gana una acción
   extra y queda "powered" hasta el fin de su turno */
static void _checkPacBall(GameState *gs) {
    for (int i = 0; i < NUM_BALLS; i++) {
        if (!gs->balls[i].eaten &&
            gs->balls[i].row == gs->pacman.row &&
            gs->balls[i].col == gs->pacman.col) {

            gs->balls[i].eaten = true;
            gs->ballsEaten++;
            gs->actionsLeft++;   /* acción extra por comer la bola */
            gs->poweredUp = true;

            /* victoria: las 4 pac-bolas comidas */
            if (gs->ballsEaten >= NUM_BALLS) {
                gs->gameOver   = true;
                gs->pacmanWins = true;
            }
            return;
        }
    }
}

/* Mueve a Pac-Man una casilla. Usa una acción del turno. */
bool game_movePacman(GameState *gs, int dir) {
    if (gs->currentTurn != TURN_PACMAN) return false;
    if (gs->actionsLeft <= 0) return false;
    if (!board_canMove(&gs->board, gs->pacman.row, gs->pacman.col, dir)) return false;

    if (dir == DIR_UP)    gs->pacman.row--;
    if (dir == DIR_DOWN)  gs->pacman.row++;
    if (dir == DIR_LEFT)  gs->pacman.col--;
    if (dir == DIR_RIGHT) gs->pacman.col++;
    gs->actionsLeft--;

    /* la bola se revisa primero: si en esa casilla también hay un
       fantasma, el power-up recién ganado hace que se lo coma */
    _checkPacBall(gs);
    if (!gs->gameOver) _checkCapture(gs);

    /* si no le quedan acciones, pasar turno automáticamente */
    if (gs->actionsLeft <= 0 && !gs->gameOver) {
        game_endPacmanTurn(gs);
    }
    return true;
}

/* Pac-Man coloca un muro temporal del lado que indica el cursor.
   Gasta una acción. */
bool game_pacmanPlaceWall(GameState *gs) {
    if (gs->currentTurn != TURN_PACMAN) return false;
    if (gs->actionsLeft <= 0) return false;
    if (gs->pacman.wallsInHand <= 0) return false;

    bool ok = board_placeWall(&gs->board,
                              gs->wallCursorRow, gs->wallCursorCol,
                              gs->wallCursorDir,
                              WALL_TEMP, 0, gs->config.wallLifetime);
    if (!ok) return false;

    gs->pacman.wallsInHand--;
    gs->actionsLeft--;
    gs->inputMode = INPUT_MOVE;

    if (gs->actionsLeft <= 0 && !gs->gameOver) {
        game_endPacmanTurn(gs);
    }
    return true;
}

/* Termina el turno de Pac-Man: tick de muros y pasa al primer
   fantasma habilitado */
void game_endPacmanTurn(GameState *gs) {
    game_tickWalls(gs);

    /* el power-up de la pac-bola dura solo el turno en que la comió */
    gs->poweredUp = false;

    gs->currentTurn    = TURN_GHOST;
    gs->activeGhostIdx = 0;
    gs->actionsLeft    = 1;
    gs->inputMode      = INPUT_MOVE;

    /* saltar fantasmas deshabilitados o comidos */
    while (gs->activeGhostIdx < NUM_GHOSTS &&
           (!gs->ghosts[gs->activeGhostIdx].enabled ||
            !gs->ghosts[gs->activeGhostIdx].alive)) {
        gs->activeGhostIdx++;
    }

    /* si no hay fantasmas válidos, volver directo a Pac-Man */
    if (gs->activeGhostIdx >= NUM_GHOSTS) {
        gs->currentTurn = TURN_PACMAN;
        gs->actionsLeft = 2;
        gs->globalTurn++;
    }
}

/* Mueve el fantasma una casilla — o dos si está en modo frenético
   (ve a Pac-Man en línea recta al momento de moverse) */
bool game_moveGhost(GameState *gs, int ghostIdx, int dir) {
    Ghost *g = &gs->ghosts[ghostIdx];
    if (!g->enabled || !g->alive) return false;

    int steps = 1;
    if (board_lineOfSight(&gs->board, g->row, g->col,
                          gs->pacman.row, gs->pacman.col)) {
        steps = 2;   /* modo frenético */
    }

    for (int s = 0; s < steps; s++) {
        if (!board_canMove(&gs->board, g->row, g->col, dir)) break;

        if (dir == DIR_UP)    g->row--;
        if (dir == DIR_DOWN)  g->row++;
        if (dir == DIR_LEFT)  g->col--;
        if (dir == DIR_RIGHT) g->col++;

        /* verificar captura después de cada paso */
        _checkCapture(gs);
        if (gs->gameOver) return true;
    }
    return true;
}

/* El fantasma coloca un muro. Usa todo su turno. */
bool game_ghostPlaceWall(GameState *gs, int ghostIdx, int row, int col, int dir) {
    (void)ghostIdx; /* la mano de muros es compartida por el equipo */
    if (gs->ghostWallsInHand <= 0) return false;

    bool ok = board_placeWall(&gs->board, row, col, dir,
                              WALL_TEMP, 1, gs->config.wallLifetime);
    if (!ok) return false;

    gs->ghostWallsInHand--;
    return true;
}

/* Pasa al siguiente fantasma habilitado y vivo.
   Si ya jugaron todos, vuelve el turno a Pac-Man. */
void game_endGhostTurn(GameState *gs) {
    gs->activeGhostIdx++;

    while (gs->activeGhostIdx < NUM_GHOSTS &&
           (!gs->ghosts[gs->activeGhostIdx].enabled ||
            !gs->ghosts[gs->activeGhostIdx].alive)) {
        gs->activeGhostIdx++;
    }

    if (gs->activeGhostIdx >= NUM_GHOSTS) {
        gs->currentTurn    = TURN_PACMAN;
        gs->actionsLeft    = 2;
        gs->activeGhostIdx = 0;
        gs->globalTurn++;
    } else {
        gs->actionsLeft = 1;
    }
}

/* Decrementa contadores de muros temporales y devuelve los
   expirados a la mano del equipo que los colocó */
void game_tickWalls(GameState *gs) {
    int expiredOwners[MAX_EXPIRED_WALLS];
    int count = board_tickWalls(&gs->board, expiredOwners, MAX_EXPIRED_WALLS);

    for (int i = 0; i < count; i++) {
        if (expiredOwners[i] == 0) gs->pacman.wallsInHand++;
        else                       gs->ghostWallsInHand++;
    }
}

/* Tabla de niveles según pac-bolas comidas (sección 2.7) */
const char* game_getLevelName(int ballsEaten) {
    switch (ballsEaten) {
        case 1: return "Pac-Man novato";
        case 2: return "Pac-Man prometedor";
        case 3: return "Pac-Man de categoria";
        case 4: return "Pac-Man de elite";
        default: return "Sin nivel";
    }
}

/* ============================================================
   6. IA DE LOS FANTASMAS

   BFS (búsqueda en anchura) para el camino más corto hasta
   Pac-Man respetando los muros. La dificultad inyecta
   aleatoriedad: en fácil solo sigue el camino óptimo el 30%
   de las veces.
   ============================================================ */

/* Nodo de la cola de BFS: guarda la dirección del PRIMER paso
   del camino, que es lo único que necesitamos al final */
typedef struct {
    int row, col;
    int firstDir;
} BFSNode;

/* Devuelve la dirección del primer paso del camino óptimo
   hacia Pac-Man, o -1 si no hay camino */
int ai_findBestMove(const GameState *gs, int ghostIdx) {
    const Ghost *g = &gs->ghosts[ghostIdx];
    const Board *b = &gs->board;
    int targetRow = gs->pacman.row;
    int targetCol = gs->pacman.col;

    bool visited[MAX_ROWS][MAX_COLS];
    memset(visited, 0, sizeof(visited));

    BFSNode queue[MAX_ROWS * MAX_COLS];
    int head = 0, tail = 0;

    visited[g->row][g->col] = true;

    int dirs[4] = { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
    int dRow[4] = { -1, 1, 0, 0 };
    int dCol[4] = { 0, 0, -1, 1 };

    /* sembrar la cola con los 4 vecinos, cada uno recordando
       por qué dirección se salió de la posición inicial */
    for (int d = 0; d < 4; d++) {
        if (!board_canMove(b, g->row, g->col, dirs[d])) continue;
        int nr = g->row + dRow[d];
        int nc = g->col + dCol[d];
        if (visited[nr][nc]) continue;
        visited[nr][nc] = true;
        queue[tail++] = (BFSNode){ nr, nc, dirs[d] };
    }

    while (head < tail) {
        BFSNode cur = queue[head++];

        if (cur.row == targetRow && cur.col == targetCol) {
            return cur.firstDir;
        }

        for (int d = 0; d < 4; d++) {
            if (!board_canMove(b, cur.row, cur.col, dirs[d])) continue;
            int nr = cur.row + dRow[d];
            int nc = cur.col + dCol[d];
            if (visited[nr][nc]) continue;
            visited[nr][nc] = true;
            queue[tail++] = (BFSNode){ nr, nc, cur.firstDir };
        }
    }

    return -1; /* sin camino */
}

/* Elige una dirección aleatoria válida, o -1 si está encerrado */
static int _randomDir(const GameState *gs, int ghostIdx) {
    const Ghost *g = &gs->ghosts[ghostIdx];
    int dirs[4] = { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

    /* mezclar para que sea realmente aleatorio */
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
    }
    for (int i = 0; i < 4; i++)
        if (board_canMove(&gs->board, g->row, g->col, dirs[i]))
            return dirs[i];
    return -1;
}

/* Solo en dificultad difícil: si tiene muros y ve a Pac-Man,
   intenta bloquearlo colocando un muro a su lado.
   Devuelve true si colocó el muro (gastó el turno). */
static bool _shouldUseWall(GameState *gs, int ghostIdx) {
    if (gs->ghosts[ghostIdx].difficulty < DIFFICULTY_HARD) return false;
    if (gs->ghostWallsInHand <= 0) return false;

    int pRow = gs->pacman.row;
    int pCol = gs->pacman.col;

    if (board_lineOfSight(&gs->board, gs->ghosts[ghostIdx].row,
                          gs->ghosts[ghostIdx].col, pRow, pCol)) {
        if (game_ghostPlaceWall(gs, ghostIdx, pRow, pCol, DIR_DOWN))  return true;
        if (game_ghostPlaceWall(gs, ghostIdx, pRow, pCol, DIR_RIGHT)) return true;
    }
    return false;
}

/* Punto de entrada de la IA: decide y ejecuta el turno completo
   del fantasma.
     Fácil (1):   30% camino óptimo, 70% aleatorio
     Medio (2):   60% óptimo, 40% aleatorio
     Difícil (3): 100% óptimo + usa muros para bloquear */
void ai_takeTurn(GameState *gs, int ghostIdx) {
    Ghost *g = &gs->ghosts[ghostIdx];
    if (!g->enabled || !g->alive) {
        game_endGhostTurn(gs);
        return;
    }

    /* inicializar la semilla aleatoria solo la primera vez */
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }

    if (g->difficulty == DIFFICULTY_HARD) {
        if (_shouldUseWall(gs, ghostIdx)) {
            game_endGhostTurn(gs);
            return;
        }
    }

    int roll = rand() % 100;
    int optimalThreshold;
    switch (g->difficulty) {
        case DIFFICULTY_EASY:   optimalThreshold = 30;  break;
        case DIFFICULTY_MEDIUM: optimalThreshold = 60;  break;
        case DIFFICULTY_HARD:   optimalThreshold = 100; break;
        default:                optimalThreshold = 60;
    }

    int dir = -1;
    if (roll < optimalThreshold) {
        dir = ai_findBestMove(gs, ghostIdx);
    }
    if (dir == -1) {
        /* aleatorio (o BFS sin camino) */
        dir = _randomDir(gs, ghostIdx);
    }

    if (dir != -1) {
        game_moveGhost(gs, ghostIdx, dir);
    }

    game_endGhostTurn(gs);
}

/* ============================================================
   7. DIBUJADO (raylib)
   ============================================================ */

#define SCREEN_W     900
#define SCREEN_H     650
#define HUD_X        680    /* donde empieza el panel lateral */
#define BOARD_MARGIN  20
#define BOARD_MAX_W  640
#define BOARD_MAX_H  600

#define WALL_THICK_PERM  5
#define WALL_THICK_TEMP  3

static const Color COL_BOARD_BG    = { 10,  20,  60, 255 };
static const Color COL_GRID_LINE   = { 40,  60, 100, 255 };
static const Color COL_WALL_PERM   = {  0, 120, 255, 255 }; /* azul */
static const Color COL_WALL_TEMP   = {  0, 230, 130, 255 }; /* verde */
static const Color COL_WALL_CURSOR = { 255, 200,   0, 150 };
static const Color COL_PACMAN      = { 255, 220,   0, 255 };
static const Color COL_BALL        = { 255, 255, 200, 255 };

/* Tamaño de cada casilla en pixels para que el tablero entre
   en el espacio disponible, sea cual sea la grilla */
static int _cellSize(int rows, int cols) {
    int szW = BOARD_MAX_W / cols;
    int szH = BOARD_MAX_H / rows;
    int sz = szW < szH ? szW : szH;
    if (sz < 20) sz = 20;
    return sz;
}

static int _cellX(int col, int cellSz) { return BOARD_MARGIN + col * cellSz + cellSz / 2; }
static int _cellY(int row, int cellSz) { return BOARD_MARGIN + row * cellSz + cellSz / 2; }

/* Grilla + muros. Los temporales se ven distintos de los
   permanentes (verde fino con contador vs azul grueso) — es
   un requisito del enunciado. */
static void _drawBoard(const GameState *gs) {
    const Board *b = &gs->board;
    int sz = _cellSize(b->rows, b->cols);

    DrawRectangle(BOARD_MARGIN, BOARD_MARGIN,
                  b->cols * sz, b->rows * sz, COL_BOARD_BG);

    for (int r = 0; r <= b->rows; r++)
        DrawLine(BOARD_MARGIN, BOARD_MARGIN + r * sz,
                 BOARD_MARGIN + b->cols * sz, BOARD_MARGIN + r * sz, COL_GRID_LINE);
    for (int c = 0; c <= b->cols; c++)
        DrawLine(BOARD_MARGIN + c * sz, BOARD_MARGIN,
                 BOARD_MARGIN + c * sz, BOARD_MARGIN + b->rows * sz, COL_GRID_LINE);

    /* muros horizontales */
    for (int r = 0; r < b->rows - 1; r++) {
        for (int c = 0; c < b->cols; c++) {
            const Wall *w = &b->hWalls[r][c];
            if (w->type == WALL_NONE) continue;

            Color col = (w->type == WALL_PERMANENT) ? COL_WALL_PERM : COL_WALL_TEMP;
            int thick = (w->type == WALL_PERMANENT) ? WALL_THICK_PERM : WALL_THICK_TEMP;
            int x1 = BOARD_MARGIN + c * sz;
            int y1 = BOARD_MARGIN + (r + 1) * sz;
            DrawLineEx((Vector2){x1, y1}, (Vector2){x1 + sz, y1}, thick, col);

            if (w->type == WALL_TEMP) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%d", w->turnsLeft);
                DrawText(buf, x1 + sz/2 - 4, y1 - 12, 10, COL_WALL_TEMP);
            }
        }
    }
    /* muros verticales */
    for (int r = 0; r < b->rows; r++) {
        for (int c = 0; c < b->cols - 1; c++) {
            const Wall *w = &b->vWalls[r][c];
            if (w->type == WALL_NONE) continue;

            Color col = (w->type == WALL_PERMANENT) ? COL_WALL_PERM : COL_WALL_TEMP;
            int thick = (w->type == WALL_PERMANENT) ? WALL_THICK_PERM : WALL_THICK_TEMP;
            int x1 = BOARD_MARGIN + (c + 1) * sz;
            int y1 = BOARD_MARGIN + r * sz;
            DrawLineEx((Vector2){x1, y1}, (Vector2){x1, y1 + sz}, thick, col);

            if (w->type == WALL_TEMP) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%d", w->turnsLeft);
                DrawText(buf, x1 + 2, y1 + sz/2 - 6, 10, COL_WALL_TEMP);
            }
        }
    }
}

/* Pac-Man, fantasmas y pac-bolas */
static void _drawEntities(const GameState *gs) {
    const Board *b = &gs->board;
    int sz = _cellSize(b->rows, b->cols);
    int r  = sz / 2 - 3;

    for (int i = 0; i < NUM_BALLS; i++) {
        if (gs->balls[i].eaten) continue;
        DrawCircle(_cellX(gs->balls[i].col, sz), _cellY(gs->balls[i].row, sz),
                   r / 2, COL_BALL);
    }

    static const Color ghostColors[NUM_GHOSTS] = {
        { 255, 60, 60, 255 }, { 60, 220, 220, 255 },
        { 255, 160, 200, 255 }, { 255, 160, 40, 255 }
    };
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (!gs->ghosts[i].enabled || !gs->ghosts[i].alive) continue;
        int gx = _cellX(gs->ghosts[i].col, sz);
        int gy = _cellY(gs->ghosts[i].row, sz);
        DrawCircle(gx, gy, r, ghostColors[i]);
        /* ojitos */
        DrawCircle(gx - r/3, gy - r/4, r/5, WHITE);
        DrawCircle(gx + r/3, gy - r/4, r/5, WHITE);
    }

    /* Pac-Man con boca (triángulo del color del fondo) */
    int px = _cellX(gs->pacman.col, sz);
    int py = _cellY(gs->pacman.row, sz);
    DrawCircle(px, py, r, COL_PACMAN);
    DrawTriangle((Vector2){px, py},
                 (Vector2){px + r, py - r/2},
                 (Vector2){px + r, py + r/2},
                 COL_BOARD_BG);
}

/* Indicador semitransparente de dónde caería el muro */
static void _drawWallCursor(const GameState *gs) {
    if (gs->inputMode != INPUT_PLACE_WALL) return;
    if (gs->currentTurn != TURN_PACMAN) return;

    const Board *b = &gs->board;
    int sz = _cellSize(b->rows, b->cols);
    int r = gs->wallCursorRow;
    int c = gs->wallCursorCol;

    if (gs->wallCursorDir == DIR_DOWN && r < b->rows - 1) {
        int x1 = BOARD_MARGIN + c * sz;
        int y1 = BOARD_MARGIN + (r + 1) * sz;
        DrawLineEx((Vector2){x1, y1}, (Vector2){x1 + sz, y1}, 5, COL_WALL_CURSOR);
    } else if (gs->wallCursorDir == DIR_UP && r > 0) {
        int x1 = BOARD_MARGIN + c * sz;
        int y1 = BOARD_MARGIN + r * sz;
        DrawLineEx((Vector2){x1, y1}, (Vector2){x1 + sz, y1}, 5, COL_WALL_CURSOR);
    } else if (gs->wallCursorDir == DIR_RIGHT && c < b->cols - 1) {
        int x1 = BOARD_MARGIN + (c + 1) * sz;
        int y1 = BOARD_MARGIN + r * sz;
        DrawLineEx((Vector2){x1, y1}, (Vector2){x1, y1 + sz}, 5, COL_WALL_CURSOR);
    } else if (gs->wallCursorDir == DIR_LEFT && c > 0) {
        int x1 = BOARD_MARGIN + c * sz;
        int y1 = BOARD_MARGIN + r * sz;
        DrawLineEx((Vector2){x1, y1}, (Vector2){x1, y1 + sz}, 5, COL_WALL_CURSOR);
    }
}

/* Panel lateral: vidas, pac-bolas, muros, turno y controles */
static void _drawHUD(const GameState *gs) {
    int x = HUD_X;
    int y = 20;
    int lineH = 22;
    char buf[64];

    DrawText("INFORMACION", x, y, 18, RAYWHITE); y += 30;

    snprintf(buf, sizeof(buf), "Vidas: %d", gs->pacman.lives);
    DrawText(buf, x, y, 16, YELLOW); y += lineH;

    snprintf(buf, sizeof(buf), "Pac-bolas: %d/4", gs->ballsEaten);
    DrawText(buf, x, y, 16, RAYWHITE); y += lineH;

    snprintf(buf, sizeof(buf), "Muros P-M: %d", gs->pacman.wallsInHand);
    DrawText(buf, x, y, 16, RAYWHITE); y += lineH;
    snprintf(buf, sizeof(buf), "Muros Fant.: %d", gs->ghostWallsInHand);
    DrawText(buf, x, y, 16, RAYWHITE); y += lineH;

    y += 10;

    if (gs->currentTurn == TURN_PACMAN) {
        DrawText("Turno: Pac-Man", x, y, 16, YELLOW);
    } else {
        const char *names[NUM_GHOSTS] = { "Blinky", "Inky", "Pinky", "Clyde" };
        snprintf(buf, sizeof(buf), "Turno: %s",
                 (gs->activeGhostIdx < NUM_GHOSTS) ? names[gs->activeGhostIdx] : "?");
        DrawText(buf, x, y, 16, RED);
    }
    y += lineH;

    snprintf(buf, sizeof(buf), "Acciones: %d", gs->actionsLeft);
    DrawText(buf, x, y, 16, RAYWHITE); y += lineH;

    y += 20;
    DrawText("Controles:", x, y, 15, GRAY); y += lineH;
    DrawText("Flechas: avanzar", x, y, 13, GRAY); y += lineH;
    DrawText("M: colocar muro",  x, y, 13, GRAY); y += lineH;
    DrawText("Enter: fin turno", x, y, 13, GRAY); y += lineH;
    DrawText("R: reiniciar",     x, y, 13, GRAY); y += lineH;
    DrawText("ESC: menu",        x, y, 13, GRAY);

    /* aviso del modo colocación con los controles que correspondan */
    if (gs->inputMode == INPUT_PLACE_WALL) {
        DrawRectangle(HUD_X - 5, SCREEN_H - 80, 215, 55, Fade(YELLOW, 0.2f));
        DrawText("MODO: colocar muro", HUD_X, SCREEN_H - 75, 14, YELLOW);
        if (gs->currentTurn == TURN_PACMAN) {
            DrawText("Flechas: elegir lado", HUD_X, SCREEN_H - 57, 13, YELLOW);
            DrawText("Enter: confirmar  ESC: cancelar", HUD_X, SCREEN_H - 40, 12, YELLOW);
        } else {
            DrawText("WASD: elegir lado", HUD_X, SCREEN_H - 57, 13, YELLOW);
            DrawText("ESC: cancelar", HUD_X, SCREEN_H - 40, 12, YELLOW);
        }
    }
}

void render_game(GameState *gs) {
    ClearBackground(BLACK);
    _drawBoard(gs);
    _drawWallCursor(gs);
    _drawEntities(gs);
    _drawHUD(gs);
}

void render_menu(GameState *gs) {
    (void)gs;
    ClearBackground((Color){10, 20, 60, 255});
    DrawText("QUORIDOR PAC-MAN", 200, 120, 48, YELLOW);
    DrawText("LP1 - Universidad Catolica", 230, 180, 20, GRAY);
    DrawText("[1] Jugar",           320, 290, 28, RAYWHITE);
    DrawText("[2] Editor de mapas", 320, 340, 28, RAYWHITE);
    DrawText("[ESC] Salir",         320, 390, 28, GRAY);
}

void render_config(GameState *gs) {
    ClearBackground((Color){10, 20, 60, 255});
    DrawText("CONFIGURACION", 50, 30, 32, YELLOW);

    int y = 90;
    int lh = 32;
    char buf[128];

    const char *modeStr = (gs->config.gameMode == MODE_AI_VS_PLAYER)
                          ? "IA vs Jugador" : "Jugador vs Jugador";
    snprintf(buf, sizeof(buf), "[G] Modo de juego: %s", modeStr);
    DrawText(buf, 50, y, 20, RAYWHITE); y += lh;

    /* [1-4] habilita/deshabilita, [5-8] cambia dificultad */
    const char *ghostNames[NUM_GHOSTS] = { "Blinky", "Inky", "Pinky", "Clyde" };
    const char *diffNames[4] = { "", "Facil", "Medio", "Dificil" };
    for (int i = 0; i < NUM_GHOSTS; i++) {
        snprintf(buf, sizeof(buf), "[%d] %s: %s   [%d] Dif: %s",
                 i + 1, ghostNames[i],
                 gs->config.ghostEnabled[i] ? "ON " : "OFF",
                 i + 5, diffNames[gs->config.ghostDifficulty[i]]);
        DrawText(buf, 50, y, 18, RAYWHITE); y += lh;
    }

    snprintf(buf, sizeof(buf), "[+/-] Muros Pac-Man: %d", gs->config.pacmanWalls);
    DrawText(buf, 50, y, 18, RAYWHITE); y += lh;
    snprintf(buf, sizeof(buf), "[W/S] Muros fantasmas: %d", gs->config.ghostWallsTotal);
    DrawText(buf, 50, y, 18, RAYWHITE); y += lh;
    snprintf(buf, sizeof(buf), "[A/D] Vida de muro (turnos): %d", gs->config.wallLifetime);
    DrawText(buf, 50, y, 18, RAYWHITE); y += lh;
    snprintf(buf, sizeof(buf), "[M] Mapa: %s", gs->config.mapFile);
    DrawText(buf, 50, y, 18, RAYWHITE); y += lh + 10;

    DrawText("[Enter] Empezar partida", 50, y, 22, GREEN);
    DrawText("[ESC] Volver al menu",    50, y + 35, 18, GRAY);
}

void render_mapSelect(GameState *gs) {
    ClearBackground((Color){10, 20, 60, 255});
    DrawText("ELEGIR MAPA", 50, 30, 32, YELLOW);
    DrawText("Flechas arriba/abajo para navegar, Enter para elegir",
             50, 75, 16, GRAY);

    for (int i = 0; i < gs->mapCount; i++) {
        Color col = (i == gs->selectedMapIdx) ? YELLOW : RAYWHITE;
        char buf[MAP_NAME_LEN + 4];
        snprintf(buf, sizeof(buf), "%s %s",
                 (i == gs->selectedMapIdx) ? ">>" : "  ",
                 gs->availableMaps[i]);
        DrawText(buf, 50, 130 + i * 35, 20, col);
    }

    DrawText("[ESC] Volver", 50, 550, 16, GRAY);
}

void render_editor(GameState *gs) {
    ClearBackground(BLACK);

    EditorState *ed = &gs->editor;
    int sz = _cellSize(ed->draft.rows, ed->draft.cols);

    DrawRectangle(BOARD_MARGIN, BOARD_MARGIN,
                  ed->draft.cols * sz, ed->draft.rows * sz,
                  (Color){10, 20, 60, 255});

    for (int r = 0; r <= ed->draft.rows; r++)
        DrawLine(BOARD_MARGIN, BOARD_MARGIN + r * sz,
                 BOARD_MARGIN + ed->draft.cols * sz,
                 BOARD_MARGIN + r * sz, (Color){40, 60, 100, 255});
    for (int c = 0; c <= ed->draft.cols; c++)
        DrawLine(BOARD_MARGIN + c * sz, BOARD_MARGIN,
                 BOARD_MARGIN + c * sz,
                 BOARD_MARGIN + ed->draft.rows * sz, (Color){40, 60, 100, 255});

    /* muros permanentes del borrador */
    for (int i = 0; i < ed->draft.wallCount; i++) {
        int wr = ed->draft.wallRow[i];
        int wc = ed->draft.wallCol[i];
        if (ed->draft.wallDir[i] == 'H') {
            int x1 = BOARD_MARGIN + wc * sz;
            int y1 = BOARD_MARGIN + (wr + 1) * sz;
            DrawLineEx((Vector2){x1, y1}, (Vector2){x1 + sz, y1}, 5, COL_WALL_PERM);
        } else {
            int x1 = BOARD_MARGIN + (wc + 1) * sz;
            int y1 = BOARD_MARGIN + wr * sz;
            DrawLineEx((Vector2){x1, y1}, (Vector2){x1, y1 + sz}, 5, COL_WALL_PERM);
        }
    }

    /* cursor */
    int cx = BOARD_MARGIN + ed->cursorCol * sz;
    int cy = BOARD_MARGIN + ed->cursorRow * sz;
    DrawRectangleLines(cx + 2, cy + 2, sz - 4, sz - 4, YELLOW);

    /* fichas colocadas */
    int r2 = sz / 2 - 4;
    DrawCircle(BOARD_MARGIN + ed->draft.pacmanCol * sz + sz/2,
               BOARD_MARGIN + ed->draft.pacmanRow * sz + sz/2, r2, YELLOW);

    static const Color gCol[NUM_GHOSTS] = {
        {255,60,60,255},{60,220,220,255},{255,160,200,255},{255,160,40,255}
    };
    for (int i = 0; i < NUM_GHOSTS; i++)
        DrawCircle(BOARD_MARGIN + ed->draft.ghostCol[i] * sz + sz/2,
                   BOARD_MARGIN + ed->draft.ghostRow[i] * sz + sz/2, r2, gCol[i]);
    for (int i = 0; i < NUM_BALLS; i++)
        DrawCircle(BOARD_MARGIN + ed->draft.ballCol[i] * sz + sz/2,
                   BOARD_MARGIN + ed->draft.ballRow[i] * sz + sz/2, r2/2, COL_BALL);

    /* instrucciones laterales */
    int hx = HUD_X;
    DrawText("EDITOR DE MAPAS", hx, 20, 18, YELLOW);

    char sizeBuf[32];
    snprintf(sizeBuf, sizeof(sizeBuf), "Tamano: %d x %d",
             ed->draft.rows, ed->draft.cols);
    DrawText(sizeBuf, hx, 48, 15, YELLOW);

    DrawText("Flechas: mover cursor", hx, 75, 14, RAYWHITE);

    const char *modes[] = {
        "T/G: filas +/-",
        "Y/H: columnas +/-",
        "P: colocar Pac-Man",
        "B: colocar Blinky",
        "I: colocar Inky",
        "N: colocar Pinky",
        "C: colocar Clyde",
        "1-4: colocar bola 1-4",
        "W: colocar muro abajo",
        "V: colocar muro der.",
        "X: borrar muro"
    };
    for (int i = 0; i < 11; i++)
        DrawText(modes[i], hx, 105 + i * 20, 13, GRAY);

    DrawText("S: guardar mapa", hx, 335, 15, GREEN);
    DrawText("ESC: volver",     hx, 365, 15, GRAY);

    /* campo de nombre si está guardando */
    if (ed->typing) {
        DrawRectangle(50, SCREEN_H - 80, 500, 50, Fade(BLACK, 0.8f));
        char buf[MAP_NAME_LEN + 20];
        snprintf(buf, sizeof(buf), "Nombre: %s|", ed->saveName);
        DrawText(buf, 60, SCREEN_H - 65, 20, YELLOW);
    }
}

void render_gameover(GameState *gs) {
    ClearBackground(BLACK);

    if (gs->pacmanWins) {
        DrawText("!PAC-MAN GANA!", 200, 150, 48, YELLOW);
    } else {
        DrawText("LOS FANTASMAS GANAN", 140, 150, 40, RED);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Nivel alcanzado: %s",
             game_getLevelName(gs->ballsEaten));
    DrawText(buf, 200, 240, 26, RAYWHITE);

    snprintf(buf, sizeof(buf), "Pac-bolas comidas: %d / 4", gs->ballsEaten);
    DrawText(buf, 260, 290, 20, GRAY);

    DrawText("[R] Jugar de nuevo",   280, 380, 24, GREEN);
    DrawText("[ESC] Volver al menu", 280, 425, 20, GRAY);
}

/* ============================================================
   8. INPUT (teclado)
   ============================================================ */

void input_menu(GameState *gs) {
    if (IsKeyPressed(KEY_ONE)) {
        gs->screen = SCREEN_CONFIG;
    }
    if (IsKeyPressed(KEY_TWO)) {
        /* inicializar el editor con un mapa vacío 9×9 y
           posiciones por defecto razonables */
        memset(&gs->editor, 0, sizeof(EditorState));
        gs->editor.draft.rows = 9;
        gs->editor.draft.cols = 9;
        gs->editor.draft.pacmanRow = 4; gs->editor.draft.pacmanCol = 4;
        gs->editor.draft.ghostRow[0] = 0; gs->editor.draft.ghostCol[0] = 0;
        gs->editor.draft.ghostRow[1] = 0; gs->editor.draft.ghostCol[1] = 8;
        gs->editor.draft.ghostRow[2] = 8; gs->editor.draft.ghostCol[2] = 0;
        gs->editor.draft.ghostRow[3] = 8; gs->editor.draft.ghostCol[3] = 8;
        gs->editor.draft.ballRow[0] = 2; gs->editor.draft.ballCol[0] = 2;
        gs->editor.draft.ballRow[1] = 2; gs->editor.draft.ballCol[1] = 6;
        gs->editor.draft.ballRow[2] = 6; gs->editor.draft.ballCol[2] = 2;
        gs->editor.draft.ballRow[3] = 6; gs->editor.draft.ballCol[3] = 6;
        gs->screen = SCREEN_EDITOR;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        /* avisar a main que el jugador quiere salir */
        gs->wantExit = true;
    }
}

void input_config(GameState *gs) {
    GameConfig *cfg = &gs->config;

    if (IsKeyPressed(KEY_G)) {
        cfg->gameMode = (cfg->gameMode == MODE_AI_VS_PLAYER)
                        ? MODE_PVP : MODE_AI_VS_PLAYER;
    }

    /* 1-4: togglear habilitado/deshabilitado del fantasma */
    if (IsKeyPressed(KEY_ONE))   cfg->ghostEnabled[0] = !cfg->ghostEnabled[0];
    if (IsKeyPressed(KEY_TWO))   cfg->ghostEnabled[1] = !cfg->ghostEnabled[1];
    if (IsKeyPressed(KEY_THREE)) cfg->ghostEnabled[2] = !cfg->ghostEnabled[2];
    if (IsKeyPressed(KEY_FOUR))  cfg->ghostEnabled[3] = !cfg->ghostEnabled[3];

    /* 5-8: ciclar la dificultad de cada fantasma por separado
       (el enunciado exige dificultad independiente por fantasma) */
    int diffKeys[NUM_GHOSTS] = { KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (IsKeyPressed(diffKeys[i])) {
            cfg->ghostDifficulty[i]++;
            if (cfg->ghostDifficulty[i] > DIFFICULTY_HARD)
                cfg->ghostDifficulty[i] = DIFFICULTY_EASY;
        }
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        if (cfg->pacmanWalls < 10) cfg->pacmanWalls++;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        if (cfg->pacmanWalls > 0) cfg->pacmanWalls--;
    }

    if (IsKeyPressed(KEY_W) && cfg->ghostWallsTotal < 10) cfg->ghostWallsTotal++;
    if (IsKeyPressed(KEY_S) && cfg->ghostWallsTotal > 0)  cfg->ghostWallsTotal--;

    if (IsKeyPressed(KEY_D) && cfg->wallLifetime < 20) cfg->wallLifetime++;
    if (IsKeyPressed(KEY_A) && cfg->wallLifetime > 1)  cfg->wallLifetime--;

    if (IsKeyPressed(KEY_M)) {
        gs->mapCount = map_listAvailable(gs->availableMaps, 10);
        gs->selectedMapIdx = 0;
        gs->screen = SCREEN_MAP_SELECT;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        game_init(gs);
        gs->screen = SCREEN_GAME;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gs->screen = SCREEN_MENU;
    }
}

void input_mapSelect(GameState *gs) {
    if (IsKeyPressed(KEY_UP)   && gs->selectedMapIdx > 0) gs->selectedMapIdx--;
    if (IsKeyPressed(KEY_DOWN) && gs->selectedMapIdx < gs->mapCount - 1) gs->selectedMapIdx++;

    if (IsKeyPressed(KEY_ENTER) && gs->mapCount > 0) {
        strncpy(gs->config.mapFile, gs->availableMaps[gs->selectedMapIdx],
                MAP_NAME_LEN - 1);
        gs->screen = SCREEN_CONFIG;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gs->screen = SCREEN_CONFIG;
    }
}

/* Modo colocación de muro de Pac-Man: las flechas eligen de qué
   lado de su casilla va el muro, Enter confirma, ESC cancela */
static void _handleWallCursorInput(GameState *gs) {
    if (IsKeyPressed(KEY_UP))    gs->wallCursorDir = DIR_UP;
    if (IsKeyPressed(KEY_DOWN))  gs->wallCursorDir = DIR_DOWN;
    if (IsKeyPressed(KEY_LEFT))  gs->wallCursorDir = DIR_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) gs->wallCursorDir = DIR_RIGHT;

    /* el cursor se ancla a la posición actual de Pac-Man */
    gs->wallCursorRow = gs->pacman.row;
    gs->wallCursorCol = gs->pacman.col;

    if (IsKeyPressed(KEY_ENTER)) {
        game_pacmanPlaceWall(gs);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gs->inputMode = INPUT_MOVE; /* cancelar sin gastar acción */
    }
}

/* Turno de fantasma en PvP:
   - WASD mueve; como un fantasma tiene UNA acción por turno,
     mover termina el turno automáticamente
   - M entra en modo muro: el siguiente WASD elige el lado
   - Tab pasa el turno sin hacer nada */
static void _handleGhostPvPInput(GameState *gs) {
    int idx = gs->activeGhostIdx;
    if (idx >= NUM_GHOSTS) return;
    Ghost *g = &gs->ghosts[idx];

    if (gs->inputMode == INPUT_PLACE_WALL) {
        int dir = -1;
        if (IsKeyPressed(KEY_W)) dir = DIR_UP;
        if (IsKeyPressed(KEY_S)) dir = DIR_DOWN;
        if (IsKeyPressed(KEY_A)) dir = DIR_LEFT;
        if (IsKeyPressed(KEY_D)) dir = DIR_RIGHT;

        if (dir != -1) {
            if (game_ghostPlaceWall(gs, idx, g->row, g->col, dir)) {
                gs->inputMode = INPUT_MOVE;
                game_endGhostTurn(gs);
            }
            /* si falló (ya hay muro ahí), sigue en modo muro */
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            gs->inputMode = INPUT_MOVE; /* cancelar sin gastar el turno */
        }
        return;
    }

    int dir = -1;
    if (IsKeyPressed(KEY_W)) dir = DIR_UP;
    if (IsKeyPressed(KEY_S)) dir = DIR_DOWN;
    if (IsKeyPressed(KEY_A)) dir = DIR_LEFT;
    if (IsKeyPressed(KEY_D)) dir = DIR_RIGHT;

    if (dir != -1) {
        if (game_moveGhost(gs, idx, dir) && !gs->gameOver) {
            game_endGhostTurn(gs);
        }
        return;
    }

    if (IsKeyPressed(KEY_M) && gs->ghostWallsInHand > 0) {
        gs->inputMode = INPUT_PLACE_WALL;
    }
    if (IsKeyPressed(KEY_TAB)) {
        game_endGhostTurn(gs);
    }
}

void input_game(GameState *gs) {
    if (gs->gameOver) return;

    if (IsKeyPressed(KEY_R)) {
        game_restart(gs);
        return;
    }
    /* ESC vuelve al menú, salvo en modo colocación de muro:
       ahí ESC cancela el modo (lo maneja cada handler) */
    if (IsKeyPressed(KEY_ESCAPE) && gs->inputMode != INPUT_PLACE_WALL) {
        gs->screen = SCREEN_MENU;
        return;
    }

    if (gs->currentTurn == TURN_PACMAN) {
        if (gs->inputMode == INPUT_PLACE_WALL) {
            _handleWallCursorInput(gs);
            return;
        }

        if (IsKeyPressed(KEY_UP))    game_movePacman(gs, DIR_UP);
        if (IsKeyPressed(KEY_DOWN))  game_movePacman(gs, DIR_DOWN);
        if (IsKeyPressed(KEY_LEFT))  game_movePacman(gs, DIR_LEFT);
        if (IsKeyPressed(KEY_RIGHT)) game_movePacman(gs, DIR_RIGHT);

        if (IsKeyPressed(KEY_M) && gs->pacman.wallsInHand > 0) {
            gs->inputMode = INPUT_PLACE_WALL;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            game_endPacmanTurn(gs);
        }

    } else {
        if (gs->config.gameMode == MODE_AI_VS_PLAYER) {
            /* la IA juega el turno del fantasma activo */
            int idx = gs->activeGhostIdx;
            if (idx < NUM_GHOSTS) {
                ai_takeTurn(gs, idx);
            }
        } else {
            _handleGhostPvPInput(gs);
        }
    }
}

void input_editor(GameState *gs) {
    EditorState *ed = &gs->editor;

    /* si está escribiendo el nombre del archivo para guardar */
    if (ed->typing) {
        int key = GetCharPressed();
        int len = strlen(ed->saveName);

        if (key >= 32 && key <= 126 && len < MAP_NAME_LEN - 5) {
            ed->saveName[len]     = (char)key;
            ed->saveName[len + 1] = '\0';
        }
        if (IsKeyPressed(KEY_BACKSPACE) && len > 0) {
            ed->saveName[len - 1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER)) {
            char fullPath[MAP_NAME_LEN];
            snprintf(fullPath, sizeof(fullPath), "maps/%s.map", ed->saveName);
            map_save(fullPath, &ed->draft);
            ed->typing = false;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            ed->typing = false;
        }
        return;
    }

    /* --- cambiar el tamaño del tablero ---
       Mínimo 5×5 para que entren todas las fichas, máximo el
       tope de los arreglos del tablero. */
    if (IsKeyPressed(KEY_T) && ed->draft.rows < MAX_ROWS) ed->draft.rows++;
    if (IsKeyPressed(KEY_G) && ed->draft.rows > 5)        ed->draft.rows--;
    if (IsKeyPressed(KEY_Y) && ed->draft.cols < MAX_COLS) ed->draft.cols++;
    if (IsKeyPressed(KEY_H) && ed->draft.cols > 5)        ed->draft.cols--;

    /* al achicar, traer adentro las fichas que quedaron afuera */
    if (ed->draft.pacmanRow >= ed->draft.rows) ed->draft.pacmanRow = ed->draft.rows - 1;
    if (ed->draft.pacmanCol >= ed->draft.cols) ed->draft.pacmanCol = ed->draft.cols - 1;
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (ed->draft.ghostRow[i] >= ed->draft.rows) ed->draft.ghostRow[i] = ed->draft.rows - 1;
        if (ed->draft.ghostCol[i] >= ed->draft.cols) ed->draft.ghostCol[i] = ed->draft.cols - 1;
    }
    for (int i = 0; i < NUM_BALLS; i++) {
        if (ed->draft.ballRow[i] >= ed->draft.rows) ed->draft.ballRow[i] = ed->draft.rows - 1;
        if (ed->draft.ballCol[i] >= ed->draft.cols) ed->draft.ballCol[i] = ed->draft.cols - 1;
    }
    if (ed->cursorRow >= ed->draft.rows) ed->cursorRow = ed->draft.rows - 1;
    if (ed->cursorCol >= ed->draft.cols) ed->cursorCol = ed->draft.cols - 1;

    /* mover cursor */
    if (IsKeyPressed(KEY_UP)    && ed->cursorRow > 0)                  ed->cursorRow--;
    if (IsKeyPressed(KEY_DOWN)  && ed->cursorRow < ed->draft.rows - 1) ed->cursorRow++;
    if (IsKeyPressed(KEY_LEFT)  && ed->cursorCol > 0)                  ed->cursorCol--;
    if (IsKeyPressed(KEY_RIGHT) && ed->cursorCol < ed->draft.cols - 1) ed->cursorCol++;

    /* colocar fichas en la celda del cursor */
    if (IsKeyPressed(KEY_P)) { ed->draft.pacmanRow   = ed->cursorRow; ed->draft.pacmanCol   = ed->cursorCol; }
    if (IsKeyPressed(KEY_B)) { ed->draft.ghostRow[0] = ed->cursorRow; ed->draft.ghostCol[0] = ed->cursorCol; }
    if (IsKeyPressed(KEY_I)) { ed->draft.ghostRow[1] = ed->cursorRow; ed->draft.ghostCol[1] = ed->cursorCol; }
    if (IsKeyPressed(KEY_N)) { ed->draft.ghostRow[2] = ed->cursorRow; ed->draft.ghostCol[2] = ed->cursorCol; }
    if (IsKeyPressed(KEY_C)) { ed->draft.ghostRow[3] = ed->cursorRow; ed->draft.ghostCol[3] = ed->cursorCol; }
    if (IsKeyPressed(KEY_ONE))   { ed->draft.ballRow[0] = ed->cursorRow; ed->draft.ballCol[0] = ed->cursorCol; }
    if (IsKeyPressed(KEY_TWO))   { ed->draft.ballRow[1] = ed->cursorRow; ed->draft.ballCol[1] = ed->cursorCol; }
    if (IsKeyPressed(KEY_THREE)) { ed->draft.ballRow[2] = ed->cursorRow; ed->draft.ballCol[2] = ed->cursorCol; }
    if (IsKeyPressed(KEY_FOUR))  { ed->draft.ballRow[3] = ed->cursorRow; ed->draft.ballCol[3] = ed->cursorCol; }

    /* muros permanentes desde el cursor */
    if (IsKeyPressed(KEY_W) && ed->draft.wallCount < MAX_PERM_WALLS) {
        ed->draft.wallRow[ed->draft.wallCount] = ed->cursorRow;
        ed->draft.wallCol[ed->draft.wallCount] = ed->cursorCol;
        ed->draft.wallDir[ed->draft.wallCount] = 'H';
        ed->draft.wallCount++;
    }
    if (IsKeyPressed(KEY_V) && ed->draft.wallCount < MAX_PERM_WALLS) {
        ed->draft.wallRow[ed->draft.wallCount] = ed->cursorRow;
        ed->draft.wallCol[ed->draft.wallCount] = ed->cursorCol;
        ed->draft.wallDir[ed->draft.wallCount] = 'V';
        ed->draft.wallCount++;
    }
    if (IsKeyPressed(KEY_X) && ed->draft.wallCount > 0) {
        ed->draft.wallCount--; /* deshacer el último muro */
    }

    if (IsKeyPressed(KEY_S)) {
        memset(ed->saveName, 0, sizeof(ed->saveName));
        ed->typing = true;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gs->screen = SCREEN_MENU;
    }
}

void input_gameover(GameState *gs) {
    if (IsKeyPressed(KEY_R)) {
        game_restart(gs);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gs->screen = SCREEN_MENU;
    }
}

/* ============================================================
   9. MAIN — loop principal

   Por cada frame: leer input según la pantalla activa,
   chequear fin de partida y dibujar la pantalla.
   ============================================================ */
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Quoridor Pac-Man - LP1");
    SetTargetFPS(60);

    /* por defecto raylib cierra la ventana con ESC; nosotros
       usamos ESC para navegar, así que lo desactivamos */
    SetExitKey(KEY_NULL);

    GameState gs;
    config_setDefaults(&gs.config);
    gs.screen   = SCREEN_MENU;
    gs.wantExit = false;

    gs.mapCount = map_listAvailable(gs.availableMaps, 10);
    gs.selectedMapIdx = 0;

    while (!WindowShouldClose() && !gs.wantExit) {

        switch (gs.screen) {
            case SCREEN_MENU:       input_menu(&gs);      break;
            case SCREEN_CONFIG:     input_config(&gs);    break;
            case SCREEN_MAP_SELECT: input_mapSelect(&gs); break;
            case SCREEN_GAME:       input_game(&gs);      break;
            case SCREEN_EDITOR:     input_editor(&gs);    break;
            case SCREEN_GAMEOVER:   input_gameover(&gs);  break;
        }

        /* si la partida terminó, pasar a la pantalla de resultado */
        if (gs.screen == SCREEN_GAME && gs.gameOver) {
            gs.screen = SCREEN_GAMEOVER;
        }

        BeginDrawing();
        switch (gs.screen) {
            case SCREEN_MENU:       render_menu(&gs);      break;
            case SCREEN_CONFIG:     render_config(&gs);    break;
            case SCREEN_MAP_SELECT: render_mapSelect(&gs); break;
            case SCREEN_GAME:       render_game(&gs);      break;
            case SCREEN_EDITOR:     render_editor(&gs);    break;
            case SCREEN_GAMEOVER:   render_gameover(&gs);  break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
