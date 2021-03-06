#include <stdio.h>
#include <stdlib.h>     /* malloc, srand, rand */
#include <math.h>       /* islessequal */
#include <time.h>       /* time */

#include "Backend.h"
#include "Mode.h"
#include "Robots.h"

static void* createFloor(struct Floor*, int width, int height, int robots_amount);			// this funtion returns NULL in case it fails to allocate the memory segment
static void destroyFloor(struct Floor*);
static int cleanFloor(struct Floor* floor_p);
static int isFloorClean(struct Floor* floor_p);
static void floorSoftReset(struct Floor* floor_p);

int
runModeOne(int robots_number, int width, int height, statusCallback publishStatus, void * front_data) {
    struct Floor floor = {0};

    void*status,*moverobots_p;

    int is_clean,is_all_clean=0, fail=0;

    status = createFloor(&floor, width, height, robots_number);

    if (status != NULL) {						//if he could assign it, I'll go for a loop
        floor.game_mode = MODE1;
        while ((is_all_clean != TILE_CLEAN) && (fail==0) ) {	//the loop will repeat until it is all clean or until some function fails
            if (publishStatus(&floor, front_data)) {
                destroyFloor(status);
                return FAILURE;
            }
            coords_t max = { .x = (double)floor.width, .y = (double)floor.height };
            moverobots_p = moveRobots(&(floor.robots), max);
            if (moverobots_p != NULL) {

                floor.time_to_clean += 1;

                is_clean = cleanFloor(&floor);
                if (is_clean == SUCCESS) {

                    is_all_clean = isFloorClean(&floor);
                }
                else {
                    fail = 1;		//the robot could not clean its tile(cleanFloor)
                }
            }
            else {					//the robots could not move(moveRobots)
                fail = 1;
            }

        }
    }
    else {
        destroyFloor(status);
        return FAILURE;
    }
    if (fail == 1) {
        destroyFloor(status);
        return FAILURE;
    }
    else {
        return SUCCESS;
    }

}

int
runModeTwo(int width, int height, statusCallback publishStatus, void* front_data) {
    int needed_robots = 0;

    long double ticks_sum = 0.0; // Sum of all times elapsed until now to clean the floor
    long double prev_simu_ticks = 0.0;

    struct Floor current_floor = { 0 };
    coords_t max_robot_coord = { .x = (double)width, .y = (double)height };

    if (createFloor(&current_floor, width, height, needed_robots) == NULL) {
        return FAILURE;
    }

    current_floor.game_mode = MODE2;

    do {
        // The first time this loop is running, thhis three lines 
        // do not do anything really useful.
        ticks_sum = 0.0;
        prev_simu_ticks = current_floor.time_to_clean;
        destroyRobots(&(current_floor.robots));

        needed_robots += 1;
        generateRobots(&(current_floor.robots), needed_robots, max_robot_coord);

        for (int i = 0; i < SIMULATION_ITERATIONS; i++) {
            floorSoftReset(&current_floor);

            do {
                coords_t max = {(double)current_floor.width, 
                                (double) current_floor.height};
                moveRobots(&(current_floor.robots), max); // A robot either moves or changes its angle
                ticks_sum += 1;
                cleanFloor(&current_floor);
            } while (isFloorClean(&current_floor) != TILE_CLEAN);

        }

        current_floor.time_to_clean = ticks_sum / SIMULATION_ITERATIONS;
        
        if (publishStatus(&current_floor, front_data)) {
            destroyFloor(&current_floor);
            return FAILURE;
        }

    } while (!(islessequal(fabs(prev_simu_ticks - current_floor.time_to_clean),
                SIMULATIONS_DELTA)));

    destroyFloor(&current_floor);
    return SUCCESS;
}

/*
 * Instead of recreating a whole floor, let's make it diry again!
 * 
 * Makes all tiles dirty and resets time_to_clean to 0.
 * Robot related variables and structures are ignoread, as well as
 * width and height properties.
 * 
 * Arguments:
 *  floor_p: The floor to soft-reset.
 * 
 * Returns:
 *  Nothing
 */
