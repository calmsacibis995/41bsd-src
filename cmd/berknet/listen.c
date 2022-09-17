/*
	listen.c

	listen for a packet from the program interact.c and print it
*/
# include "defs.h"
main(argc,argv)
  char **argv;
  {
	struct packet *pp;
	char buf[BUFSIZ];
	setupdaemon(argc,argv);
	putchar('!');
	while(getreset() == BROKENREAD);
	printf("got reset\n");
	for(;;){
		pp = getpacket();
		printpacket(pp,buf);
		printf("got %s\n",buf);
		if(pp == NULL || pp->pcode == RESET)continue;
		pp->pcode = ACK;
		pp->len = 0;
		sendpacket(pp);
		printpacket(pp,buf);
		printf("sent %s\n",buf);
		}
	}
