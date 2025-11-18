#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h> 

struct connection {
  int consockfd;
  int contype;
  char con_ip[MAX];
  char topic[MAX];
  int port;
  LIST_ENTRY(connection) connections;
};
LIST_HEAD(conhead, connection);

int main(int argc, char **argv)
{	
	int	   newsockfd, clisockfd; 
	struct sockaddr_in cli_addr, serv_addr;
	fd_set readset;

	struct conhead head;
	LIST_INIT(&head);

	/* Create communication endpoint */
	int sockfd;			/* Listening socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("directory server: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Add SO_REUSEADDRR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("directory server: can't set stream socket address reuse option");
		return EXIT_FAILURE;
	}

	/* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port		= htons(SERV_TCP_PORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("directory server: can't bind local address");
		return EXIT_FAILURE;
	}

	listen(sockfd, 5);

	for (;;) {

		/* Initialize and populate your readset and compute maxfd */
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		int max_fd = sockfd; /* We won't write to a listening socket so no need to add it to the writeset */

		/* Populate readset with all connection sockets */
		struct connection *con;
		LIST_FOREACH(con, &head, connections) { 
			FD_SET(con->consockfd, &readset);
			if (con->consockfd > max_fd) { max_fd = con->consockfd; } /* Compute max_fd as you go */
		}

		/* The humble select call */
		if (select(max_fd+1, &readset, NULL, NULL, NULL) > 0) {
			
			if (FD_ISSET(sockfd, &readset)) {
				/* Accept a new connection request */
				socklen_t clilen = sizeof(cli_addr);
				int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
				if (newsockfd < 0) {
					perror("directory server: accept error");
					close(newsockfd);
					continue; /* We can ignore this */
				}
				
				// Make a new connection which we will read from later
				struct connection* nconnection = calloc(1, sizeof(*nconnection));
				nconnection->consockfd = newsockfd;
				nconnection->contype = 0; // unknown type of connection
				LIST_INSERT_HEAD(&head, nconnection, connections);

				// Debug line to check connection?
				//fprintf(stderr, "%s:%d Accepted connection from %s\n", __FILE__, __LINE__, inet_ntoa(cli_addr.sin_addr));
			}

			struct connection *con = LIST_FIRST(&head);
			while (con != NULL) {
				struct user *next = LIST_NEXT(con, connections); // store next first

				if (FD_ISSET(con->consockfd, &readset)) {

					/* Read the request */
					char message[MAX] = {'\0'};
					ssize_t nread = read(con->consockfd, message, MAX);
					
					// if the read fails or is error
					if (nread <= 0) {				
						
						if(con->contype == 1){
							fprintf(stderr, "De-registered chat server: %s (%s:%d)\n", con->topic, con->con_ip, con->port);
						}
						
						close(con->consockfd);
						LIST_REMOVE(con, connections);
						free(con);
						con = next;
						continue;
					}

					// Check the type of the connection (if any)
					switch (con->contype){
						case 0: {
							if (message[0] == 'S') {
								// Register chat server
								char topic[MAX] = {'\0'};
								int port;
								
								// Parse server register message
								if (sscanf(message + 2, "%[^,],%d\n", topic, &port) != 2) {
									if (write(con->consockfd, "Invalid registration format\n", MAX) < 0) {
										perror("directory server: Error writing to server that registration was invalid");
									}
									close(con->consockfd);
									LIST_REMOVE(con, connections);
									free(con);
									con = next;
									continue;
								}

								// Check duplicates
								int topic_unique_check = 1;
								struct connection *existing;
								LIST_FOREACH(existing, &head, connections) {
									if (existing->contype == 1 && strncmp(existing->topic, topic, MAX) == 0) {
										topic_unique_check = 0;
										break;
									}
								}
								
								// if not unique topic: Explode it
								if (!topic_unique_check){
									if (write(con->consockfd, "Duplicate topic\n", MAX) < 0) {
										perror("directory server: Error writing to server that topic was duplicate");
									}

									close(con->consockfd);
									LIST_REMOVE(con, connections);
									free(con);
									con = next;
									continue;
								}

								// Save as a server
								con->contype = 1;
								con->port = port;
								snprintf(con->topic, MAX, "%s", topic);
								snprintf(con->con_ip, MAX, "%s", inet_ntoa(cli_addr.sin_addr));
								fprintf(stderr, "Registered chat server: %s (%s:%d)\n", con->topic, con->con_ip, con->port);
								
								if (write(con->consockfd, "X Registered\n", MAX) < 0) {
									perror("directory server: Error writing server was registered");
									close(con->consockfd);
									LIST_REMOVE(con, connections);
									free(con);
									con = next;
									continue;
								}

							} 
							else if (message[0] == 'C') {
								// List request (Chat Client)
								con->contype = 2;
								char srv_list[MAX * MAX_CLIENTS] = {'\0'};
								int offset = 0;
								int count = 0;

								struct connection *srv;
								LIST_FOREACH(srv, &head, connections) {
									if (srv->contype == 1) {
										count++;
										offset += 
										snprintf(srv_list + offset, sizeof(srv_list) - offset,
											"  [%d] %s (%s:%d)\n", count, srv->topic, srv->con_ip, srv->port
										);
									}
								}
								
								if (count == 0) {
									offset += snprintf(srv_list + offset, sizeof(srv_list) - offset,
									"No active chat rooms.\n");
								}

								if (write(con->consockfd, &count, MAX) < 0) {
									perror("directory server: Error writing server count to client");
								}

								if (write(con->consockfd, srv_list, MAX * MAX_CLIENTS) < 0) {
									perror("directory server: Error writing list of servers to the chat client");
								}

								close(con->consockfd);
								LIST_REMOVE(con, connections);
								free(con);
								con = next;
							}
							break;
						}
						case 1: {
							// Server action logic? Check deregister? If not, nothing?
							break;
						}
						case 2: {
							// Client -- Shouldn't really get here either, as they should be re-routed and disconnected
							fprintf(stderr, "Client case entered with message %s on fd %d\n", message, con->consockfd);
							close(con->consockfd);
							LIST_REMOVE(con, connections);
							free(con);
							con = next;
							break;
						}
						default: {
							// Should never get here (in a perfect world)
							fprintf(stderr, "Unknown request '%s' made to directory server on fd %d\n", message, con->consockfd);
							close(con->consockfd);
							LIST_REMOVE(con, connections);
							free(con);
							con = next;
							break;
						}
					}

				}
				// moves it to the next connection to handle
				con = next;
			}
		}
		else {
			if (errno == EINTR) {
				// interrupted by a signal, just restart the loop
				fprintf(stderr, "%s:%d interrupted by a signal, restarting \n", __FILE__, __LINE__); //DEBUG
				continue;
				}
			else {
				fprintf(stderr, "%s:%d error with select \n", __FILE__, __LINE__); //DEBUG
				perror("directory server: select");
				exit(EXIT_FAILURE); 
			}
		}
	}
}
