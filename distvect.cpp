#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
}rtable[MAX_NODES];

struct 	ThreadParam {
	// Save the thread parameters in a struct
	int socketfd; 							// binded socket FD
	struct sockaddr_in clientadd; 			// Client Address info
	struct timeval t1;
};

void initialize(char*, int, u_short);
void readConfigFile(char*);
void generateGraph(int);
void displayGraph();
void generateRoutingTable(u_short);
void displayRoutingTable();
void sendAdv();
void update();

int hostnameToIp(const char*, sockaddr_in*);


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

	initialize(configfile, infinity, ttl);
	sendAdv();
	update();
	
	// Just for printing
	int i = 0;
	cout << "Nodes: \n";
	for (i=0; i< node_count; i++){
		cout << i << " " << nodes[i] << " " << endl;
	}

	cout << "Neighbours: \n";
	for (i=0; i<neighbour_count; i++){
		cout << neighbours[i] << " " << endl;
	}

	cout << "Node count: " << node_count << endl;
	cout << "Neighbour count: " << neighbour_count << endl;

	return 0;
}

void initialize(char* filename, int infinity,u_short ttl){
	readConfigFile(filename);
	generateGraph(infinity);
	cout << "Initial graph:\n";
	displayGraph();
	generateRoutingTable(ttl);
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
	int a = 0;
	printf("--"); 
	while(a < node_count){
		printf(" %2d", a);
		a++;	
	}
	printf("\n");
	for(int i = 0; i < node_count; i++){
		printf("%2d ", i);
		for(int j = 0; j < node_count; j++){
			printf("%2d ", graph[i][j]);
		}
		printf("\n");
	}
}

void generateRoutingTable(u_short ttl){
	// for own entry
	hostnameToIp(nodes[0].c_str(), &rtable[0].destadr);
	hostnameToIp(nodes[0].c_str(), &rtable[0].nexthop);
	rtable[0].cost = 0;
	rtable[0].ttl = ttl;

	for(int i = 1; i < node_count; i++){
		// the destination node
		hostnameToIp(nodes[i].c_str(), &rtable[i].destadr);

		// the next hop
		// if it's a neighbour, then it is the next hop as well
		if(is_neighbour[i]){
			hostnameToIp(nodes[i].c_str(), &rtable[i].nexthop);
			rtable[i].cost = 1;			
		}
		else{
			rtable[i].cost = 0;	
		}
		rtable[i].ttl = ttl;
	}
}

void displayRoutingTable(){
	cout << "\n\nRouting table:\n";

	for(int i = 0; i < node_count; i++){
		char ipadd[INET_ADDRSTRLEN];

		cout << "Entry " << i+1 << endl;
		cout << "Node: " << inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, ipadd, INET_ADDRSTRLEN) << endl;
		cout << "Next: " << inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, ipadd, INET_ADDRSTRLEN) << endl;
		cout << "Cost: " << rtable[i].cost << endl;
		cout << "TTL : " << rtable[i].ttl << endl;
		cout << endl;
	}
}

void sendAdv(){
}

void update(){
}

void *rcvAcks(void *paramP) {
	return 0;
}

int hostnameToIp(const char * hostname , sockaddr_in* node){
	// http://stackoverflow.com/a/5445610

	struct hostent        *he;
	
	/* resolve hostname */
	if ((he = gethostbyname(hostname)) == NULL){
	    return 1;
	}

	/* copy the network address to sockaddr_in structure */
	memcpy(&node->sin_addr, he->h_addr_list[0], he->h_length);
	
	return 0;
}


