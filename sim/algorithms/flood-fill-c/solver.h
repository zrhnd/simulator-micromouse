#ifndef SOLVER_H
#define SOLVER_H

typedef enum Heading {NORTH, EAST, SOUTH, WEST} Heading;
typedef enum Action {LEFT, FORWARD, RIGHT, IDLE} Action;

/* MAZE CONSTANTS */
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

extern unsigned int maze[MAZE_MAX_SIZE][MAZE_MAX_SIZE];
extern int distances[MAZE_MAX_SIZE][MAZE_MAX_SIZE];
extern int mazeWidth;
extern int mazeHeight;

struct Coordinate {
    int x;
    int y;
};

void initialize();
void updateMaze();      // updates the maze array with the walls around the mouse's current position
void updateDistances();         // the "floodfill" algorithm
void resetDistances();
void showPath();                 // colors the currently-known shortest path to the goal
int xyToSquare(int x, int y);
struct Coordinate squareToCoord(int square);
int isWallInDirection(int x, int y, Heading direction);
void updateHeading(Action nextAction);
void updatePosition(Action nextAction);
Action solver();
Action leftWallFollower();
Action floodFill();

#endif