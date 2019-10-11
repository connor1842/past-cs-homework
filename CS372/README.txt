Connor Shields
CS372
Program 2 - file transfer implementation
11/25/2018

To compile the program, run 'make all'

To run the server:

enter on the command line: ftserver [port number]

To run the client:

For requesting the directory: 

python3 ftclient.py [server's address] [serverport] -l [clientport]

For requesting a file:

python3 ftclient.py [server's address] [serverport] -g [filename] [clientport]

NOTE 1: When running ftclient.py, only enter flip1, flip2, or flip3 to specify the server's address. The program appends the '.engr.oregonstate.edu' to your argument

NOTE 2: The makefile creates a directory, 'clientDir', and places a copy of ftclient.py there. The client script should be ran from that directory (or copied to a different directory, if you prefer) to avoid conflict when reading/writing files.