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
#define BUFFER_SIZE 1472


void initialize();
void generateGraph();
void generateRoutingTable();
void *update(void*);
void decrementTTLs();
void sendAdv();
void *getAdvs(void*);
void processAdv(char *, char*);

int hostnameToIp(const char*, sockaddr_in*);
void refreshValues(string, string, char*);
void readConfigFile();
void displayGraph();
void displayRoutingTable();
string makeAdv();
void showStats();

char configfilename[15];	
	
int portno, infinity;
u_short ttl;
u_short period;
int 	shflag = 0; 						// split horizon flag

string 	nodes[MAX_NODES]; 					// stores names of all the nodes, including itself
string 	neighbours[MAX_NODES]; 				// stores names of self's neigbhours

bool	is_neighbour[MAX_NODES]; 			// bool array of whether a node is a neighbour or not
int		graph[MAX_NODES][MAX_NODES];

int 	node_count = 0; 					// total number of nodes in the network, including itself
int 	neighbour_count = 0; 				// number of neigbhours

struct 	RouteEntry {
	struct sockaddr_in destadr;  			// address of the destination
	struct sockaddr_in nexthop;  			// address of the next hop
	int cost;  								// distance
	u_short ttl;  							// TTL in seconds
} rtable[MAX_NODES];
	

int main(int argc, char* argv[]) {
	// check the parameters	
	if(argc < 7){
		cout << "Parameters: " << "Configfile Portnumber TTL Infinity Period Splithorizon" << endl;
		return 1;
	}
	
	bzero((char *) &configfilename, sizeof(configfilename));
	strcpy(configfilename, argv[1]);
	
	portno = atoi(argv[2]);
	ttl = atoi(argv[3]);
	infinity = atoi(argv[4]);
	period = atoi(argv[5]);
	shflag = atoi(argv[6]);

	showStats();
	
	initialize();
			
	pthread_t thread_update, thread_listener;

	if( pthread_create( &thread_update , NULL , update , NULL) < 0){
	      cout << "Could not create update thread.\n";
	      exit(EXIT_FAILURE);
	}
	if( pthread_create( &thread_listener , NULL , getAdvs , NULL) < 0){
	      cout << "Could not create listener thread.\n";
	      exit(EXIT_FAILURE);
	}

	pthread_join(thread_update, NULL);
	pthread_join(thread_listener, NULL);

	return 0;
}

void initialize(){
	readConfigFile();
	generateGraph();
	cout << "Initial graph:\n";
	displayGraph();
	generateRoutingTable();
	displayRoutingTable();
}

void readConfigFile(){
	char line[MAX_LINE_LENGTH];
	char *nodename;
	char *nbr;
	
	is_neighbour[0] = false;
	nodes[0] = "localhost";
	
	int i = 1;
	
	ifstream configfile(configfilename);

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

void generateGraph(){
	for(int i = 0; i < node_count; i++){
		for(int j = 0; j < node_count; j++){
			graph[i][j] = infinity;
		}
	}
    graph[0][0] = 0;
	for(int i = 1; i < node_count; i++){
		if(is_neighbour[i]){
			graph[0][i] = 1;
		}
		else{
			graph[0][i] = infinity;
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

void generateRoutingTable(){
	
	// for own entry
	
	hostnameToIp(nodes[0].c_str(), &rtable[0].destadr);
	hostnameToIp(nodes[0].c_str(), &rtable[0].nexthop);
	rtable[0].cost = 0;
	rtable[0].ttl = ttl;

	// for the other nodes

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
			rtable[i].cost = infinity;	
		}
		rtable[i].ttl = ttl;
	}
}

void displayRoutingTable(){
	cout << "\n\nRouting table:\n";

	for(int i = 0; i < node_count; i++){
		char ipadd[INET_ADDRSTRLEN];

		cout << "Entry " << i << endl;
		cout << "Node: " << inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, ipadd, INET_ADDRSTRLEN) << endl;
		cout << "Next: " << inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, ipadd, INET_ADDRSTRLEN) << endl;
		cout << "Cost: " << rtable[i].cost << endl;
		cout << "TTL : " << rtable[i].ttl << endl;
		cout << endl;
	}
}

void *update(void* a){
    while(true){
        displayRoutingTable();
        sleep(5);
        decrementTTLs();
        sendAdv();          
    }
    return NULL;
}

void decrementTTLs(){
	for(int i = 1; i < node_count; i++){
		rtable[i].ttl -= period;
		if(rtable[i].ttl == 0){
			rtable[i].nexthop.sin_addr.s_addr = 0;
            rtable[i].cost = infinity;
            graph[0][i] = infinity;
			rtable[i].ttl = ttl;
		}
	}
}

void *getAdvs(void *b) {
	int server_socket, client_socket, no_of_bytes;
	struct sockaddr_in server_address, client_address;
	socklen_t client_length;
	char buffer[BUFFER_SIZE];
	char from_ip[INET_ADDRSTRLEN];
	bzero(buffer, BUFFER_SIZE);
		
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_socket < 0){
		cout << "Error while creating socket\n";
		return NULL;
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(portno);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
		cout << "Binding error\n";
		return NULL;
	}

	while(true){
		listen(server_socket, 5);
		client_length = sizeof(client_address);
		
		if(client_socket < 0){
			//cout << "Error while accepting connection\n";
			continue;
		}

		bzero(buffer, BUFFER_SIZE);
		no_of_bytes = recvfrom(server_socket,buffer,BUFFER_SIZE,0,(struct sockaddr *)&client_address,&client_length);

		//cout << "Received: " << buffer << endl;
		//cout << "From: " << 
        inet_ntop(AF_INET, &client_address.sin_addr, from_ip, INET_ADDRSTRLEN);// << endl;
		
		processAdv(buffer, from_ip);
	}
	return NULL;
}

