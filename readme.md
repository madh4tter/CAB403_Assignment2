# CAB403_Assignment2
CAB403 Assignment 2 Repository

Task Overview: 

Your task is to develop and submit three pieces of software relating to a car park
management system:

● A car park management system, henceforth referred to as the ‘manager’ - software
that takes care of the automated aspects of running a car park, interacting with
license plate readers, boom gates and electronic displays to ensure smooth
operation and accurate reporting of the car park.

● A car park simulator, henceforth referred to as the ‘simulator’ - software that
simulates all of the aforementioned pieces of hardware such that the manager can be
tested without physical access to the actual car park systems. This also includes
simulating the movement of vehicles around the car park.

● A fire alarm system for the car park, henceforth known as the ‘fire alarm system’ -
software that interacts with temperature sensors within the car park and follows
required procedures if a fire is detected. This component is regarded as a safety
critical system, and while a functioning one has already been implemented
(firealarm.c, available via Blackboard), your task is to assess it, write a report on its
suitability as a safety-critical software component, and if necessary, fix or rewrite it.

These three pieces of software are all separate programs. They must be written in standard
C (no other programming languages are permitted) and be compilable and runnable on the
provided MX Linux virtual machine as-is. You will need to make use of the POSIX threads
library (pthreads), POSIX shared memory and other libraries that you have used in the
CAB403 practical classes to complete this assignment.
To simulate the interactions between the manager and the physical hardware present in the
car park, a shared memory segment is used by the three programs to communicate- for
example, the manager will check the status of the license plate recognition (LPR) sensors
and control boom gates and digital signs by accessing this segment. A detailed description
of the structure of this shared memory segment is available in the section titled ‘Shared
memory’.
