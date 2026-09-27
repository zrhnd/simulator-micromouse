#include <stdio.h>
#include <stdlib.h>

#include "API.h"

/* ===========================================================================
   TYPES & CONSTANTS
   =========================================================================== */

typedef enum Heading {NORTH, EAST, SOUTH, WEST} Heading;
typedef enum Action {LEFT, FORWARD, RIGHT, IDLE} Action;

// Array capacity (not the actual maze size). Covers the largest maze we
// expect to load (halfsize competitions are 32x32); actual dimensions are
// read at runtime from the simulator via API_mazeWidth()/API_mazeHeight().
#define MAZE_MAX_SIZE 32

/*
Sets the different types of cells (the walls around a given cell) in the format:
    _TopWall RightWall BottomWall LeftWall
    1: there is a wall
    0: there isn't a wall
*/
#define _0000 0
#define _0001 1
#define _0010 2
#define _0011 3
#define _0100 4
#define _0101 5
#define _0110 6
#define _0111 7
#define _1000 8
#define _1001 9
#define _1010 10
#define _1011 11
#define _1100 12
#define _1101 13
#define _1110 14
#define _1111 15    // not actually possible in a maze

// sentinel: no real g-cost/f-cost in this maze ever gets close to this
#define INF 100000

struct Coordinate {
    int x;
    int y;
};

/* ===========================================================================
   GLOBAL STATE
   =========================================================================== */

unsigned int maze[MAZE_MAX_SIZE][MAZE_MAX_SIZE] = { 0 };
int mazeWidth;
int mazeHeight;

// A* working arrays (re-initialized at the start of every aStarSearch() call)
int gCost[MAZE_MAX_SIZE][MAZE_MAX_SIZE];    // known cost from current position to this cell
int cameFrom[MAZE_MAX_SIZE][MAZE_MAX_SIZE]; // predecessor cell, packed via xyToSquare(); -1 = none
int closedSet[MAZE_MAX_SIZE][MAZE_MAX_SIZE];
int inOpenSet[MAZE_MAX_SIZE][MAZE_MAX_SIZE];

struct Coordinate position;
Heading heading;

int reached_center = 0;   // same meaning as in flood-fill-c: which target (center vs start) is active

// last sensor reading, relative to the mouse's current heading (for the
// run-output log line, same convention as flood-fill-c)
int lastFront;
int lastLeft;
int lastRight;

// set by aStarSearch() when the first step of the found path points
// directly behind the mouse (needs two turns, not one) -- used to label
// that case "Turn-Around" in the log line, same convention as flood-fill-c
int isTurnAround = 0;

int lastPathCost = -1;   // total path cost A* found on the PREVIOUS step, for the REROUTE log field

/* ===========================================================================
   SETUP & SENSING
   =========================================================================== */

void initialize() {
    mazeWidth = API_mazeWidth();
    mazeHeight = API_mazeHeight();

    // setting the west/east borders, one cell per row (excludes corners) --
    // same reasoning as flood-fill-c: the outer wall is guaranteed by the
    // competition rules, so it's known before any sensing happens
    for (int y = 1; y < mazeHeight - 1; ++y) {
        maze[0][y] = _0001;
        maze[mazeWidth - 1][y] = _0100;
    }
    for (int x = 1; x < mazeWidth - 1; ++x) {
        maze[x][0] = _0010;
        maze[x][mazeHeight - 1] = _1000;
    }
    maze[0][0] = _0011;
    maze[0][mazeHeight - 1] = _1001;
    maze[mazeWidth - 1][0] = _0110;
    maze[mazeWidth - 1][mazeHeight - 1] = _1100;

    position.x = 0;
    position.y = 0;
    heading = NORTH;
}

