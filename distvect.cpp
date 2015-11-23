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

using 	namespace std;

#define MAX_LINE_LENGTH 255
#define MAX_NODES 100

string 	nodes[MAX_NODES]; 					// stores names of all the nodes, including itself
string 	neighbours[MAX_NODES]; 				// stores names of self's neigbhours

bool	is_neighbour[MAX_NODES]; 			// bool array of whether a node is a neighbour or not
int		graph[MAX_NODES][MAX_NODES];

int 	node_count = 0; 					// total number of nodes in the network, including itself
int 	neighbour_count = 0; 				// number of neigbhours

int 	shflag = 0; 						// split horizon flag

struct 	RouteEntry {
	struct sockaddr_in destadr;  			// address of the destination
	struct sockaddr_in nexthop;  			// address of the next hop
	int cost;  								// distance
	u_short ttl;  							// TTL in seconds
};

struct 	ThreadParam {
	// Save the thread parameters in a struct
	int socketfd; 							// binded socket FD
	struct sockaddr_in clientadd; 			// Client Address info
	struct timeval t1;
};

void initialize(char*, int);
void readConfigFile(char*);
void generateGraph(int);
void displayGraph();
void generateRoutingTable();
void displayRoutingTable();
void sendAdv();
void update();


int main(int argc, char* argv[]) {
	// check the parameters	
	if(argc < 7){
		cout << "Parameters: " << "Configfile Portnumber TTL Infinity Period Splithorizon" << endl;
		return 1;	// no parameters passed
	}
	
	int portno, infinity;
	u_short ttl = 90;
	u_short period = 30;
	
	char configfile[15];	
	bzero((char *) &configfile, sizeof(configfile));
	strcpy(configfile, argv[1]);
	
	portno = atoi(argv[2]);
	ttl = atoi(argv[3]);
	infinity = atoi(argv[4]);
	period = atoi(argv[5]);
	shflag = atoi(argv[6]);

	initialize(configfile, infinity);
	sendAdv();
	update();
	
	// Just for printing
	int i = 0;
	cout << "Nodes: \n";
	for (i=0; i< node_count; i++)
		cout << i << " " << nodes[i] << " " << endl;

	cout << "Neighbours: \n";
	for (i=0; i<neighbour_count; i++)
		cout << neighbours[i] << " " << endl;
	cout << "Reached end.\n";
	

	return 0;
}

void initialize(char* filename, int infinity){
	readConfigFile(filename);
	generateGraph(infinity);
	cout << "Initial graph:\n";
	displayGraph();
	generateRoutingTable();
	displayRoutingTable();
}

void readConfigFile(char* filename){
	char line[MAX_LINE_LENGTH];
	char *nodename;
	char *nbr;
	
	is_neighbour[0] = false;
	nodes[0] = "localhost";
	
	int i = 1;
	
	ifstream configfile(filename);

	while(configfile.getline(line, MAX_LINE_LENGTH, '\n')){
		nodename = strtok(line, " ");
		nbr = strtok(NULL, " ");
		string temp = nbr;
		
		is_neighbour[i] = false;
		
		if(temp.compare("yes") == 0){
			neighbours[neighbour_count] = nodename;
			neighbour_count++;
			is_neighbour[i] = true;
		}
		// store all nodes
		nodes[i] = nodename;
		bzero(line, MAX_LINE_LENGTH);
		i++;
	}  

	node_count = i;  
}


void generateGraph(int infinity){
	for(int i = 0; i < node_count; i++){
		for(int j = 0; j < node_count; j++){
			graph[i][j] = infinity;
		}
	}
	for(int i = 0; i < node_count; i++){
		if(is_neighbour[i]){
			graph[0][i] = 1;
		}
		else{
			graph[0][i] = 0;
		}
	}
}

void displayGraph(){
	printf("-- %2d %2d %2d %2d %2d %2d %2d %2d\n", 0, 1, 2, 3, 4, 5, 6, 7);
	for(int i = 0; i < node_count; i++){
		printf("%2d ", i);
		for(int j = 0; j < node_count; j++){
			printf("%2d ", graph[i][j]);
		}
		printf("\n");
	}

}

void generateRoutingTable(){

}

void displayRoutingTable(){
	
}

void sendAdv(){
}

void update(){
}

void *rcvAcks(void *paramP) {
	return 0;
}






