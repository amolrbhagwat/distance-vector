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

#define MAX_LINE_LENGTH 255


using namespace std;


void readConfigFile(char* filename){
	char line[MAX_LINE_LENGTH];
	char *nodename; 	//[32];//[NodeLength];
	char *nbr; // yes if neighbour, no if not
	
	ifstream configfile(filename);

	while(configfile.getline(line, MAX_LINE_LENGTH, '\n')){

		nodename = strtok(line, " ");
		nbr = strtok(NULL, " ");
		string n = nbr;
		bool neighbouring = false;
		if(n.compare("yes") == 0) neighbouring = true;

		cout << nodename << " " << neighbouring << endl;
		bzero(line, MAX_LINE_LENGTH);

		
	}    

}

void initialize(char* filename){
	readConfigFile(filename);

}


int main(int argc, char* argv[]) {
	// check the parameters	
	if(argc < 7){
		cout << "Parameters: " << "configfile portnumber TTL Infinity period splithorizon" << endl;
		return 1;	// no parameters passed
	}
	
	initialize(argv[1]);

	cout << "Reached end.";

	/*
	int server_socket, client_socket, portno, no_of_bytes;
	char buffer[BUFFER_SIZE];

	struct sockaddr_in server_address, client_address;
	socklen_t client_length;

	// creating the socket
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_socket < 0){
		cout << "Error while creating socket\n";
		return 1;
	}

	bzero((char *) &server_address, sizeof(server_address));
	portno = atoi(argv[1]);

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(portno);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind the socket to the port
	if(bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
		cout << "Binding error\n";
		return 1;
	}

	*/

	return 0;
}








