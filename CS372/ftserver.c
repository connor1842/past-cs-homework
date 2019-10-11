#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dirent.h>

/*
 *	Connor Shields
 *	ftserver.c - acts as the server in the file transfer implementation.
 *  The server opens an FTP socket, which remains open until a SIGINT control message
 *  is received. This server supports listing the current server directory
 *  (similar to 'ls') with the 'l' command, and file download with the 'g' command.
 *	CS372
 *	11/25/2018
 *
 * */

int listenSocketFD;

/*
 * generic sendMsg function - all sent messages in this program use this method.
 * takes socket file descriptor and message as input
 * sends the given message over the specified socket FD
 * returns 0 if the message failed to send at all
 * returns -1 if the full message wasn't sent
 * returns 1 otherwise
 *
 */ 
 int sendMsg(int connectionFD, char *msg)
 {
	int charsSent = send(connectionFD, msg, strlen(msg), 0);
	if (charsSent < 0)
		return 0;
	if (charsSent < strlen(msg))
		 return -1;
	
	int checkSend = -5;
	do{
		ioctl(listenSocketFD, TIOCOUTQ, &checkSend);
	}
	while (checkSend > 0);
	
	return 1;
 }

/*
 *
 * - Takes as parameters the port specified on the command line, as well as socket information.
 * - Establishes the socket for the control connection. If successful, the reference to the open
 *   socket will be stored in socketFD. If unsuccessful, the program will terminate.
 *
 */
