import os
import sys

if (len(sys.argv) == 3):
    width = sys.argv[1]
    height = sys.argv[2]
    if (int(width) > 85 or int(height) > 25):
        print('Too big! Python will probably crap itself. Try using cat for REALLY BIG mazes')
        print('I will write 10 10 to the proc now')
        with open('/proc/raptor_maze', 'w') as f:
            f.write('10 10')
    else:
        with open('/proc/raptor_maze', 'w') as f:
            f.write(width + ' ' + height)

with open('/proc/raptor_maze', 'r') as f:
    print(f.read())
