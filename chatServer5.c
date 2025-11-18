#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h> 

struct user {
	int clisockfd;
	char nickname[MAX]; 
	LIST_ENTRY(user) users;
};

LIST_HEAD(userhead, user);

int check_nicknames(struct userhead *head, char *nickname){
	
	// temporary buffer to measure nickname with snprintf
    char nickname_temp[MAX];
    int nickname_len = snprintf(nickname_temp, MAX, "%s", nickname);

	// if the proposed username is empty or beyond big
	if( nickname_len <= 0 || nickname_len > MAX-1 ){
			return 0; // bad format/length --> false
	}

	// if the proposed username starts with one of our delimiters
	if( nickname[0] == '%' ){
			return 0; //bad name
	}

	//check  for duplicates by looping
	struct user *u;
	LIST_FOREACH(u, head, users) {
		if(strncmp(u->nickname, nickname, MAX) == 0){
			return 0; // duplicate found --> false
		}
	}

	return 1; //true
}

void tell_everyone(struct userhead *head, struct user *speaking_user, char* message){
	struct user *otheruser;
	LIST_FOREACH(otheruser, head, users){
		if(otheruser->clisockfd != speaking_user->clisockfd && otheruser->nickname[0] != '\0'){
			if (write(otheruser->clisockfd, message, MAX) < 0) {
				perror("server: Error writing message to all clients");
				continue;
			}
		}
	}
}

void user_disconnect(struct user *u, int *num_users) {
    close(u->clisockfd);
    LIST_REMOVE(u, users);
    (*num_users)--;
    free(u);
}

