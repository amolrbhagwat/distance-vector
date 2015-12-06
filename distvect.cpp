/*
CN Assignmnent 3
Amol Bhagwat
*/

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


void	initialize();
void	generateGraph();
void	generateRoutingTable();
void	*update(void*);
void	decrementTTLs();
void	sendAdv();
void	*getAdvs(void*);
void 	processAdv(char *, char*);

int 	hostnameToIp(const char*, sockaddr_in*);
void 	readConfigFile();
void	displayGraph();
void 	displayRoutingTable();
string 	makeAdv(char *);
void 	showStats();
bool 	updateGraph(int, string, string);
int 	getIndexFromAddr(const char*);
bool 	updateRoutingTable(int);
bool	 updateRoutingTable();
char 	configfilename[15];	
	
int 	portno, infinity;
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
	
pthread_mutex_t lock;

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

	//showStats();
	
	initialize();
			
	pthread_t thread_update, thread_listener;
	
	if (pthread_mutex_init(&lock, NULL) != 0){
	    printf("Mutex initialization failed!\n");
	    return 1;
	}

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
	pthread_mutex_destroy(&lock);

	return 0;
}

void initialize(){
	readConfigFile();
	generateGraph();
	cout << "Initial graph:\n";
	//displayGraph();
	generateRoutingTable();
	//displayRoutingTable();
}

void generateGraph(){
	// fill all cells with infinity
	for(int i = 0; i < node_count; i++){
		for(int j = 0; j < node_count; j++){
			graph[i][j] = infinity;
		}
	}
	// diagonal with zeros
	for(int i = 0; i < node_count; i++){
		graph[i][i] = 0;
	}
	// own vector
	for(int i = 1; i < node_count; i++){
		if(is_neighbour[i]){
			graph[0][i] = 1;
		}
		else{
			graph[0][i] = infinity;
		}
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
			rtable[i].nexthop.sin_addr.s_addr = 0;
			rtable[i].cost = infinity;	
		}
		rtable[i].ttl = ttl;
	}
}

void *update(void* a){
    while(true){
    	displayGraph();
        displayRoutingTable();
        sleep(period);
        
        pthread_mutex_lock(&lock);
        decrementTTLs();
        updateRoutingTable();
        sendAdv();
        pthread_mutex_unlock(&lock);        
    
    }
    return NULL;
}

void decrementTTLs(){
	for(int i = 1; i < node_count; i++){
		if(rtable[i].ttl == 0){
			continue;
		}
		rtable[i].ttl -= period;
		if(rtable[i].ttl == 0){
			rtable[i].nexthop.sin_addr.s_addr = 0; // setting null as next hop
            rtable[i].cost = infinity;
            graph[0][i] = infinity;
			//rtable[i].ttl = ttl;
		}
	}
}

void *getAdvs(void *b) {
	int server_socket, client_socket, no_of_bytes;
	struct sockaddr_in server_address, client_address;
	socklen_t client_length;
	char buffer[BUFFER_SIZE];
	char from_ip[INET_ADDRSTRLEN]; // holds ip address of node from which adv was received
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

		inet_ntop(AF_INET, &client_address.sin_addr, from_ip, INET_ADDRSTRLEN);// << endl;
		struct hostent *hp;
		hp = gethostbyaddr(&client_address.sin_addr, sizeof(client_address.sin_addr), AF_INET);
		cout << "\nReceived from: (" << getIndexFromAddr(from_ip) << ") " << from_ip << " " << hp->h_name << endl;
        cout << buffer << endl;
                
        pthread_mutex_lock(&lock);
		processAdv(buffer, from_ip);
		pthread_mutex_unlock(&lock);
	}
	return NULL;
}

