#include <stdio.h>

#include "solver.h"
#include "API.h"
#include "queue.h"

unsigned int maze[MAZE_MAX_SIZE][MAZE_MAX_SIZE] = { 0 };
int distances[MAZE_MAX_SIZE][MAZE_MAX_SIZE] = { -1 };   // 1000 if it hasn't been visited yet
struct Coordinate position;
Heading heading;
int mazeWidth;
int mazeHeight;

int reached_center = 0;     // "boolean" that stores whether the mouse should start exploring more squares

// last sensor reading, relative to the mouse's current heading (updated by
// updateMaze() each step, used by solver() to build the run-output log line)
int lastFront;
int lastLeft;
int lastRight;

// set by floodFill() when it fell back to an arbitrary turn because no
// neighboring cell had a smaller distance (i.e. a dead end) — used to
// label that case "Turn-Around" instead of "Right" in the log line
int isDeadEndTurn = 0;

void initialize() {
    mazeWidth = API_mazeWidth();
    mazeHeight = API_mazeHeight();

    // setting the west/east borders, one cell per row (excludes corners)
    for (int y = 1; y < mazeHeight - 1; ++y) {
        maze[0][y] = _0001;
        maze[mazeWidth - 1][y] = _0100;
    }
    // setting the south/north borders, one cell per column (excludes corners)
    for (int x = 1; x < mazeWidth - 1; ++x) {
        maze[x][0] = _0010;
        maze[x][mazeHeight - 1] = _1000;
    }
    maze[0][0] = _0011;
    maze[0][mazeHeight - 1] = _1001;
    maze[mazeWidth - 1][0] = _0110;
    maze[mazeWidth - 1][mazeHeight - 1] = _1100;

    // setting initial distances
    resetDistances();

    // setting mouse position + heading
    position.x = 0;
    position.y = 0;
    heading = NORTH;
}

