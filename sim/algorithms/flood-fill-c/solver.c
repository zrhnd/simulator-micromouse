#include "solver.h"
#include "API.h"
#include "queue.h"

unsigned int maze[MAZE_SIZE][MAZE_SIZE] = { 0 };
int distances[MAZE_SIZE][MAZE_SIZE] = { -1 };   // 1000 if it hasn't been visited yet
struct Coordinate position;
Heading heading;

int reached_center = 0;     // "boolean" that stores whether the mouse should start exploring more squares

void initialize() {
    // setting the borders
    for (int i = 1; i < MAZE_SIZE - 1; ++i) {
        maze[0][i] = _0001;
        maze[i][0] = _0010;
        maze[i][MAZE_SIZE - 1] = _1000;
        maze[MAZE_SIZE - 1][i] = _0100;
    }
    maze[0][0] = _0011;
    maze[0][MAZE_SIZE - 1] = _1001;
    maze[MAZE_SIZE - 1][0] = _0110;
    maze[MAZE_SIZE - 1][MAZE_SIZE - 1] = _1100;

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

    switch (heading) {
        case NORTH:
            if (API_wallFront()) {
                walls |= _1000; // stores the wall to the north in walls (to be updated at the end of switch statement)
                // updating neighboring squares as well (if there is one):
                if (y + 1 != MAZE_SIZE)
                    maze[x][y + 1] |= _0010;
            }
            if (API_wallLeft()) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            if (API_wallRight()) {
                walls |= _0100;
                if (x + 1 != MAZE_SIZE)
                    maze[x + 1][y] |= _0001;
            }
            break;
        case EAST:
            if (API_wallFront()) {
                walls |= _0100;
                if (x + 1 != MAZE_SIZE)
                    maze[x + 1][y] |= _0001;
            }
            if (API_wallLeft()) {
                walls |= _1000;
                if (y + 1 != MAZE_SIZE)
                    maze[x][y + 1] |= _0010;
            }
            if (API_wallRight()) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            break;
        case SOUTH:
            if (API_wallFront()) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            if (API_wallLeft()) {
                walls |= _0100;
                if (x + 1 != MAZE_SIZE)
                    maze[x + 1][y] |= _0001;
            }
            if (API_wallRight()) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            break;
        case WEST:
            if (API_wallFront()) {
                walls |= _0001;
                if (x - 1 >= 0)
                    maze[x - 1][y] |= _0100;
            }
            if (API_wallLeft()) {
                walls |= _0010;
                if (y - 1 >= 0)
                    maze[x][y - 1] |= _1000;
            }
            if (API_wallRight()) {
                walls |= _1000;
                if (y + 1 != MAZE_SIZE)
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
    return x + MAZE_SIZE * y;
}

struct Coordinate squareToCoord(int square) {
    struct Coordinate coord;
    coord.x = square % MAZE_SIZE;
    coord.y = square / MAZE_SIZE;
    return coord;
}

void resetDistances() {
    // initially sets all the distances to -1 (invalid distance)
    for (int x = 0; x < MAZE_SIZE; ++x) {
        for (int y = 0; y < MAZE_SIZE; ++y) {
            distances[x][y] = -1;
        }
    }

    // if you haven't reached the center, set the goal to be the center
    if (!reached_center) {
        // sets goal distances
        if (MAZE_SIZE % 2 == 0) {
            distances[MAZE_SIZE/2][MAZE_SIZE/2] = 0; 
            distances[MAZE_SIZE/2 - 1][MAZE_SIZE/2] = 0; 
            distances[MAZE_SIZE/2][MAZE_SIZE/2 - 1] = 0; 
            distances[MAZE_SIZE/2 - 1][MAZE_SIZE/2 - 1] = 0; 
        }
        else {
            distances[MAZE_SIZE/2][MAZE_SIZE/2] = 0; 
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
    for (int x = 0; x < MAZE_SIZE; ++x) {
        for (int y = 0; y < MAZE_SIZE; ++y) {
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

    while (distances[x][y] != 0 && steps < MAZE_SIZE * MAZE_SIZE) {
        API_setColor(x, y, 'Y');

        int best_distance = distances[x][y];
        int next_x = x;
        int next_y = y;

        if (!isWallInDirection(x, y, NORTH) && y + 1 < MAZE_SIZE &&
            distances[x][y + 1] >= 0 && distances[x][y + 1] < best_distance) {
            best_distance = distances[x][y + 1];
            next_x = x;
            next_y = y + 1;
        }
        if (!isWallInDirection(x, y, EAST) && x + 1 < MAZE_SIZE &&
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
    // if you reached the center, go back to the start
    if (!reached_center && distances[position.x][position.y] == 0) {
        reached_center = 1;
    }
    // if you went to the center & all the way back to the start, restart
    else if (reached_center && distances[position.x][position.y] == 0) {
        reached_center = 0;
    }

    updateMaze();
    updateDistances();
    showPath();

    Action action = floodFill();

    updateHeading(action);
    updatePosition(action);
    debug_coord(position.x, position.y);
    return action;
}

// Put your implementation of floodfill here!
Action floodFill() {
    unsigned int least_distance = 300;   // just some large number, none of the distances will be over 300
    Action optimal_move = IDLE;

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
    if (least_distance == 300)
        optimal_move = RIGHT;   // arbitrary, can be any turn
    
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