void processAdv(char* recdadv, char* heard_from_ip){
	
	int heard_from_index = getIndexFromAddr(heard_from_ip);
	if(heard_from_index == -1 || heard_from_index == node_count || !is_neighbour[heard_from_index]){
		/* got an advertisement from a non-neighbour or node a registered node*/
		return;
	}

	// restoring all fields of the node from which ad was received
	rtable[heard_from_index].ttl = ttl;
	rtable[heard_from_index].cost = 1;
	hostnameToIp(nodes[heard_from_index].c_str(), &rtable[heard_from_index].nexthop);
	graph[0][heard_from_index] = 1;

	// now updating the graph
    bool graph_updated = false;

    char token_ip[20], token_cost[20];
    char *temp_str;

    temp_str = strtok (recdadv,",;");

    while (temp_str != NULL)	{
    	strcpy(token_ip, temp_str);
    	string destination(token_ip);
    	
    	temp_str = strtok (NULL, ",;");
    	strcpy(token_cost, temp_str);
    	string costtodestination(token_cost);
    	
    	graph_updated =  updateGraph(heard_from_index, destination, costtodestination) | graph_updated;
    	
        temp_str = strtok (NULL, ",;");
    }

    // for all nodes in rtable which have as nexthop, the node from which adv was recd, restore TTL, unless new cost = infinity
    // (remaining nodes which are connected through the node which advertised)
    
    char temp_ip[INET_ADDRSTRLEN];
    for(int i = 1; i < node_count; i++){
        inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, temp_ip, INET_ADDRSTRLEN);
        if((strcmp(temp_ip, heard_from_ip) == 0) && graph[heard_from_index][i] != infinity){
            rtable[i].ttl = ttl;
        }
    }

    if(graph_updated){
    	displayGraph();
    	cout << "Graph was updated.\n";
    	if(updateRoutingTable(getIndexFromAddr(heard_from_ip))){
    		displayRoutingTable();
    		cout << "Table was updated based on adv. received." << endl;
    		sendAdv();
    	}
    }
}

bool updateGraph(int heard_from_index, string destination, string costtodestination){
	//char twmp[INET_ADDRSTRLEN];
	//cout << "updateGraph() " << heard_from_index << " " << inet_ntop(AF_INET, &rtable[heard_from_index].destadr.sin_addr, twmp, INET_ADDRSTRLEN) << endl;
	//cout << "dest " << destination << endl;					

	// fills up the corresponding cell in the matrix
	char temp_ip[INET_ADDRSTRLEN];
	int to_index = -1;
	for(int i = 1; i < node_count; i++){
		inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, temp_ip, INET_ADDRSTRLEN);
		if(strcmp(temp_ip, destination.c_str()) == 0){
			to_index = i;
			break;
		}
	}
	if(to_index == -1){
		return false;
	}

	int cost = stoi(costtodestination);
	
	bool cost_changed = (graph[heard_from_index][to_index] != cost);
	if(cost_changed){
		graph[heard_from_index][to_index] = cost;
	}
	
	return cost_changed;
}


bool updateRoutingTable(int updated_row){
	cout << "Row updated was: " << updated_row << endl;

	bool table_updated = false;

	for(int i = 1; i < node_count; i++){
		if(updated_row == i){continue;}

		char temp_ip[INET_ADDRSTRLEN];
		bzero(temp_ip, INET_ADDRSTRLEN);

		int new_cost = graph[updated_row][i] + graph[0][updated_row];

		if( (getIndexFromAddr(inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, temp_ip, INET_ADDRSTRLEN)) == updated_row) 
			&& !is_neighbour[updated_row]){
			// got adv that a node's cost has incr, and if we are routing to that node via whom we heard the adv from

			graph[0][i] = graph[updated_row][i] + graph[0][updated_row];

			rtable[i].cost = graph[0][i];
			
			// the destination node should not be a neighbor ,in order to prevent a loop
			if(!is_neighbour[i]){
				rtable[i].ttl = ttl;
			}
			continue;
		}


		// cost to dest + cost to source < own cost to dest
		
		if(new_cost < graph[0][i] && new_cost < infinity){
			graph[0][i] = new_cost;
			
			rtable[i].cost = new_cost;
			rtable[i].ttl = ttl;
			hostnameToIp(nodes[updated_row].c_str(), &rtable[i].nexthop);
			table_updated = true;
			continue;
		}

		
	}	

	return table_updated;	
}

