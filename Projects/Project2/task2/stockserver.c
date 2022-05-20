/* 
 * stockserver.c - An stock server 
 */ 
/* $begin stockserverimain */
#include "csapp.h"

#define SBUFSIZE 128
#define NTHREADS 64

void echo(int connfd);

typedef struct {
	int *buf;		/* Buffer array */
	int n;			/* Maximum number of slots */
	int front;		/* buf[(front + 1) % n] is first item */
	int rear;		/* buf[rear % n] is last item */
	sem_t mutex;	/* Protects accesses to buf */
	sem_t slots;	/* Counts available slots */
	sem_t items;	/* Counts available items */
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

struct item {	//structure for each stock data
	int ID;
	int left_stock;
	int price;
	int readcnt;
	sem_t mutex, w;
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

void buy(int connfd, tree_pointer ptr, int id, int num);
void sell(int connfd, tree_pointer ptr, int id, int num);
void show(int connfd, tree_pointer ptr);
char showBuf[MAXLINE];
void bufClear() {
	for(int i = 0; i < MAXLINE; i++) {
		showBuf[i] = '\0';
	}
}

sbuf_t sbuf;	/* Shared buffer of connected descriptors */

void *thread(void *vargp);
void echo_cnt(int connfd);
static int byte_cnt;
static sem_t mutex;
static void init_echo_cnt(void);
int main(int argc, char **argv) 
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;

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
	inorder(root);//test
    listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	for(i = 0; i < NTHREADS; i++)	/* Create worker threads */
		Pthread_create(&tid, NULL, thread, NULL);
    while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd);	/* Insert connfd in buffer */
	    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
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
		ptr->data.readcnt = 0;
		Sem_init(&ptr->data.mutex, 0, 1);
		Sem_init(&ptr->data.w, 0, 1);
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

void show(int connfd, tree_pointer ptr) {
	char buf[MAXLINE];
	if(ptr) {
		show(connfd, ptr->left);
		
		P(&ptr->data.mutex);
		ptr->data.readcnt++;
		if(ptr->data.readcnt == 1)
			P(&ptr->data.w);
		V(&ptr->data.mutex);
		sprintf(buf, "%d %d %d\n", ptr->data.ID, ptr->data.left_stock, ptr->data.price);
		strcat(showBuf, buf);
		P(&ptr->data.mutex);
		ptr->data.readcnt--;
		if(ptr->data.readcnt == 0)
			V(&ptr->data.w);
		V(&ptr->data.mutex);

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
	P(&ptr->data.w);
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
	V(&ptr->data.w);
}
void sell(int connfd, tree_pointer ptr, int id, int num) {
	ptr = search(ptr, id);
	P(&ptr->data.w);
	ptr->data.left_stock += num;
	char buf[MAXLINE];
	sprintf(buf, "[sell] success\n");
	Rio_writen(connfd, buf, MAXLINE);
	V(&ptr->data.w);
}
void updateFile(FILE* fp, tree_pointer ptr) {
	if(ptr) {
		updateFile(fp, ptr->left);

		P(&ptr->data.mutex);
		ptr->data.readcnt++;
		if(ptr->data.readcnt == 1)
			P(&ptr->data.w);
		V(&ptr->data.mutex);

		fprintf(fp, "%d %d %d\n", ptr->data.ID, ptr->data.left_stock, ptr->data.price);

		P(&ptr->data.mutex);
		ptr->data.readcnt--;
		if(ptr->data.readcnt == 0)
			V(&ptr->data.w);
		V(&ptr->data.mutex);

		updateFile(fp, ptr->right);
	}
}

void sbuf_init(sbuf_t *sp, int n) {
	sp->buf = Calloc(n, sizeof(int));
	sp->n = n;					/* Buffer holds max of n items */
	sp->front = sp->rear = 0;	/* Empty buffer iff front == rear */
	Sem_init(&sp->mutex, 0, 1);	/* Binary semaphore for locking */
	Sem_init(&sp->slots, 0, n);	/* Initially, buf has n empty slots */
	Sem_init(&sp->items, 0, 0);	/* Initially, buf has 0 items */
}
void sbuf_deinit(sbuf_t *sp) {
	Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item) {
	P(&sp->slots);							/* Wait for available slot */
	P(&sp->mutex);							/* Lock the buffer */
	sp->buf[(++sp->rear) % (sp->n)] = item;	/* Insert the item */
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->items);							/* Announce available item */
}
int sbuf_remove(sbuf_t *sp) {
	int item;
	P(&sp->items);							/* Wait for available item */
	P(&sp->mutex);							/* Lock the buffer */
	item = sp->buf[(++sp->front)%(sp->n)];	/* Remove the item */
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->slots);							/* Announce available slot */
	return item;
}

void *thread(void *vargp) {
	Pthread_detach(pthread_self());
	while(1) {
		int connfd = sbuf_remove(&sbuf);	/* Remove connfd from buf */
		echo_cnt(connfd);					/* Service client */
		Close(connfd);
	}
}

static void init_echo_cnt(void) {
	Sem_init(&mutex, 0, 1);
	byte_cnt = 0;
}

void echo_cnt(int connfd) {
	int n;
	char buf[MAXLINE];
	rio_t rio;
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	char command[5];
	int sID = 0; int sNum = 0;
	Pthread_once(&once, init_echo_cnt);
	bufClear();
	Rio_readinitb(&rio, connfd);
	while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		command[0] = 0; sID = 0; sNum = 0;
		sscanf(buf, "%s %d %d", command, &sID, &sNum);
		//P(&mutex);
		//byte_cnt += n;
		printf("%d bytes\n", n);
		//V(&mutex);
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
		else if(!strcmp(command, "exit")) {
			bufClear();
			break;
		}
		FILE* fp = fopen("stock.txt", "w");
		updateFile(fp, root);
		fclose(fp);
		bufClear();
	}
}
