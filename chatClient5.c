#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"

int				sockfd;
struct sockaddr_in chat_serv_addr, dir_serv_addr;

int main()
{
	char s[MAX] = {'\0'};
	fd_set			readset;

	/* Set up the address of the directory server. */
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family			= AF_INET;
	dir_serv_addr.sin_addr.s_addr	= inet_addr(SERV_HOST_ADDR);	/* hard-coded in inet.h */
	dir_serv_addr.sin_port			= htons(SERV_TCP_PORT);			/* hard-coded in inet.h */

	/* Create a socket (an endpoint for communication). */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Connect to the server. */
	if (connect(sockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) {
		perror("client: can't connect to server");
		return EXIT_FAILURE;
	}

    // Request servers
    if (write(sockfd, "C\n", 2) < 0) {
        perror("client: failed to send list request");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

	// Read in the number of servers at the current point in time
	int num_servers = 0;
	if (read(sockfd, &num_servers, MAX) <= 0) {
		perror("client: failed to read number of servers");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	// Read in the list of servers at the current point in time
	char srv_list[MAX * MAX_CLIENTS] = {'\0'};
    ssize_t nread = read(sockfd, srv_list, sizeof(srv_list) - 1);
    if (nread <= 0) {
        perror("client: failed to read response");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

	if (num_servers <= 0 || strncmp(srv_list, "No active chat rooms.\n", MAX) == 0) {
		printf("There are no active chat rooms right now.\n");
		close(sockfd);
		exit(EXIT_SUCCESS);
	}

    printf("\n%s\n", srv_list);
    close(sockfd);

	//Get the choice from the user
	printf("Enter the number of the chat room to join(1 - %d): ", num_servers);

	int choice = 0;
	int c;
	while (scanf("%d", &choice) != 1 || choice < 1 || choice > num_servers) {
		fprintf(stderr, "Invalid selection.\n");
		printf("Enter the number of the chat room to join: ");
		while ((c = getchar()) != '\n' && c != EOF);  // clear invalid input
	}

    //Parse the chosen server from the printed list
	char topic[MAX] = {'\0'};
	char ip[MAX] = {'\0'};
	int port;
	int idx = 0;

	const char *next_srv = srv_list;
	char line[MAX];

	while (sscanf(next_srv, " %[^\n]\n", line) == 1) {
		int number, temp_port;
		char temp_topic[MAX] = {'\0'};
		char temp_ip[MAX] = {'\0'};
		if (sscanf(line, "  [%d] %[^'(] (%[^:]:%d)", &number, temp_topic, temp_ip, &temp_port) == 4) {
			if(number == choice){
				snprintf(topic, MAX, "%s", temp_topic);
				snprintf(ip, MAX, "%s", temp_ip);
				port = temp_port;
				break;
			}
		}
		
		int len = (int)strnlen(line, MAX);
		next_srv += len + 1;  // skip this line & nl (I think)
		
		if (*next_srv == '\0'){
			break;
		} 
	}

    printf("\nNow joining...: '%s' at %s:%d\n", topic, ip, port);

	/* Set up the address of the directory server. */
	memset((char *) &chat_serv_addr, 0, sizeof(chat_serv_addr));
	chat_serv_addr.sin_family			= AF_INET;
	chat_serv_addr.sin_addr.s_addr 		= inet_addr(ip);
	chat_serv_addr.sin_port				= htons(port);

	/* Create a socket (an endpoint for communication). */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		return EXIT_FAILURE;
	}

	/* Connect to the server. */
	if (connect(sockfd, (struct sockaddr *) &chat_serv_addr, sizeof(chat_serv_addr)) < 0) {
		perror("client: can't connect to server");
		return EXIT_FAILURE;
	}

	for(;;) {

		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(sockfd, &readset);

		if (select(sockfd+1, &readset, NULL, NULL, NULL) > 0)
		{
			/* Check whether there's user input to read */
			if (FD_ISSET(STDIN_FILENO, &readset)) {
				if (1 == scanf(" %[^\n]", s)) {
					/* Send the user's message to the server */
					char message[MAX] = {'\0'};
					snprintf(message, MAX, "!%s", s);

					if (write(sockfd, message, MAX) < 0) {
						perror("client: Error send the user's message to the server");
						close(sockfd);
						return EXIT_FAILURE;
					}
					printf("\n");
				} else {
					fprintf(stderr, "%s:%d Error reading or parsing user input\n", __FILE__, __LINE__); //DEBUG
				}
			}

			/* Check whether there's a message from the server to read */
			if (FD_ISSET(sockfd, &readset)) {
				ssize_t nread = read(sockfd, s, MAX);
				if (nread <= 0) {
					/* Not every error is fatal. Check the return value and act accordingly. */
					fprintf(stderr, "%s:%d Error reading from server\n", __FILE__, __LINE__); //DEBUG
					return EXIT_FAILURE;
				} else {
					//fprintf(stderr, "%s:%d Read %zd bytes from server: %s\n", __FILE__, __LINE__, nread, s); //DEBUG
					fprintf(stderr, "%s\n", s); 
				}
			}
		}
	}
	close(sockfd);
	// return or exit(0) is implied; no need to do anything because main() ends
}