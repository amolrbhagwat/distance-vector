#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <netdb.h>
#include <sys/time.h>
#include <pthread.h>

using namespace std;

#define MAX_LINE_LENGTH 255
#define MAX_NODES 85
string  NODES[MAX_NODES]; // Stores all nodes
string NHBR[MAX_NODES]; // Stores the node's  neigbhours
int NHBR_COUNT; // count of the number of neigbhours
int SHFlag = 0; // split horizon flag

// routing table entry
struct RouteEntry {
	struct sockaddr_in destAdr;  // address of the destination
	struct sockaddr_in nextHop;  // address of the next hop
	int cost;  // distance
	u_short TTL;  // TTL in seconds
};

// Save the thread parameters in a struct
struct ThreadParam {
	int socketFD; // binded socket FD
	struct sockaddr_in clientAdd; 	// Client Address info
	struct timeval t1;

};



void readConfigFile(char* filename){
	char line[MAX_LINE_LENGTH];
	char *nodename; 	//[32];//[NodeLength];
	char *nbr; // yes if neighbour, no if not
	int i = 0;
	int j = 0;
	ifstream configfile(filename);

	while(configfile.getline(line, MAX_LINE_LENGTH, '\n')){

		nodename = strtok(line, " ");
		nbr = strtok(NULL, " ");
		string temp = nbr;
		// store neighbours
		if(temp.compare("yes") == 0){
			NHBR[j] = nodename;
			j++;
		}
		// store all nodes
		NODES[i] = nodename;
		bzero(line, MAX_LINE_LENGTH);
		i++;
	}    
	NHBR_COUNT = j;

}

void initialize(char* filename){
	readConfigFile(filename);

}

void sendAdv(){

}

void update(){

}


void *sendFile(void *paramP) {

	return 0;
}

void *rcvAcks(void *paramP) {

	return 0;
}

int main(int argc, char* argv[]) {
	int portNo = 55555;
	pthread_t thrdID1, thrdID2;
	u_short TTL = 90;
	u_short period = 30;
	char configFile[15];
	int infinity = 16;

	// check the parameters	
	if(argc < 7){
		cout << "Parameters: " << "configfile portnumber TTL Infinity period splithorizon" << endl;
		return 1;	// no parameters passed
	}
	
	bzero((char *) &configFile, sizeof(configFile));
	strcpy(configFile, argv[1]);
	portNo = atoi(argv[2]);
	TTL = atoi(argv[3]);
	infinity = atoi(argv[4]);
	period = atoi(argv[5]);
	SHFlag = atoi(argv[6]);

	// 1. initialize
	initialize(configFile);
////////////////////////
	// 2. send adv
	sendAdv();

	// 3. update
	update();
	// Just for printing
	int i = 0;
	for (i=0; i<MAX_NODES; i++)
		cout << NODES[i] << " " << endl;

	cout << "Neighbours: \n";
	for (i=0; i<NHBR_COUNT; i++)
		cout << NHBR[i] << " " << endl;
	cout << "Reached end.";
////////////////////////////////////////////////

	// we will use some code from this part later
	/*
	// 0. Setting up the server
	bzero((char *) &serverAdd, sizeof(serverAdd));
	serverAdd.sin_family = AF_INET;
	serverAdd.sin_addr.s_addr = INADDR_ANY;
	serverAdd.sin_port = htons(portNo); // convert to network byte order

	// 1. Create a socket
	// socket (address domains, socket type , protocol)
	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0) {
		printf("\n socket creation failed");
		exit(1);
	}
	serverAdd.sin_family = AF_INET;

	// 2. Bind
	bindNo = bind(socketFD, (struct sockaddr *) &serverAdd, sizeof(serverAdd));
	if (bindNo < 0) {
		printf("Binding failed, the socket may be used by another process\n");
		exit(1);
	}
	socklen_t cAddLen = sizeof(struct sockaddr_in);

	// Setting up the parameters to be sent threads
	struct ThreadParam param;
	struct ThreadParam *paramPointer;
	paramPointer = &param;
	paramPointer->socketFD = socketFD;
	paramPointer->clientAdd = clientAdd;

	// 4. Create a thread to Send File chunks
	if (pthread_create(&thrdID1, NULL, sendFile, (void *) paramPointer)) {
		printf("Pthread creation failed \n");
		exit(1);
	}

	// 5. Create a thread that Receives Acks
	if (pthread_create(&thrdID2, NULL, rcvAcks, (void *) paramPointer)) {
		printf("Pthread creation failed \n");
		exit(1);
	}
	pthread_join(thrdID1, NULL);
	pthread_join(thrdID2, NULL);
*/

	return 0;
}