void socketSetup(char *port, int numConnections, int *socketFD, int *establishedConnectionFD)
{
	int portNumber;
	struct sockaddr_in serverAddress;
	
	memset((char *)&serverAddress, '\0', sizeof(serverAddress));
	portNumber = atoi(port);
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(portNumber);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	*socketFD = socket(AF_INET, SOCK_STREAM, 0);//create socket
	if (*socketFD < 0) printf("Error opening server socket");
	if (bind(*socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
		printf("Error binding to socket\n");
		exit(1);
	}
	listen(*socketFD, numConnections);
	printf("Server open on %s\n", port);
}

/*
 *
 * - Accepts a new connection on the specified port - used to establish the control connection.
 * - The establishedConnectionFD struct reference is ready for use after this function completes.
 *
 */
void acceptNewConnection(int *socketFD, int *establishedConnectionFD)	
{
	struct sockaddr_in clientAddress;
	socklen_t sizeOfClientInfo = sizeof(clientAddress);
	*establishedConnectionFD = accept(*socketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
	if (*establishedConnectionFD < 0)
		printf("Error accepting connection from client\n");
}

/*
 *
 *	- Connects to a data socket created on the client machine
 *	- Returns 0 if any step of the connection fails
 *	- Returns 1 if successful
 *
 */
int socketConnect(char *port, char *hostAddr, int *socketFD, int controlConnectionFD)
{
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	memset((char*)&serverAddress, '\0', sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(atoi(port));
	serverHostInfo = gethostbyname(hostAddr);
	if (serverHostInfo == NULL){
		sendMsg(controlConnectionFD, "No hostname sent. Couldn't establish the connection\n");		
		return 0;
	}
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length);
	*socketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (*socketFD < 0){
		sendMsg(controlConnectionFD, "Unable to setup socket. Try again\n");		
		return 0;
	}

	if (connect(*socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
		sendMsg(controlConnectionFD, "Couldn't connect on specified host. Try again\n");
		return 0;
	}
	return 1;
}

/*
 * - Takes the established data connection as argument
 * - appends each directory item to a string and sends the string to the client
 *
 */
//referred for directory iteration: https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
void sendDirectory(int dataConnectionFD)
{
	struct dirent *directoryItem;
	DIR *directory = opendir(".");
	char fullDirectory[1024];
	memset(fullDirectory, '\0', sizeof(fullDirectory));
	printf("Sending directory contents\n");
	int firstIteration = 1;
	while ((directoryItem = readdir(directory)) != NULL)
	{
		if (!firstIteration)	strcat(fullDirectory, "	");
		strcat(fullDirectory, directoryItem->d_name);
		firstIteration = 0;
	}
	sendMsg(dataConnectionFD, fullDirectory);
}

/*
 *
 * - Takes the file pointer as an argument
 * - Uses fseek to determine the file size
 * - Returns the size of the file
 *
 */
long getFileSize(FILE *filePtr)
{
	fseek(filePtr, 0L, SEEK_END);
	long filesize = ftell(filePtr);
	rewind(filePtr);
	return filesize;
}

/*
 *
 * takes the filename, data connection, and control connection as arguments
 * extracts the file size, sends the size to the client, and verifies that 
 * the correct size was received.
 * sends the contents of the file in increments of 1000 char packets
 *
 */
void sendFile(char *port, char *filename, int controlConnectionFD, int dataConnectionFD)
{
	FILE *filePtr = fopen(filename, "r");
	if (!filePtr)
	{
		char errorMsg[128];
		sprintf(errorMsg, "%s does not exist", filename);
		sendMsg(controlConnectionFD, errorMsg);
		return;
	}
	else
	{
		char fileSize[32];
		char tempFileSize[32];
		memset(fileSize, '\0', sizeof(fileSize));
		memset(tempFileSize, '\0', sizeof(tempFileSize));
		sprintf(fileSize, "%d", getFileSize(filePtr));
		
		int sendTextSizeResult = sendMsg(controlConnectionFD, fileSize);
		if (sendTextSizeResult < 0)
		{
			printf("Warning: Full length of message was not sent\n");
			return;
		}
		if (sendTextSizeResult == 0)
		{
			printf("Warning: message length not sent\n");
			return;
		}
		//the client acknowledges the file size so we know both sides know how much will be sent
		int verifyTextSizeResult = recv(controlConnectionFD, tempFileSize, strlen(fileSize), 0);
		if (strcmp(tempFileSize, fileSize) != 0)
		{
			sendMsg(controlConnectionFD, "Failure.");
			return;
		}

		//Indicate to the client that we are going to start sending the file
		printf("Sending %s to client\n", filename);
		sendMsg(controlConnectionFD, "continue");
		char packet[1001];
		int i, j = 0, done = 0;
		char c = fgetc(filePtr);
		//iterate through each character in the file. Send in groups of 1000 characters.
		while (!done)
		{
			i = 0;
			memset(packet, '\0', sizeof(packet));
			while (i < 1000 && !done)
			{
				packet[i] = c;
				c = fgetc(filePtr);
				i++;
				j++;
				if (c == EOF)	done = 1;
			}
			sendMsg(dataConnectionFD, packet);
		}
	}
	fclose(filePtr);
}

/*
 *
 * - Takes as arguments the client's command information
 * - Accepts the following commands:
 *		- 'l': List the file directory.
 *		- 'g': Download a file. A filename must be provided.
 *
 */
void processCommand(char *clientAddr, char *port, char *command, char *filename, int controlConnectionFD)
{
	int dataConnectionFD;
	int connectionResult = socketConnect(port, clientAddr, &dataConnectionFD, controlConnectionFD);
	if (!connectionResult)	return;
	switch (command[1])
	{
		case 'l':
			printf("List directory requested on port %s\n", port);
			fflush(stdout);
			sendDirectory(dataConnectionFD);
			break;
		case 'g':
			printf("File \"%s\" requested on port %s\n", filename, port);
			fflush(stdout);
			sendFile(port, filename, controlConnectionFD, dataConnectionFD);
			break;
		default:
			break;
	}
}

/*
 *
 * - This prevents potential race conditions: the server waits until an indication that
 *   the client has setup a socket before attempting to connect to the data port.
 * - Returns 1 if the client has indicated it is ready.
 * - Returns 0 otherwise.
 *
 */
int waitForNewConnection(int controlConnectionFD){
	sendMsg(controlConnectionFD, "Waiting");
	char readyStatus[6];
	int charsRead;
	memset(readyStatus, '\0', sizeof(readyStatus));
	charsRead = recv(controlConnectionFD, readyStatus, 5, 0);
	if (strcmp(readyStatus, "Ready") == 0)
		return 1;
	else
		return 0;
}

/*
 *
 * - Receives every element of the client's command over the control connection.
 * - Acknowledges reception of every piece of client's command (address, port,
 *   and command).
 * - Calls processCommand() once all information is received.
 *
 *
 */
void receiveCommand(controlConnectionFD)
{
	char commandBuffer[8], clientAddrBuffer[256], newPortBuffer[32], filename[256];
	int charsRead, sendResult;
	memset(filename, '\0', sizeof(filename));
	memset(commandBuffer, '\0', sizeof(commandBuffer));
	memset(clientAddrBuffer, '\0', sizeof(clientAddrBuffer));
	memset(newPortBuffer, '\0', sizeof(newPortBuffer));

	//Receive address from the client
	charsRead = recv(controlConnectionFD, clientAddrBuffer, 255, 0);
	if (charsRead < 0)
	{ 	
		printf("Error reveiving address from client\n");	 
		return; 
	}
	printf("Connection from %s.\n", clientAddrBuffer);

	//Ackowledge reception of the address
	sendResult = sendMsg(controlConnectionFD, "Received Address");
	if (sendResult < 1)	
		return;

	//Receive desired port number from the client
	charsRead = recv(controlConnectionFD, newPortBuffer, 31, 0);
	if (charsRead < 0)
	{		
		printf("Error receiving port number from client\n"); 
		return; 
	}
	//Ackowledge reception of the port
	sendResult = sendMsg(controlConnectionFD, "Received Port");
	if (sendResult < 1)	
		return;
	
	//Receive command from the client
	charsRead = recv(controlConnectionFD, commandBuffer, 7, 0);
	if (charsRead < 0)
	{	
		printf("Error receiving command from client\n"); 		 
		return; 
	}
	//Acknowledge reception of command
	sendResult = sendMsg(controlConnectionFD, "Received Command");
	if (sendResult < 1)	
		return;
	//Inform the client if command is invalid.
	if (strlen(commandBuffer) != 2 || (commandBuffer[1] != 'g' && commandBuffer[1] != 'l')){
		sendMsg(controlConnectionFD, "Invalid command");
		return;
	}
	//if command is g, then we request a filename for download
	if (commandBuffer[1] == 'g')
	{
		charsRead = recv(controlConnectionFD, filename, 256, 0);
		if (charsRead < 0) 
		{	
			printf("Error receiving filename from client\n");	
			return;
		}
		sendResult = sendMsg(controlConnectionFD, "Received filename");
		if (sendResult < 1)	
			return;
	}
	//waits for client to indicate that it is ready to host a connection for downloading the file.
	int ready = waitForNewConnection(controlConnectionFD);
	if (!ready)
	{
		sendMsg(controlConnectionFD, "Unable to coordinate data connection. Try again");
		return;
	}
	processCommand(clientAddrBuffer, newPortBuffer, commandBuffer, filename, controlConnectionFD);
}
 
/*
 *
 * - Main function: accepts a connection on a loop (maximum of 100 connections)
 * - Calls receive command.
 * - Closes the connection once the command has been received, processed, and executed
 *
 */ 
int main(int argc, char *argv[])
{
	int controlConnectionFD;
	if (argc < 2)
	{ 
		printf("Error: Invalid arguments. Usage: 'ftserver [port]', where [port] is the port number on which to open the connection\n"); 
		return 1;
	}
	//Establish the socket on the specified port
	socketSetup(argv[1], 100, &listenSocketFD, &controlConnectionFD);
	//Loop indefinitely, awaiting connections as they come in
	while (1)
	{
		acceptNewConnection(&listenSocketFD, &controlConnectionFD);
		receiveCommand(controlConnectionFD);
		close(controlConnectionFD);
	}
	
	return 0;
	
}
