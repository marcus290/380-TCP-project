/**
 * Struct for storing data of the tracefile line.
 */
struct packet {
	unsigned long seqNum;
	double timeStamp;
	unsigned long payloadSize;
	int syn;
	int fin;
	uint64_t connID;
};

struct connection {
	unsigned long sourceIP;
	unsigned long sourcePort;
	unsigned long destIP;
	unsigned long destPort;
};

struct node {
    uint64_t connID;
    struct node* next;
};
