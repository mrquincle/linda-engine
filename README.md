# Linda Engine

Dear developer, dear user!

This directory contains several subfolders. Each of those folders can be checked out as a separate project in the Eclipse GUI. 

The Colinda code should run on an embedded microcontroller. This means that caution is needed with printf statements and other functionalities that do not exist on the target platform. 

Another point is that some tests run with the player/stage simulator and link to the libplayerc++ library. If you do not intend to run that test, either remove the -lplayerc++ flag from the linker in Project Properties. Or install the player on your machine. On Ubuntu this is "sudo aptitude install libplayerc++2" (the dev files are not needed) and it's a pity but after that you probably need to create one symbolic link: "sudo ln -s /usr/lib/libplayerc++.so.2.0.4 /usr/lib/libplayerc++.so"
  
Enjoy the engine!! :-) 

## Compilation

To compile the Robot simulator add paths like this to the Eclipse Project Properties for the GCC C Compiler:

"${workspace_loc}/${project_name}/inc"

## Copyrights
The copyrights (2008) belong to:

- Author: Anne van Rossum
- Almende B.V., http://www.almende.com
- Rotterdam, The Netherlands