/*
Updates the maze's walls based on what the mouse can currently see
*/
void updateMaze() {
    int x = position.x;
    int y = position.y;
    // start by assuming there are no walls, this variable will be changed based on which walls you see
    unsigned int walls = _0000;

    // read each sensor exactly once per step; stored in globals so solver()
    // can include them in the run-output log line after deciding an action
    int front = lastFront = API_wallFront();
    int left = lastLeft = API_wallLeft();
    int right = lastRight = API_wallRight();

    switch (heading) {
        case NORTH:
            if (front) {
                walls |= _1000; // stores the wall to the north in walls (to be updated at the end of switch statement)
                // updating neighboring squares as well (if there is one):
                if (y + 1 != mazeHeight)
                    maze[x][y + 1] |= _0010;
            }
            if (left) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            if (right) {
                walls |= _0100;
                if (x + 1 != mazeWidth)
                    maze[x + 1][y] |= _0001;
            }
            break;
        case EAST:
            if (front) {
                walls |= _0100;
                if (x + 1 != mazeWidth)
                    maze[x + 1][y] |= _0001;
            }
            if (left) {
                walls |= _1000;
                if (y + 1 != mazeHeight)
                    maze[x][y + 1] |= _0010;
            }
            if (right) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            break;
        case SOUTH:
            if (front) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            if (left) {
                walls |= _0100;
                if (x + 1 != mazeWidth)
                    maze[x + 1][y] |= _0001;
            }
            if (right) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            break;
        case WEST:
            if (front) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            if (left) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            if (right) {
                walls |= _1000;
                if (y + 1 != mazeHeight)
                    maze[x][y + 1] |= _0010;
            }
            break;
    }

    maze[x][y] |= walls;

    // REMOVE LATER
    //debug_int(maze[x][y]);
    unsigned int north = _1000;
    if (maze[x][y] >= 8) {
        API_setWall(x, y, 'n');
        //debug_log("There's a wall to the north");
    }
    if (maze[x][y] % 8 >= 4) {
        API_setWall(x, y, 'e');
        //debug_log("There's a wall to the east");
    }
    if (maze[x][y] % 4 >= 2) {
        API_setWall(x, y, 's');
        //debug_log("There's a wall to the south");
    }
    if (maze[x][y] % 2 == 1) {
        API_setWall(x, y, 'w');
        //debug_log("There's a wall to the west");
    }
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

void resetDistances() {
    // initially sets all the distances to -1 (invalid distance)
    for (int x = 0; x < mazeWidth; ++x) {
        for (int y = 0; y < mazeHeight; ++y) {
            distances[x][y] = -1;
        }
    }

    // if you haven't reached the center, set the goal to be the center
    // (same rule the simulator itself uses: 1 cell if both dimensions are
    // odd, 2 cells if exactly one is even, 4 cells if both are even)
    if (!reached_center) {
        int ax = (mazeWidth - 1) / 2, ay = (mazeHeight - 1) / 2;
        int bx = mazeWidth / 2,       by = (mazeHeight - 1) / 2;
        int cx = (mazeWidth - 1) / 2, cy = mazeHeight / 2;
        int dx = mazeWidth / 2,       dy = mazeHeight / 2;

        distances[ax][ay] = 0;
        if (mazeWidth % 2 == 0) {
            distances[bx][by] = 0;
        }
        if (mazeHeight % 2 == 0) {
            distances[cx][cy] = 0;
        }
        if (mazeWidth % 2 == 0 && mazeHeight % 2 == 0) {
            distances[dx][dy] = 0;
        }
    }
    else {
        distances[0][0] = 0;    // go back to the start
    }
}

int isWallInDirection(int x, int y, Heading direction) {
    switch (direction) {
        case NORTH:
            if (maze[x][y] >= 8)
                return 1;
            break;
        case EAST:
            if (maze[x][y] % 8 >= 4)
                return 1;
            break;
        case SOUTH:
            if (maze[x][y] % 4 >= 2)
                return 1;
            break;
        case WEST:
            if (maze[x][y] % 2 == 1) 
                return 1;
            break;
    }
    return 0;
}

void updateDistances() {
    resetDistances();
    queue squares = queue_create();

    // adds the goal squares to the queue (the middle of the maze or the starting position depending on if you've reached the center)
    for (int x = 0; x < mazeWidth; ++x) {
        for (int y = 0; y < mazeHeight; ++y) {
            if (distances[x][y] == 0)
                queue_push(squares, xyToSquare(x, y));
        }
    }

    while (!queue_is_empty(squares)) {
        struct Coordinate square = squareToCoord(queue_pop(squares));
        int x = square.x;
        int y = square.y;

        // if there's no wall to the north && the square to the north hasn't been checked yet
        if (isWallInDirection(x, y, NORTH) == 0 && distances[x][y + 1] == -1) {
            distances[x][y + 1] = distances[x][y] + 1;
            queue_push(squares, xyToSquare(x, y + 1));
        }
        // same as ^ but for east
        if (isWallInDirection(x, y, EAST) == 0 && distances[x + 1][y] == -1) {
            distances[x + 1][y] = distances[x][y] + 1;
            queue_push(squares, xyToSquare(x + 1, y));
        }
        // same as ^ but for south
        if (isWallInDirection(x, y, SOUTH) == 0 && distances[x][y - 1] == -1) {
            distances[x][y - 1] = distances[x][y] + 1;
            queue_push(squares, xyToSquare(x, y - 1));
        }
        // same as ^ but for west
        if (isWallInDirection(x, y, WEST) == 0 && distances[x - 1][y] == -1) {
            distances[x - 1][y] = distances[x][y] + 1;
            queue_push(squares, xyToSquare(x - 1, y));
        }
    }
}

void updateHeading(Action nextAction) {
    if (nextAction == FORWARD || nextAction == IDLE) {
        return;
    }
    else if (nextAction == LEFT) {
        switch (heading) {
            case NORTH:
                heading = WEST;
                break;
            case EAST:
                heading = NORTH;
                break;
            case SOUTH:
                heading = EAST;
                break;
            case WEST:
                heading = SOUTH;
                break;
            default:
                break;
        }
    }
    else if (nextAction == RIGHT) {
        switch (heading) {
            case NORTH:
                heading = EAST;
                break;
            case EAST:
                heading = SOUTH;
                break;
            case SOUTH:
                heading = WEST;
                break;
            case WEST:
                heading = NORTH;
                break;
            default:
                break;
        }
    }
}

void updatePosition(Action nextAction) {
    if (nextAction != FORWARD) {
        return;
    }

    switch (heading) {
        case NORTH:
            position.y += 1;
            break;
        case SOUTH:
            position.y -= 1;
            break;
        case EAST:
            position.x += 1;
            break;
        case WEST:
            position.x -= 1;
            break;
        default:
            break;
    }
}

/*
Colors the cells along the currently-known shortest path from the mouse's
position to the goal (greedily follows decreasing distances[][]), so the
simulator shows the planned route, not just the raw distance numbers.
*/
void showPath() {
    API_clearAllColor();

    int x = position.x;
    int y = position.y;
    int steps = 0;

    while (distances[x][y] != 0 && steps < mazeWidth * mazeHeight) {
        API_setColor(x, y, 'Y');

        int best_distance = distances[x][y];
        int next_x = x;
        int next_y = y;

        if (!isWallInDirection(x, y, NORTH) && y + 1 < mazeHeight &&
            distances[x][y + 1] >= 0 && distances[x][y + 1] < best_distance) {
            best_distance = distances[x][y + 1];
            next_x = x;
            next_y = y + 1;
        }
        if (!isWallInDirection(x, y, EAST) && x + 1 < mazeWidth &&
            distances[x + 1][y] >= 0 && distances[x + 1][y] < best_distance) {
            best_distance = distances[x + 1][y];
            next_x = x + 1;
            next_y = y;
        }
        if (!isWallInDirection(x, y, SOUTH) && y - 1 >= 0 &&
            distances[x][y - 1] >= 0 && distances[x][y - 1] < best_distance) {
            best_distance = distances[x][y - 1];
            next_x = x;
            next_y = y - 1;
        }
        if (!isWallInDirection(x, y, WEST) && x - 1 >= 0 &&
            distances[x - 1][y] >= 0 && distances[x - 1][y] < best_distance) {
            best_distance = distances[x - 1][y];
            next_x = x - 1;
            next_y = y;
        }

        if (next_x == x && next_y == y) {
            break;  // no improving neighbor found (shouldn't normally happen)
        }
        x = next_x;
        y = next_y;
        steps++;
    }

    API_setColor(x, y, 'Y');  // color the goal cell too
}

Action solver() {
    int goalChanged = 0;

    // if you reached the center, go back to the start
    if (!reached_center && distances[position.x][position.y] == 0) {
        reached_center = 1;
        goalChanged = 1;
    }
    // if you went to the center & all the way back to the start, restart
    else if (reached_center && distances[position.x][position.y] == 0) {
        reached_center = 0;
        goalChanged = 1;
    }

    // distance-to-goal as known BEFORE this step's sensor reading, so we
    // can tell whether a newly-discovered wall just blocked the planned
    // route (distance getting worse) versus the goal itself just changing
    int distanceBefore = distances[position.x][position.y];

    updateMaze();
    updateDistances();
    showPath();

    char rerouteField[24];
    if (!goalChanged && distanceBefore >= 0 &&
        distances[position.x][position.y] > distanceBefore) {
        snprintf(rerouteField, sizeof(rerouteField), "%d->%d",
                 distanceBefore, distances[position.x][position.y]);
    } else {
        rerouteField[0] = '\0';
    }

    Action action = floodFill();

    updateHeading(action);
    updatePosition(action);

    const char *actionLabel;
    switch (action) {
        case FORWARD: actionLabel = "Forward"; break;
        case LEFT:     actionLabel = "Left"; break;
        case RIGHT:    actionLabel = isDeadEndTurn ? "Turn-Around" : "Right"; break;
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

// Put your implementation of floodfill here!
Action floodFill() {
    unsigned int least_distance = 300;   // just some large number, none of the distances will be over 300
    Action optimal_move = IDLE;
    isDeadEndTurn = 0;

    /*
    Basic Idea:
    - Look at the square in front of you, to the left, and to the right if there are no walls
    - Find the square with the lowest distance to the goal
    - Move in the direction of that square (move forward if it's the forward square, turn in the correct direction otherwise)
    */

    if (heading == NORTH) {
        if (!isWallInDirection(position.x, position.y, NORTH) && distances[position.x][position.y + 1] < least_distance) {
            least_distance = distances[position.x][position.y + 1];
            optimal_move = FORWARD;
        }
        if (!isWallInDirection(position.x, position.y, EAST) && distances[position.x + 1][position.y] < least_distance) {
            least_distance = distances[position.x + 1][position.y];
            optimal_move = RIGHT;
        }
        if (!isWallInDirection(position.x, position.y, WEST) && distances[position.x - 1][position.y] < least_distance) {
            least_distance = distances[position.x - 1][position.y];
            optimal_move = LEFT;
        }
    }
    else if (heading == EAST) {
        if (!isWallInDirection(position.x, position.y, EAST) && distances[position.x + 1][position.y] < least_distance) {
            least_distance = distances[position.x + 1][position.y];
            optimal_move = FORWARD;
        }
        if (!isWallInDirection(position.x, position.y, SOUTH) && distances[position.x][position.y - 1] < least_distance) {
            least_distance = distances[position.x][position.y - 1];
            optimal_move = RIGHT;
        }
        if (!isWallInDirection(position.x, position.y, NORTH) && distances[position.x][position.y + 1] < least_distance) {
            least_distance = distances[position.x][position.y + 1];
            optimal_move = LEFT;
        }
    }
    else if (heading == SOUTH) {
        if (!isWallInDirection(position.x, position.y, SOUTH) && distances[position.x][position.y - 1] < least_distance) {
            least_distance = distances[position.x][position.y - 1];
            optimal_move = FORWARD;
        }
        if (!isWallInDirection(position.x, position.y, WEST) && distances[position.x - 1][position.y] < least_distance) {
            least_distance = distances[position.x - 1][position.y];
            optimal_move = RIGHT;
        }
        if (!isWallInDirection(position.x, position.y, EAST) && distances[position.x + 1][position.y] < least_distance) {
            least_distance = distances[position.x + 1][position.y];
            optimal_move = LEFT;
        }
    }
    else if (heading == WEST) {
        if (!isWallInDirection(position.x, position.y, WEST) && distances[position.x - 1][position.y] < least_distance) {
            least_distance = distances[position.x - 1][position.y];
            optimal_move = FORWARD;
        }
        if (!isWallInDirection(position.x, position.y, NORTH) && distances[position.x][position.y + 1] < least_distance) {
            least_distance = distances[position.x][position.y + 1];
            optimal_move = RIGHT;
        }
        if (!isWallInDirection(position.x, position.y, SOUTH) && distances[position.x][position.y - 1] < least_distance) {
            least_distance = distances[position.x][position.y - 1];
            optimal_move = LEFT;
        }
    }

    // handles dead ends (when there's no walls in front, to the right or to the left)
    if (least_distance == 300) {
        optimal_move = RIGHT;   // arbitrary, can be any turn
        isDeadEndTurn = 1;
    }
    
    return optimal_move;
}

// This is an example of a simple left wall following algorithm.
Action leftWallFollower() {
    if(API_wallFront()) {
        if(API_wallLeft()){
            return RIGHT;
        }
        return LEFT;
    }
    return FORWARD;
}