/*
Updates the maze's walls based on what the mouse can currently see.
Identical sensing/bit-encoding logic to flood-fill-c -- the wall model and
the sensor API don't change between algorithms, only how the resulting
maze[][] is used to plan a route.
*/
void updateMaze() {
    int x = position.x;
    int y = position.y;
    unsigned int walls = _0000;

    int front = lastFront = API_wallFront();
    int left = lastLeft = API_wallLeft();
    int right = lastRight = API_wallRight();

    switch (heading) {
        case NORTH:
            if (front) {
                walls |= _1000;
                if (y + 1 != mazeHeight) maze[x][y + 1] |= _0010;
            }
            if (left) {
                walls |= _0001;
                if (x - 1 >= 0) maze[x - 1][y] |= _0100;
            }
            if (right) {
                walls |= _0100;
                if (x + 1 != mazeWidth) maze[x + 1][y] |= _0001;
            }
            break;
        case EAST:
            if (front) {
                walls |= _0100;
                if (x + 1 != mazeWidth) maze[x + 1][y] |= _0001;
            }
            if (left) {
                walls |= _1000;
                if (y + 1 != mazeHeight) maze[x][y + 1] |= _0010;
            }
            if (right) {
                walls |= _0010;
                if (y - 1 >= 0) maze[x][y - 1] |= _1000;
            }
            break;
        case SOUTH:
            if (front) {
                walls |= _0010;
                if (y - 1 >= 0) maze[x][y - 1] |= _1000;
            }
            if (left) {
                walls |= _0100;
                if (x + 1 != mazeWidth) maze[x + 1][y] |= _0001;
            }
            if (right) {
                walls |= _0001;
                if (x - 1 >= 0) maze[x - 1][y] |= _0100;
            }
            break;
        case WEST:
            if (front) {
                walls |= _0001;
                if (x - 1 >= 0) maze[x - 1][y] |= _0100;
            }
            if (left) {
                walls |= _0010;
                if (y - 1 >= 0) maze[x][y - 1] |= _1000;
            }
            if (right) {
                walls |= _1000;
                if (y + 1 != mazeHeight) maze[x][y + 1] |= _0010;
            }
            break;
    }

    maze[x][y] |= walls;

    if (maze[x][y] >= 8) API_setWall(x, y, 'n');
    if (maze[x][y] % 8 >= 4) API_setWall(x, y, 'e');
    if (maze[x][y] % 4 >= 2) API_setWall(x, y, 's');
    if (maze[x][y] % 2 == 1) API_setWall(x, y, 'w');
}

int xyToSquare(int x, int y) {
    return x + mazeWidth * y;
}

struct Coordinate squareToCoord(int square) {
    struct Coordinate coord;
    coord.x = square % mazeWidth;
    coord.y = square / mazeWidth;
    return coord;
}

int isWallInDirection(int x, int y, Heading direction) {
    switch (direction) {
        case NORTH: if (maze[x][y] >= 8) return 1; break;
        case EAST:  if (maze[x][y] % 8 >= 4) return 1; break;
        case SOUTH: if (maze[x][y] % 4 >= 2) return 1; break;
        case WEST:  if (maze[x][y] % 2 == 1) return 1; break;
    }
    return 0;
}

/* ===========================================================================
   GOAL / HEURISTIC
   =========================================================================== */

/*
Same goal-cell rule as flood-fill-c (mirrors mms's own
Maze::getCenterPositions()): 1 cell if both dimensions are odd, 2 cells if
exactly one is even, 4 cells if both are even. Fills `goals` and returns
how many cells were written (1, 2, or 4).
*/
static int getGoalCells(struct Coordinate goals[4]) {
    if (reached_center) {
        goals[0].x = 0;
        goals[0].y = 0;
        return 1;
    }

    int ax = (mazeWidth - 1) / 2, ay = (mazeHeight - 1) / 2;
    int bx = mazeWidth / 2,       by = (mazeHeight - 1) / 2;
    int cx = (mazeWidth - 1) / 2, cy = mazeHeight / 2;
    int dx = mazeWidth / 2,       dy = mazeHeight / 2;

    int n = 0;
    goals[n].x = ax; goals[n].y = ay; n++;
    if (mazeWidth % 2 == 0) { goals[n].x = bx; goals[n].y = by; n++; }
    if (mazeHeight % 2 == 0) { goals[n].x = cx; goals[n].y = cy; n++; }
    if (mazeWidth % 2 == 0 && mazeHeight % 2 == 0) { goals[n].x = dx; goals[n].y = dy; n++; }
    return n;
}

int isGoalCell(int x, int y) {
    struct Coordinate goals[4];
    int n = getGoalCells(goals);
    for (int i = 0; i < n; ++i) {
        if (goals[i].x == x && goals[i].y == y) return 1;
    }
    return 0;
}

/*
Admissible heuristic for A*: Manhattan distance to the NEAREST goal cell,
ignoring walls. Since walls can only make the true distance longer (never
shorter) than a straight-line grid distance, this never overestimates --
that's what keeps A* guaranteed-optimal here, same as plain BFS/flood fill
(which is equivalent to using a heuristic of 0 everywhere).
*/
int heuristic(int x, int y) {
    struct Coordinate goals[4];
    int n = getGoalCells(goals);
    int best = INF;
    for (int i = 0; i < n; ++i) {
        int dx = x - goals[i].x; if (dx < 0) dx = -dx;
        int dy = y - goals[i].y; if (dy < 0) dy = -dy;
        int d = dx + dy;
        if (d < best) best = d;
    }
    return best;
}

/* ===========================================================================
   HEADING / MOVEMENT BOOKKEEPING
   =========================================================================== */

