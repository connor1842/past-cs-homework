'''
Connor Shields
ftclient - acts as the client program in the file transfer implementation
CS372
11/25/2018
'''

import socket
import sys
import signal
import re
import math
from pathlib import Path	

DEBUG = False

def debugprint(debugString):
	if DEBUG is True:
		print(debugString)

#	setupConnection method:
#	takes serversocket, host address, and port number as inputs
#	binds the socket and starts listening for a connection
def setupConnection(serversocket, host, port, controlConnection):
	serverStatus = controlConnection.recv(7).decode('ascii')
	if serverStatus != 'Waiting':
		return 'Failed', 'Failed'
	try:
		serversocket.bind((host, int(port)))
	except:
		print("port already in use")
		exit(1)

	controlConnection.send('Ready'.encode('ascii'))
	serversocket.listen(1)
	clientsocket, addr = serversocket.accept()
	return clientsocket, addr



#	takes the hostname and port number of the server the user wants to connect to
#	returns a control connection file descriptor for communication with the server
def connectToHostSocket(hostname, serverPort):
	socketConnection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	serverInfo = (hostname, int(serverPort))
	socketConnection.connect(serverInfo)
	return socketConnection



#	parses and verifies all arguments. Returns false if any of the arguments are invalid,
#	otherwise, returns all of the arguments parsed into an array
def parseArgs(arguments):
	if len(arguments) < 5:
		print("Error: Please enter command in the format:\n 'ftclient.py [hostname] [portnumber] [command] [dataport or download file] [data port (if download file was specified)]'")
		return False, False, False, False, False, False
	hostname = arguments[1] + '.engr.oregonstate.edu'
	serverPort = arguments[2]
	command = arguments[3]
	validated = True
	if len(arguments) == 6:
		downloadFile = arguments[4]
		dataPort = arguments[5]
		#verifies that the file doesn't already exist
		#reference: //stackoverflow.com/questions/82831/how-do-i-check-whether-a-file-exists-without-exceptions
		if Path('./'+downloadFile).is_file():
			overwrite = input(downloadFile + ' already exists. Do you want to overwrite it? (Y/N)\n')
			if overwrite != 'Y':
				print("Exiting")
				exit(0)

	else:
		dataPort = arguments[4]
		downloadFile = ''
	if not serverPort.isdigit():
		validated = False
		print('Invalid port entered. Specify the server port you want to connect to in the second command line argument')
	if not dataPort.isdigit():
		validated = False
		print('Invalid port entered. Specify the port you want to use to get data from the server')
	if command != '-l' and command != '-g':
		validated = False
		print("Invalid command. Enter '-l' to list the directory or '-g' to download a file")
	if command == '-l':
		if len(downloadFile) > 0:
			validated = False
			print("Invalid command. Do not specify a file name with the -l command.")
	if command == '-g' and len(downloadFile) == 0:
		validated = False
		print("Invalid command. Please specify a file name when sending the -g command.")
	return validated, hostname, serverPort, dataPort, command, downloadFile


#	called from main: takes the parsed command line arguments and established control connection as parameters
#	sends each important argument to the server and verifies that each was received
def sendCommand(controlConnection, command, downloadFile, myHostName, dataPort):
	#Send the hostname so the server knows where to establish the data connection
	controlConnection.send(myHostName.encode('ascii'))
	response = controlConnection.recv(500).decode('ascii')
	if response != 'Received Address':
		print("Error sending address")
		print("Response: " + response)
		return
	#send the port for the server to connect
	controlConnection.send(dataPort.encode('ascii'))
	response = controlConnection.recv(500).decode('ascii')
	if response != 'Received Port':
		print("Error sending port number")
		print("Response: " + response)
		return
	#send the command to the server
	controlConnection.send(command.encode('ascii'))
	response = controlConnection.recv(500).decode('ascii')
	if response != 'Received Command':
		print("Error sending command")
		print("Response: " + response)
		return
	#send the download file name to the server, if there is one
	if command == '-g':
		controlConnection.send(downloadFile.encode('ascii'))
		response = controlConnection.recv(500).decode('ascii')
		if response != 'Received filename':
			print("Error sending filename")
			print("Response: " + response)
			return


#	takes the data connection as parameter
#	receives a single string containing the directory structure
def receiveDirectoryInfo(dataConnection):
	fullDirectory = dataConnection.recv(1024).decode('ascii').split('\t')
	for listing in fullDirectory:
		print(listing)



#	Handler for receiving a file from the server
#	takes the control connection and data connection as parameters, as well as the filename
#	receives the size of the file from the server over the control connection
#	uses the control connection to verify with the server that the correct size was received
#	receives the contents of the file over the data connection, then writes each packet to a local file
def receiveFile(dataConnection, controlConnection, filename):
	newFile = open(filename, 'w+')
	filesize = controlConnection.recv(32).decode('ascii')
	packetsize = 1000
	if filesize == filename + ' does not exist':
		print(filesize)
		return
	controlConnection.send(filesize.encode('ascii'))
	status = controlConnection.recv(8).decode('ascii')
	if status != 'continue':
		print('The server failed to send the correct file size. Try again')
		print(status)
		return
	numPackets = math.ceil(int(filesize) / packetsize)
	received = 0
	for i in range(0, int(numPackets)):
		if i == numPackets - 1:
			fileChunk = dataConnection.recv((int(filesize) % packetsize)).decode('ascii')
		else:
			fileChunk = dataConnection.recv(packetsize).decode('ascii')
		newFile.write(fileChunk)
		received += len(fileChunk)

	newFile.close()
	print("File transfer complete")



#	this is the launching point for receiving either a directory or a file
#	called from main after the server has designated that it is ready to send data
#	calls receiveDirectoryInfo or receiveFile
def receiveData(dataConnection, controlConnection, command, filename, serverport, hostname):
	if command == '-l':
		print('Receiving directory structure from ' + hostname + ': ' + serverport)
		receiveDirectoryInfo(dataConnection)
	elif command == '-g':
		print('Receiving ' + filename + ' from ' + hostname + ': ' + serverport)
		receiveFile(dataConnection, controlConnection, filename)

#	main function
#	coordinates the ftclient program
if __name__ == '__main__':
	validated, hostname, serverPort, dataPort, command, downloadFile = parseArgs(sys.argv)
	if not validated:
		exit(1)
	myHostName = socket.gethostname()
	controlConnection = connectToHostSocket(hostname, serverPort)
	sendCommand(controlConnection, command, downloadFile, myHostName, str(dataPort))
	serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	dataConnection, addr  = setupConnection(serversocket, myHostName, dataPort, controlConnection)
	if dataConnection == 'Failed' or addr == 'Failed':
		print("Error setting up data connection")
		exit(1)
	receiveData(dataConnection, controlConnection, command, downloadFile, serverPort, hostname)
	dataConnection.close()



	
