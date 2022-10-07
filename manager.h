/*****************************************************************//**
 * \file   manager.h
 * \brief  API definition for the manager c file 
 * 
 * \author Fraser Toon + Kanye West
 * \date   24/09/2022
 *********************************************************************/
#pragma once

/**
 * Monitor the status of the LPR sensors and keep track of where each car is in the car
 * park
*/

/**
 * Tell the boom gates when to open and when to close (the boom gates are a simple
 * piece of hardware that can only be told to open or close, so the job of automatically
 * closing the boom gates after they have been open for a little while is up to the
 * manager)
 * 
*/

/**
 * Control what is displayed on the information signs at each entrance
*/

/**
 * As the manager knows where each car is, it is the manager’s job to ensure that there
 * is room in the car park before allowing new vehicles in (number of cars < number of
 * levels * the number of cars per level). The manager also needs to keep track of how
 * full the individual levels are and direct new cars to a level that is not fully occupied
*/

/**
 * Keep track of how long each car has been in the parking lot and produce a bill once
 * the car leaves.
*/

/**
 * Display the current status of the parking lot on a frequently-updating screen, showing
 * how full each level is, the current status of the boom gates, signs, temperature
 * sensors and alarms, as well as how much revenue the car park has brought in so far
*/


// Get shared memory and check it works (What infomation is being shared to manager?)\

// Create a vechicle type to store infomation about each vechile that comes in

// Maybe create level type that stores infomation about what vehicle is where (Hash table? 5x(Whatever))

// Threads that need to be used (One for each enterence (5), exit (5), boomgate enternce and boomgate exit) and a status thread

// Manage enterence, what vehicle are coming in, where they go and operate boom gate

// Manage exit, what vehicle are leaving, where they go and operate boom gate

// Mange each level (What vehicle are going where and recording in vehicle type)
//  Find level to put car, find car park in level to put car

// Raise and lower boom gate to allow vehicle to go through (2 funcations, reads licence plate)
//  How long will the boom gate stay up for

// Check whether an emergency is called (If so do what?)
//  Use infomation from the shared data steam

// Calculate revenue (Maybe type that records what vehicle come in, what time they do and what time they leave)