static void
floorSoftReset(struct Floor* floor_p) {
    for (int i = 0; i < floor_p->clean_size; i++) {
        floor_p->clean[i] = TILE_DIRTY;
    }
    floor_p->time_to_clean = 0.0;
}

/*
 * Create a floor where robots will work.
 * 
 * Arguments:
 *  floor_p: Structure where the new floor will be stored.
 *  height: Floor's desired height.
 *  width: Floor's desired width.
 *  robots_ammount: How many robots should be initialized over this floor.
 * 
 * Returns:
 *  Success: Pointer to floor_p
 *  Failure: NULL
 *
 */
static void*
createFloor(struct Floor* floor_p, int width, int height, int robots_amount) {
    if (floor_p == NULL)
        return NULL;

    if (width == 0 || height == 0)
        return NULL;

    if (robots_amount < 0)
        return NULL;

    void* pointer;
    coords_t max = { .x = (double)width, .y = (double)height };
    srand((unsigned int)time(NULL)); // For robots :)

    floor_p->clean = (int*) calloc(height*width, sizeof(int));				//an order is placed for a memory segment
    if (floor_p->clean == NULL) {//if returns NULL then the memory segment could not be allocated,otherwise it will contain the segment address
        destroyFloor(floor_p);
        return NULL;
    }
    floor_p->clean_size = height*width;

    floor_p->height = height;
    floor_p->width = width;
    floor_p->time_to_clean = 0;							//initially since the time was not calculated, it is zero

    // All tiles are dirty
    for (int i = 0; i < floor_p->clean_size; i++) {
        floor_p->clean[i] = TILE_DIRTY;     
    }

    pointer = generateRobots(&(floor_p->robots), robots_amount, max);

    if (pointer == NULL) {							//if it points to null then the robots could not be created
        destroyFloor(floor_p);
        return NULL;
    }
    
    return floor_p;
}

/*
 * Destroy a floor.
 * 
 * Arguments:
 *  floor_p: Floor to destroy.
 * 
 * Returns:
 *  Nothing
 */
static void
destroyFloor(struct Floor* floor_p){
    if (floor_p == NULL)
        return;

    if (floor_p->clean != NULL) free(floor_p->clean);
    floor_p->clean_size = 0;

    floor_p->game_mode = MODE_UNSET;
    floor_p->height = 0;
    floor_p->width = 0;
    floor_p->time_to_clean = 0;

    destroyRobots(&(floor_p->robots));
}

/*
 * Clean those tiles where a robot is standing.
 * 
 * Arguments:
 *  floor_p: Floor to clean.
 * 
 * Returns:
 *  Success: SUCCESS definition
 *  Failure: FAILURE definition
 */
int
cleanFloor(struct Floor* floor_p) {
    if (floor_p == NULL)
        return FAILURE;

    int i = 0;
    while (i < floor_p->robots.robots_number) {
        int x = (int) floor(floor_p->robots.robots[i].coordinates.x);
        int y = (int) floor(floor_p->robots.robots[i].coordinates.y);

        // floor->clean is a 2D dynamic array
        floor_p->clean[x + floor_p->width * y] = TILE_CLEAN;
        i++;
    }
    //printTiles(floor_p);
    return SUCCESS;
}

/*
 * Verify if the whole floor has been cleaned.
 * 
 * Arguments:
 *  floor_p: Floor to verify.
 * 
 * Returns:
 *  All tiles are clean: TILE_CLEAN definition
 *  At least one tile is dirty: TILE_DIRTY definition
 */
int
isFloorClean(struct Floor* floor) {
    if (floor == NULL)
        return FAILURE;

    int tile_status = TILE_CLEAN;
    int i = 0;
    while (i < floor->clean_size && tile_status != TILE_DIRTY) {
        tile_status = floor->clean[i];
        i++;
    }
    return tile_status;
}
