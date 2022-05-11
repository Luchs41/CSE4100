/* 
 * stockserver.c - An stock server 
 */ 
/* $begin stockserverimain */
#include "csapp.h"

void echo(int connfd);
typedef struct {	/* Represents a pool of connected descriptors */
	int maxfd;			/* Largest descriptor in read_set */
	fd_set read_set;	/* Set of all active descripotors */
	fd_set ready_set;	/* Subset of descripotrs ready for reading */
	int nready;			/* Number of ready descriptors from select */
	int maxi;			/* High water index into client array */
	int clientfd[FD_SETSIZE];		/* Set of active descripotrs */
	rio_t clientrio[FD_SETSIZE];	/* Set of active read buffers */
} pool;

struct item {	//structure for each stock data
	int ID;
	int left_stock;
	int price;
	int readcnt;
	sem_t mutex;
};

struct node {
	struct item data;
	struct node *left, *right;
};
typedef struct node *tree_pointer;

tree_pointer insertNode(tree_pointer ptr, int newID, int newLeft, int newPrice);
void inorder(tree_pointer ptr);
void updateFile(FILE* fp, tree_pointer ptr);
tree_pointer search(tree_pointer ptr, int id);
tree_pointer root = NULL;

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);


void buy(int connfd, tree_pointer ptr, int id, int num);
void sell(int connfd, tree_pointer ptr, int id, int num);
void show(int connfd, tree_pointer ptr);
char showBuf[MAXLINE];
void bufClear() {
	for(int i = 0; i < MAXLINE; i++) {
		showBuf[i] = '\0';
	}
}
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	static pool pool;	

	FILE* fp = fopen("stock.txt", "r");
	if(!fp) {
		fprintf(stderr, "stock data file does not exist. \n");
		exit(0);
	}

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }
	
	// read from stock.txt file
	int id, left, price;
	while(!feof(fp)) {
		if(fscanf(fp, "%d %d %d", &id, &left, &price) == EOF) ;
		root = insertNode(root, id, left, price);
	}
	fclose(fp);
	inorder(root);
	// end of reading data
	
    listenfd = Open_listenfd(argv[1]);
	init_pool(listenfd, &pool);
    while (1) {
		/*clientlen = sizeof(struct sockaddr_storage); 
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		echo(connfd);
		Close(connfd);*/
		/* Wait for listening/connected descripotr(s) to become ready */
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

		/* If listening descriptor ready, add new client to pool */
		if(FD_ISSET(listenfd, &pool.ready_set)) {
			clientlen = sizeof(struct sockaddr_storage);
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(connfd, &pool);
		}
		//echo(connfd);
		/* Echo a text line from each ready connected descriptor */
		check_clients(&pool);
	}
    exit(0);
}
/* $end stockserverimain */


tree_pointer insertNode(tree_pointer ptr, int newID, int newLeft, int newPrice) {
	if(ptr == NULL) {
		ptr = (tree_pointer)malloc(sizeof(*ptr));
		ptr->data.ID = newID;
		ptr->data.left_stock = newLeft;
		ptr->data.price = newPrice;
		ptr->left = NULL;
		ptr->right = NULL;
	}
	else if(ptr->data.ID < newID) {
		ptr->right = insertNode(ptr->right, newID, newLeft, newPrice);
	}
	else if(ptr->data.ID > newID) {
		ptr->left = insertNode(ptr->left, newID, newLeft, newPrice);
	}
	return ptr;
}

void inorder(tree_pointer ptr) {
	if(ptr) {
		inorder(ptr->left);
		fprintf(stdout, "%d %d %d\n", ptr->data.ID, ptr->data.left_stock, ptr->data.price);
		inorder(ptr->right);
	}
}

void init_pool(int listenfd, pool *p) {
	/* Initially, there are no connected descripotrs */
	int i;
	p->maxi = -1;
	for(i = 0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;

	/* Initially, listenfd is only member of select read set */
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p) {
	int i;
	p->nready--;
	for(i = 0; i < FD_SETSIZE; i++) /* Find an available slot */
		if(p->clientfd[i] < 0) {
			printf("add\n");//test
			/* Add connected descriptor to the pool */
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);

			/* Add the descriptor to descriptor set */
			FD_SET(connfd, &p->read_set);

			/* Update max descriptor and pool high water mark */
			if(connfd > p->maxfd)
				p->maxfd = connfd;
			if(i > p->maxi)
				p->maxi = i;
			break;
		}
	if(i == FD_SETSIZE) /* Couldn't find an empty slot */
		app_error("add_client error: Too many clients");
}

void check_clients(pool *p) {
	int i, connfd, n;
	char buf[MAXLINE];
	rio_t rio;
	char command[5];
	int sID = 0; int sNum = 0;
	for(i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];
		/* If the descriptor is ready, echo a text line from it */
		if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			p->nready--;
			if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0 && strcmp(buf, "exit\n") != 0) {
				command[0] = 0; sID = 0; sNum = 0;
				sscanf(buf, "%s %d %d", command, &sID, &sNum);
				fprintf(stdout, "%d bytes\n", n);
				if(!strcmp(command, "show")) {
					show(connfd, root);
					Rio_writen(connfd, showBuf, MAXLINE);
					bufClear();
				}
				else if(!strcmp(command, "buy")) {
					buy(connfd, root, sID, sNum);
				}
				else if(!strcmp(command, "sell")) {
					sell(connfd, root, sID, sNum);
				}
				bufClear();

			}

			/* EOF detected, remove descriptor from pool */
			else {
				//sprintf(buf, "disconnected\n");
				//Rio_writen(connfd, buf, MAXLINE);
				Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i] = -1;
				bufClear();
				FILE* fp = fopen("stock.txt", "w");
				updateFile(fp, root);
				fclose(fp);
			}
		}
	}
}

void show(int connfd, tree_pointer ptr) {
	char buf[MAXLINE];
	if(ptr) {
		show(connfd, ptr->left);
		sprintf(buf, "%d %d %d\n", ptr->data.ID, ptr->data.left_stock, ptr->data.price);
		strcat(showBuf, buf);
		show(connfd, ptr->right);
	}

}

tree_pointer search(tree_pointer ptr, int id) {
	while(ptr && ptr->data.ID != id) {
		if(ptr->data.ID > id) ptr = ptr->left;
		else if(ptr->data.ID < id) ptr = ptr->right;
	}
	return ptr;
}

void buy(int connfd, tree_pointer ptr, int id, int num) {
	ptr = search(ptr, id);
	char buf[MAXLINE];
	if(ptr->data.left_stock >= num) {
		ptr->data.left_stock -= num;
		sprintf(buf, "[buy] success\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
	else {
		sprintf(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, MAXLINE);
	}
	
}
void sell(int connfd, tree_pointer ptr, int id, int num) {
	ptr = search(ptr, id);
	ptr->data.left_stock += num;
	char buf[MAXLINE];
	sprintf(buf, "[sell] success\n");
	Rio_writen(connfd, buf, MAXLINE);
}
void updateFile(FILE* fp, tree_pointer ptr) {
	if(ptr) {
		updateFile(fp, ptr->left);
		fprintf(fp, "%d %d %d\n", ptr->data.ID, ptr->data.left_stock, ptr->data.price);
		updateFile(fp, ptr->right);
	}
}
