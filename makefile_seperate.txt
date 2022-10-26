all: simulator manager #firealarm

simulator : simulator.c
	gcc simulator.c -o simulator -Wall -Wextra -lrt -lpthread

manager : manager.c
	gcc manager.c -o manager -Wall -Wextra -lrt -lpthread

# fire: firealarm.c
# 	gcc firealarm.c -o firealarm -Wall -Wextra -lrt -lpthread

clean: 
	rm -f simulator
	rm -f manager
#	rm -f firealarm
	rm -f *.0
	
.PHONY: all clean
