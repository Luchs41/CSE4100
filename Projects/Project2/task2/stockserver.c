/* 
 * stockserver.c - An stock server 
 */ 
/* $begin stockserverimain */
#include "csapp.h"

void echo(int connfd);

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
tree_pointer root = NULL;
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	

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
		fscanf(fp, "%d %d %d", &id, &left, &price);
		root = insertNode(root, id, left, price);
	}
	inorder(root);
    listenfd = Open_listenfd(argv[1]);
    while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		echo(connfd);
		Close(connfd);
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