Heading headingLeftOf(Heading h) {
    switch (h) {
        case NORTH: return WEST;
        case WEST:  return SOUTH;
        case SOUTH: return EAST;
        case EAST:  return NORTH;
    }
    return h;
}

Heading headingRightOf(Heading h) {
    switch (h) {
        case NORTH: return EAST;
        case EAST:  return SOUTH;
        case SOUTH: return WEST;
        case WEST:  return NORTH;
    }
    return h;
}

Heading headingOppositeOf(Heading h) {
    switch (h) {
        case NORTH: return SOUTH;
        case SOUTH: return NORTH;
        case EAST:  return WEST;
        case WEST:  return EAST;
    }
    return h;
}

void updateHeading(Action nextAction) {
    if (nextAction == LEFT) heading = headingLeftOf(heading);
    else if (nextAction == RIGHT) heading = headingRightOf(heading);
    // FORWARD/IDLE: heading unchanged
}

void updatePosition(Action nextAction) {
    if (nextAction != FORWARD) return;
    switch (heading) {
        case NORTH: position.y += 1; break;
        case SOUTH: position.y -= 1; break;
        case EAST:  position.x += 1; break;
        case WEST:  position.x -= 1; break;
    }
}

/* ===========================================================================
   A* SEARCH  (this is the actual algorithm)
   =========================================================================== */

/*
Unlike flood-fill-c's BFS (which fills in a distance-to-goal value for
EVERY reachable cell), A* searches specifically FROM the mouse's current
position TOWARD the nearest goal cell, using heuristic(x,y) to prioritize
expanding cells that look closer to the goal first. It still only ever
settles a cell once it's certain that cell's g-cost (distance from the
current position) is optimal -- same guarantee BFS gives -- but typically
visits far fewer cells to get there, since the heuristic actively steers
the search instead of expanding uniformly in every direction.

The open set here is just two parallel boolean/int arrays scanned linearly
each iteration (no binary heap) -- for a maze this size (<=1024 cells) a
full O(n) scan per pop is still instant, and it keeps the code simple and
easy to follow, matching the style of the rest of this project.
*/
Action aStarSearch(int *outGoalX, int *outGoalY, int *outPathCost) {
    for (int x = 0; x < mazeWidth; ++x) {
        for (int y = 0; y < mazeHeight; ++y) {
            gCost[x][y] = INF;
            cameFrom[x][y] = -1;
            closedSet[x][y] = 0;
            inOpenSet[x][y] = 0;
        }
    }

    int sx = position.x, sy = position.y;
    gCost[sx][sy] = 0;
    inOpenSet[sx][sy] = 1;

    int goalX = -1, goalY = -1;

    while (1) {
        // find the open cell with the lowest f = g + h (linear scan)
        int bestF = INF + INF;
        int ux = -1, uy = -1;
        for (int x = 0; x < mazeWidth; ++x) {
            for (int y = 0; y < mazeHeight; ++y) {
                if (inOpenSet[x][y] && !closedSet[x][y]) {
                    int f = gCost[x][y] + heuristic(x, y);
                    if (f < bestF) { bestF = f; ux = x; uy = y; }
                }
            }
        }

        if (ux == -1) {
            // open set exhausted without ever reaching a goal cell: the
            // mouse is fully boxed in by currently-known walls (a true
            // dead end, not just "3 sides closed" -- same rare edge case
            // flood-fill-c guards against with its own dead-end fallback)
            *outGoalX = sx; *outGoalY = sy; *outPathCost = 0;
            isTurnAround = 1;
            return RIGHT;
        }

        if (isGoalCell(ux, uy)) { goalX = ux; goalY = uy; break; }

        closedSet[ux][uy] = 1;
        inOpenSet[ux][uy] = 0;

        static const Heading DIRS[4] = {NORTH, EAST, SOUTH, WEST};
        for (int d = 0; d < 4; ++d) {
            if (isWallInDirection(ux, uy, DIRS[d])) continue;
            int vx = ux, vy = uy;
            switch (DIRS[d]) {
                case NORTH: vy += 1; break;
                case SOUTH: vy -= 1; break;
                case EAST:  vx += 1; break;
                case WEST:  vx -= 1; break;
            }
            if (vx < 0 || vx >= mazeWidth || vy < 0 || vy >= mazeHeight) continue;
            if (closedSet[vx][vy]) continue;

            int tentativeG = gCost[ux][uy] + 1;
            if (tentativeG < gCost[vx][vy]) {
                gCost[vx][vy] = tentativeG;
                cameFrom[vx][vy] = xyToSquare(ux, uy);
                inOpenSet[vx][vy] = 1;
            }
        }
    }

    *outGoalX = goalX;
    *outGoalY = goalY;
    *outPathCost = gCost[goalX][goalY];

    // walk the path back from the goal to the start, keeping the last
    // cell visited before we land exactly on the start -- that's the
    // first step of the forward path
    int cx = goalX, cy = goalY;
    int prevX = cx, prevY = cy;
    while (!(cx == sx && cy == sy)) {
        prevX = cx;
        prevY = cy;
        struct Coordinate p = squareToCoord(cameFrom[cx][cy]);
        cx = p.x;
        cy = p.y;
    }

    isTurnAround = 0;

    if (prevX == sx && prevY == sy) {
        // goal cell IS the start cell (shouldn't normally happen -- solver()
        // flips the goal before this runs whenever that's about to occur)
        return IDLE;
    }

    Heading desired;
    if (prevY > sy) desired = NORTH;
    else if (prevY < sy) desired = SOUTH;
    else if (prevX > sx) desired = EAST;
    else desired = WEST;

    if (desired == heading) return FORWARD;
    if (desired == headingLeftOf(heading)) return LEFT;
    if (desired == headingRightOf(heading)) return RIGHT;

    // desired == headingOppositeOf(heading): the path's first step is
    // directly behind the mouse. One action can only turn 90 degrees, so
    // turn (arbitrarily right) and let next step's fresh search continue
    // from the new heading -- same convention as flood-fill-c's dead-end
    // fallback, just triggered by a different situation (backtracking a
    // real path, not being boxed in).
    isTurnAround = 1;
    return RIGHT;
}

