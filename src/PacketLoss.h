/**
 * Struct for storing data of the tracefile line.
 */
struct packet {
	unsigned long seqNum;
	double timeStamp;
	unsigned long payloadSize;
	int syn;
	int fin;
	int ping;
	int connState;
	uint64_t connID;
};

struct connStatus {
	unsigned long seqNum;
	double timeStamp;
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

struct warningNode {
    uint64_t connID;
	double timeStamp;
	unsigned long bytesMissing;
    struct warningNode* next;
};
