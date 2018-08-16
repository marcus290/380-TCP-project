#include <stdio.h>
#include <stdint.h>

void main(int argc, char *argv[]) {
    //HashMap<Long, Integer> connectionsDict = new HashMap<>();
	//HashMap<Long, HashMap<Integer, oOSPacket>> outOfSeq = new HashMap<>(); //hashmap of packets that are out of sequence
	//ArrayList<Long> closedConnections = new ArrayList<Long>(); //record of connections which have all packets accounted for in sequence and closed

    char defaultFile[] = "trace-small.txt";
    unsigned int packetCounter = 0;
	unsigned int byteCounter = 0;
	unsigned int connectionCounter = 0;
	unsigned int missingBytes = 0;

	struct traceFileLine {
		unsigned int packetNum;
		double timeStamp;
		uint64_t connID;
	}
	;
}