/*
Colors the path A* just found, from the mouse's position to the goal cell
it settled on -- same visual role as flood-fill-c's showPath(), just fed
from A*'s cameFrom[][] chain instead of flood fill's distances[][] field.
*/
void showPath(int goalX, int goalY) {
    API_clearAllColor();
    int x = goalX, y = goalY;
    while (1) {
        API_setColor(x, y, 'Y');
        if (x == position.x && y == position.y) break;
        struct Coordinate p = squareToCoord(cameFrom[x][y]);
        x = p.x;
        y = p.y;
    }
}

/* ===========================================================================
   PER-STEP CONDUCTOR
   =========================================================================== */

Action solver() {
    int goalChanged = 0;

    if (!reached_center && isGoalCell(position.x, position.y)) {
        reached_center = 1;
        goalChanged = 1;
    } else if (reached_center && position.x == 0 && position.y == 0) {
        reached_center = 0;
        goalChanged = 1;
    }

    updateMaze();

    int goalX, goalY, pathCost;
    Action action = aStarSearch(&goalX, &goalY, &pathCost);
    showPath(goalX, goalY);

    char rerouteField[24];
    if (!goalChanged && lastPathCost >= 0 && pathCost > lastPathCost - 1) {
        snprintf(rerouteField, sizeof(rerouteField), "%d->%d", lastPathCost - 1, pathCost);
    } else {
        rerouteField[0] = '\0';
    }
    lastPathCost = pathCost;

    updateHeading(action);
    updatePosition(action);

    const char *actionLabel;
    switch (action) {
        case FORWARD: actionLabel = "Forward"; break;
        case LEFT:     actionLabel = "Left"; break;
        case RIGHT:    actionLabel = isTurnAround ? "Turn-Around" : "Right"; break;
        default:       actionLabel = "Idle"; break;
    }

    char logLine[128];
    snprintf(logLine, sizeof(logLine),
             "F:%s R:%s L:%s\tACTION: %s\tMOVE-TO: [%d,%d]\tREROUTE: %s",
             lastFront ? "wall" : "open",
             lastRight ? "wall" : "open",
             lastLeft ? "wall" : "open",
             actionLabel, position.x, position.y, rerouteField);
    debug_log(logLine);

    return action;
}

/* ===========================================================================
   ENTRY POINT
   =========================================================================== */

// You do not need to edit this part. It just runs solver() every step and
// passes the resulting action to the simulator.
int main(int argc, char* argv[]) {
    debug_log("Running...");
    initialize();
    while (1) {
        Action nextMove = solver();

        // displays each cell's g-cost from the mouse's CURRENT position
        // (A* has no single goal-anchored distance field the way
        // flood-fill-c's distances[][] does -- gCost[][] is recomputed
        // fresh, anchored at wherever the mouse currently stands, every
        // single step)
        for (int x = 0; x < mazeWidth; ++x) {
            for (int y = 0; y < mazeHeight; ++y) {
                if (gCost[x][y] >= INF) {
                    API_clearText(x, y);
                } else {
                    API_setText(x, y, gCost[x][y]);
                }
            }
        }

        switch (nextMove) {
            case FORWARD:
                API_moveForward();
                break;
            case LEFT:
                API_turnLeft();
                break;
            case RIGHT:
                API_turnRight();
                break;
            case IDLE:
                break;
        }
    }
}