int main(int argc, char **argv)
{
	int	   newsockfd, clisockfd; 
	struct sockaddr_in cli_addr, serv_addr, dir_serv_addr;
	fd_set readset;

	// ADDED Linked list stuff
	int	   num_users = 0;
	struct userhead head;
	LIST_INIT(&head);

	/* Create communication endpoint */
	int sockfd;			/* Listening socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Add SO_REUSEADDRR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("server: can't set stream socket address reuse option");
		return EXIT_FAILURE;
	}

	/* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family 		= AF_INET;
	serv_addr.sin_addr.s_addr 	= htonl(INADDR_ANY);
	serv_addr.sin_port			= NULL; // FIXME --> fixed after argument parsing for port

	/*Get the topic and port from input arguments */
	char topic[MAX] = {'\0'};
	int port;

	switch(argc){
		case 0:
			break;
		case 1:
			// Got no args: I think just the server arg (./chatServer2) 
			fprintf(stderr, "Error: Missing topic and port. Start server like this: %s <\"topic\"> <port>\n", argv[0]);
			return EXIT_FAILURE;
		case 2:
			// Got one arg: the server topic (no port) 
			fprintf(stderr, "Error: Missing topic and port. Start server like this: %s <\"topic\"> <port>\n", argv[0]);
			return EXIT_FAILURE;
		case 3:
			// Got all args: server + port 
			snprintf(topic, MAX, "%s", argv[1]); // topic is set :)
			

			if (sscanf(argv[2], "%d", &port) != 1) {
				fprintf(stderr, "Invalid port number: %s\n", argv[2]);
				return EXIT_FAILURE;
			}

			if (port < 49152 || port > 65535) {
				fprintf(stderr, "Port number out of range (49152 - 65535): %d\n", port);
				return EXIT_FAILURE;
			}

			printf("Starting Chat Server with topic '%s' on port %d\n", topic, port);
			break;
		default:
			// More/less args than needed, give catch-all error
			fprintf(stderr, "Error: Invalid number of arguments: Actual: %d, Expected 3. Start server like this: %s <\"topic\"> <port>\n", argc, argv[0]);
			return EXIT_FAILURE;
	}

	serv_addr.sin_port = htons(port);

	/* Bind to local IP/port before registering with the Directory Server to ensure
	* that the port is available */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("server: can't bind local address");
		return EXIT_FAILURE;
	}

	/* Create communication endpoint */
	int dir_sockfd;
	if ((dir_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Bind socket to local address */
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family 		= AF_INET;
	dir_serv_addr.sin_addr.s_addr	= inet_addr(SERV_HOST_ADDR);	/* hard-coded in inet.h */
	dir_serv_addr.sin_port			= htons(SERV_TCP_PORT);			/* hard-coded in inet.h */

	/* Connect to the directory server. */
	if (connect(dir_sockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) {
		perror("server: can't connect to directory server");
		return EXIT_FAILURE;
	}

	/*Register with the directory server */
	char reg_msg[MAX];
	snprintf(reg_msg, MAX, "S %s,%d\n", topic, port);

	if (write(dir_sockfd, reg_msg, MAX) < 0) {
		perror("server: Error writing register message to directory server");
		close(dir_sockfd);
		return EXIT_FAILURE;
	}
	
	char response[MAX] = {'\0'};
	if (read(dir_sockfd, response, MAX) <= 0) {
		perror("server: Error reading register response from directory server");
		close(dir_sockfd);
		return EXIT_FAILURE;
	} 
	if (response[0] == 'X'){
		printf("Chat Server with topic '%s' on registered on port %d\n", topic, port);
	}
	else{
		printf("Directory Server failed to register Chat Server: %s\n", response);
		close(dir_sockfd);
		return EXIT_FAILURE;
	}

	/* Now you are ready to accept client connections */
	listen(sockfd, 5);

	for (;;) {

		/* Initialize and populate your readset and compute maxfd */
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		int max_fd = sockfd; /* We won't write to a listening socket so no need to add it to the writeset */

		/* Populate readset with ALL your client sockets */
		struct user *u;
		LIST_FOREACH(u, &head, users) { 
			FD_SET(u->clisockfd, &readset);
			if (u->clisockfd > max_fd) { max_fd = u->clisockfd; } /* Compute max_fd as you go */
		}

		if (select(max_fd+1, &readset, NULL, NULL, NULL) > 0) {

			/* Check to see if our listening socket has a pending connection */
			if (FD_ISSET(sockfd, &readset)) {
				
				/* Accept a new connection request */
				socklen_t clilen = sizeof(cli_addr);
				newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
				if (newsockfd < 0) {
					perror("server: accept error");
					close(newsockfd);
					return EXIT_FAILURE;
				}
				
				// Make a new "user" which we will read from later
				struct user* nuser = calloc(1, sizeof(*nuser));
				nuser->clisockfd = newsockfd;
				snprintf(nuser->nickname, MAX, ""); 
				LIST_INSERT_HEAD(&head, nuser, users);
				
				// give the user a welcome prompt to let them know their first message should be their nickname?
				char s[MAX] = {'\0'};
				char s3[MAX] = {'\0'};

				snprintf(s, MAX, "\n--Welcome to the chatroom!--");
				snprintf(s3, MAX, "Enter your username: ");
				
				//write messages but don't close (that would be rude)
				if (write(newsockfd, s, MAX) < 0) {
					perror("server: Error sending the user the welcome message");
					close(newsockfd);
					
					user_disconnect(nuser, &num_users);
					
					return EXIT_FAILURE;
				}
				if (write(newsockfd, s3, MAX) < 0) {
					perror("server: Error sending the user the username entry message");
					close(newsockfd);
					user_disconnect(nuser, &num_users);
					return EXIT_FAILURE;
				}

			}

			/*Check ALL your client sockets */
			// Aiming to loop over all of 'em
			struct user *u = LIST_FIRST(&head);
			while (u != NULL) {
				struct user *next = LIST_NEXT(u, users); // store next first

				if (FD_ISSET(u->clisockfd, &readset)) {

					/* Read the request from the client */
					char message[MAX] = {'\0'};
					ssize_t nread = read(u->clisockfd, message, MAX);
					
					// if the read fails or is error
					if (nread <= 0) {
						if(u->nickname[0] != '\0'){
							snprintf(message, MAX, "-- %s has left the chat --\n", u->nickname);
							tell_everyone(&head, u, message);
						}
						user_disconnect(u, &num_users);
						u = next;
						continue;
					}
					
					// We check if the user is a new user by checking if they have the placeholder empty name
					// if so, we can "loop" them by intercepting their message until they have an ok one
					if(u->nickname[0] == '\0'){
						
						char joined[MAX] = {'\0'};
						
						//we check the entered message to see if it's a valid username?
						if (check_nicknames(&head, message + 1)){

							// Way of marking that the user is "connected", 
							// we do it here as we want the first valid login. not accepted connection
							num_users++;
							snprintf(u->nickname, MAX, message + 1);
							
							char directions[MAX] = {'\0'};
							snprintf(directions, MAX, "--Type and enter to send messages. Begin msg with '%%' to quit--\n");
							
							if (write(u->clisockfd, directions, MAX) < 0) {
								perror("server: Error sending the user the messaging rules");
								user_disconnect(u, &num_users);
								u = next;
								continue;
							}
							
							// The first user (and only the first user) to log in should receive the message
							// "You are the first user to join the chat".
							// value is one; checking if it's empty wouldn't work because we added the first user
							if (num_users == 1){
								snprintf(joined, MAX, "-- You are the first user to join the chat --\n");
								if (write(u->clisockfd, joined, MAX) < 0) {
									perror("server: Error sending the user the first joined message");
									user_disconnect(u, &num_users);
									u = next;
									continue;
								}
							}  
							else { //tell all pre-existing users someone has joined
								snprintf(joined, MAX, "-- %s has joined the chat --\n", u->nickname);
								//somehow broadcast to everyone -- ask whether it should go to new user too
								tell_everyone(&head, u, joined);
							}
						}
						else{
							// else if username sucks
							// write to that user that it sucks or won't work
							snprintf(joined, MAX, "Provided name is invalid. Please enter a new one.\n");
							if (write(u->clisockfd, joined, MAX) < 0) {
								perror("server: Error sending the user the first invalid username message");
								user_disconnect(u, &num_users);
								u = next;
								continue;
							}
						}
					}
					else{
						
						// optional disconnect from server
						if (message[1] == '%') {
							
							// Notify everyone in the chat that the user has left (e.g., "Dan has left the chat") 
							char leave_message[MAX] = {'\0'};
							snprintf(leave_message, MAX, "-- %s has left the chat --\n", u->nickname);
							
							tell_everyone(&head, u, leave_message);

							// disconnect user
							user_disconnect(u, &num_users);
							u = next;
							continue;
						}
						/* Nickname is valid, we thus enter proper message handling */
						else if (message[0] == '!') {
							// seperate string
							char actual_message[MAX] = {'\0'};
							snprintf(actual_message, MAX, "%s: %s\n", u->nickname, message + 1);
							
							// Tell everyone
							tell_everyone(&head, u, actual_message);
						}
						// error case, tell the user that their message sucks
						else {
							//write an error message
							char error_message[MAX] = {'\0'};
							snprintf(error_message, MAX, "-- ERROR Could not process request --\n-- Begin messages with '!' or enter '%%' to quit --\n");
							
							//send it to the specific user 
							if (write(u->clisockfd, error_message, MAX) < 0) {
								perror("server: Error send the user the invalid messaging identifier error");
								user_disconnect(u, &num_users);
								u = next;
								continue;
							}
						}
					}
				}
				// moves it to the next client to handle
				u = next;
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
				perror("server: select");
				exit(EXIT_FAILURE); 
			}
		}
	}
}