void processAdv(char* recdadv, char* from_ip){
	// when a advertisement is receieved from a neighbour, the neighbour's TTL is restored
    // then check each and every entry in the advertisement, if there's a lower cost and make updates as needed
    // finally, for all nodes in rtable which have as nexthop, the node from which adv was recd, restore TTL


    // restoring neighbour's TTL

    char temp_ip[INET_ADDRSTRLEN];

    for(int i = 1; i < node_count; i++){
        inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, temp_ip, INET_ADDRSTRLEN);
        if(strcmp(temp_ip, from_ip) == 0 && is_neighbour[i]){
            cout << "Heard from: " << from_ip << endl;
            rtable[i].cost = 1;
            rtable[i].ttl = ttl;
            hostnameToIp(nodes[i].c_str(), &rtable[i].nexthop);
            break;
        }
    }

    // check for lower costs, and update/refresh if needed

    char token1[20], token2[20];
	char *temp_str;

	temp_str = strtok (recdadv,",:");

	while (temp_str != NULL)	{
		strcpy(token1, temp_str);
		string destination(token1);
		
		temp_str = strtok (NULL, ",;");
		strcpy(token2, temp_str);
		string costtodestination(token2);
		
        refreshValues(destination, costtodestination, from_ip);	
		
		temp_str = strtok (NULL, ",;");		
	}

    // for the remaining nodes connected through the node which advertised, restore TTL
    
    for(int i = 1; i < node_count; i++){
        inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, temp_ip, INET_ADDRSTRLEN);
        if(strcmp(temp_ip, from_ip) == 0){
            rtable[i].ttl = ttl;
        }
    }
}

void refreshValues(string destination, string costtodestination, char *through){
    //cout << "Destination: " << destination << endl;
    //cout << "Cost: " << costtodestination << endl;

    int index;
    char dest_ip[INET_ADDRSTRLEN];

    for(index = 1; index < node_count; index++){
        inet_ntop(AF_INET, &rtable[index].destadr.sin_addr, dest_ip, INET_ADDRSTRLEN);
        if(strcmp(dest_ip, destination.c_str()) == 0){
            //cout << destination << " found at index " << index << endl;
            break;
        }
    }

    if(index == node_count){
        // not found
        return;
    }

    int current_cost = rtable[index].cost;
    int given_cost = stoi(costtodestination);

    if(given_cost + 1 < current_cost){
        rtable[index].ttl = ttl;
        rtable[index].cost = given_cost + 1;
        inet_pton(AF_INET, through, &rtable[index].nexthop.sin_addr);

        //displayGraph();
        //displayRoutingTable();
    }
}

int hostnameToIp(const char * hostname , sockaddr_in* node){
	// Source: http://stackoverflow.com/a/5445610

	struct hostent        *he;
	
	/* resolve hostname */
	if ((he = gethostbyname(hostname)) == NULL){
	    return 1;
	}

	/* copy the network address to sockaddr_in structure */
	memcpy(&node->sin_addr, he->h_addr_list[0], he->h_length);
	
	return 0;
}

string makeAdv(){
	char ipadd[INET_ADDRSTRLEN];

	string adv = "";
	
	for(int i = 1; i < node_count; i++){
		adv.append(inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, ipadd, INET_ADDRSTRLEN));
		adv.append(",");
		
		adv.append(to_string(rtable[i].cost));
		adv.append(";");
	}
	return adv;
}

void sendAdv(){
    string adv = makeAdv();
    
    int udp_socket, no_of_bytes;
    struct sockaddr_in server_address;
    struct hostent *server;
    socklen_t len;

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
       std::cout << "Error opening socket!\n";
       return;
    }
    
    for(int i = 0; i < node_count; i++){
        if(is_neighbour[i]){
            server = gethostbyname(nodes[i].c_str());

            //cout << "Sending to: " << nodes[i] << " ";

            if (server == NULL) {
               cout << nodes[i] << "FAILED - Host not found - " << nodes[i] << endl;
               continue;
            }
            
            bzero((char *) &server_address, sizeof(server_address));
            server_address.sin_family = AF_INET;
            bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
                
            server_address.sin_port = htons(portno);
            if (connect(udp_socket,(struct sockaddr *) &server_address,sizeof(server_address)) < 0) {
               std::cout << "FAILED - Error while connecting to " << nodes[i] << endl;
               continue;
            }
                
            no_of_bytes = sendto(udp_socket,adv.c_str(),adv.length(),0,(struct sockaddr *)&server_address,sizeof(server_address));
            //cout << " SUCCESSFUL - Sent " << no_of_bytes << " Bytes" << endl;
            if (no_of_bytes < 0) {
               std::cout << "FAILED - Error while writing to " << nodes[i] << endl;
               continue;
            }
        }
    }
}

void showStats(){
	int i = 0;
	cout << "\nNodes: \n";
	for (i=0; i< node_count; i++){
		cout << i << " " << nodes[i] << " " << endl;
	}
	cout << "Node count: " << node_count << endl;
	

	cout << "\nNeighbours: \n";
	for (i=0; i<neighbour_count; i++){
		cout << neighbours[i] << " " << endl;
	}

	cout << "Neighbour count: " << neighbour_count << endl;
}