bool updateRoutingTable(){
	bool table_updated = false;

	int old_costs[node_count];
	int cost_from[node_count];
	for(int i = 1; i < node_count; i++){
		old_costs[i] = graph[0][i];
		graph[0][i] = infinity;
		cost_from[i] = 0;
	}

	for(int row = 1; row < node_count; row++){
		if(!is_neighbour[row]){continue;}
		for(int i = 1; i < node_count; i++){
			if( row == i || is_neighbour[i] ){ continue; }
			
			int new_cost = graph[row][i] + old_costs[row];
			
			if(new_cost < graph[0][i]){
				graph[0][i] = new_cost;
				cost_from[i] = row;
			}
		}	
	}

	for(int i = 1; i < node_count; i++){
		if(old_costs[i] < graph[0][i]){
			graph[0][i] = old_costs[i];
		}
		else{
			if(is_neighbour[i]){continue;}
			rtable[i].cost = graph[0][i];
			rtable[i].ttl = ttl;
			if(cost_from[i] == 0){
				rtable[i].nexthop.sin_addr.s_addr = 0;	
			}
			else{
				hostnameToIp(nodes[cost_from[i]].c_str(), &rtable[i].nexthop);	
			}
			table_updated = true;
		}
	}

	return table_updated;	
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

string makeAdv(char *recr_ip){
	char ipadd[INET_ADDRSTRLEN];
	char next_ip[INET_ADDRSTRLEN];

	string adv = "";
	
	for(int i = 1; i < node_count; i++){
		inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, next_ip, INET_ADDRSTRLEN);
		if(shflag && (strcmp(recr_ip, next_ip) == 0)){
			// implies route has been learnt from node to which Adv will be sent to
			// so skip this entry
			continue;
		}

		adv.append(inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, ipadd, INET_ADDRSTRLEN));
		adv.append(",");
		
		adv.append(to_string(rtable[i].cost));
		adv.append(";");
	}
	return adv;
}

void sendAdv(){
    int udp_socket, no_of_bytes;
    struct sockaddr_in server_address;
    struct hostent *server;
    socklen_t len;

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
       std::cout << "Error opening socket!\n";
       return;
    }
    
    for(int i = 1; i < node_count; i++){
        if(is_neighbour[i]){
            char temp_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, temp_ip, INET_ADDRSTRLEN);

            string adv = makeAdv(temp_ip);
                     

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

int getIndexFromAddr(const char* heard_from_ip){
	char temp_ip[INET_ADDRSTRLEN];
	int heard_from_index = -1;
	// to find index of node from which advertisement was heard from
    for(int i = 1; i < node_count; i++){
        inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, temp_ip, INET_ADDRSTRLEN);
        if(strcmp(temp_ip, heard_from_ip) == 0 && is_neighbour[i]){
            heard_from_index = i;
            break;
        }
    }
    return heard_from_index;
}

void displayGraph(){
	//cout << "\033[H\033[2J";
	cout << endl;
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
		cout << nodes[i];
		printf("\n");
	}
}

void displayRoutingTable(){
		
	cout << "\nRouting table:\n";

	printf("%3s %-15s %-15s %-4s %4s\n", "Num", "Node", "Next", "Cost", "TTL");

	for(int i = 0; i < node_count; i++){
		char destadd[INET_ADDRSTRLEN];
		char nextadd[INET_ADDRSTRLEN];

		printf("%3d %-15s %-15s %4d %4d ", 	
										i,
										inet_ntop(AF_INET, &rtable[i].destadr.sin_addr, destadd, INET_ADDRSTRLEN),
										inet_ntop(AF_INET, &rtable[i].nexthop.sin_addr, nextadd, INET_ADDRSTRLEN),
										rtable[i].cost,
										rtable[i].ttl );
		cout << nodes[i] << endl;
	}
}
