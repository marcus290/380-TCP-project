import java.util.ArrayList;
import java.util.Set;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
//import java.util.Comparator;
//import java.util.regex.*;
import java.util.Scanner;

import java.io.File;
import java.io.IOException;

public class PacketLoss {
	HashMap<Long, Integer> connectionsDict = new HashMap<>();
	HashMap<Long, HashMap<Integer, oOSPacket>> outOfSeq = new HashMap<>(); //hashmap of packets that are out of sequence
	ArrayList<Long> closedConnections = new ArrayList<Long>(); //record of connections which have all packets accounted for in sequence and closed
	static String defaultFile = "trace-small.txt";
	//int counter = 0;
	int packetCounter = 0;
	int byteCounter = 0;
	int connectionCounter = 0;
	int missingBytes = 0;
		
	public static void main(String args[]) {
		String filename = (args.length > 0) ? args[0] : defaultFile;
		PacketLoss pl = new PacketLoss();
		pl.parse(filename);
	}
	
	/**
	 * Function for analysing the trace file data and identifying packet loss. Instantiates a Tracefile object containing
	 * the data and checks the sequence of packets by their TCP sequence number. If packets are out-of-sequence ("oOS"),
	 * stores them in a hashmap for further sequencing.
	 * 
	 * Prints summary information. Missing packets are identified using the oOS hashmap after processing, whereby those 
	 * remaining packets have not been sequenced successfully, indicating missing packets in the sequence.
	 * @param filename takes a filename string
	 */	
	public void parse(String filename) {
		File f = new File(filename);
		TraceData traceInput = new TraceData(f);
		for (int i = 0; i < traceInput.traceLines.size(); i++) {
			String[] line = traceInput.traceLines.get(i);
			if (line[2].isEmpty() || line[3].isEmpty() || line[4].isEmpty() || line[5].isEmpty()) //if any IP or ports are empty, skip
				continue;
			long connID = makeLongID(line);
			int seqNum = Integer.parseInt(line[13]);
			packetCounter += 1;
			byteCounter += Integer.parseInt(line[8]) + Integer.parseInt(line[11]);
			//counter += 1;
			//if (counter % 10000 == 0) 
			//	System.out.println(counter + " lines parsed.");
			//if (!outOfSeq.isEmpty()) {
			//	System.out.println("OutOfSeq packet(s) from:");
			//	for (long key: outOfSeq.keySet())
			//		System.out.println(key + " " + outOfSeq.get(key));
			//}
			
			if (connectionsDict.containsKey(connID) && seqNum == connectionsDict.get(connID)) {
				updateSeqNum(connectionsDict.get(connID), seqNum + Integer.parseInt(line[8]) + Integer.parseInt(line[11]), 
							connID, Integer.parseInt(line[14]));
				
			} else if (seqNum == 0) {//Handle new connections
				connectionCounter += 1;
				connectionsDict.put(connID, 1);
			} else { //Handle out-of-sequence packets
				if (!outOfSeq.containsKey(connID)) 
					outOfSeq.put(connID, new HashMap<>()); //add connection and new HashMap to outOfSeq HashMap
				outOfSeq.get(connID).put(seqNum, new oOSPacket(seqNum, seqNum + Integer.parseInt(line[8]) + Integer.parseInt(line[11]), Integer.parseInt(line[14]), Double.parseDouble(line[1])));
				// add TCP sequence number as key, add next TCP sequence number & connection close flag as value array
			}
		}

		/* Final printout */
		for (long closedConn: closedConnections) { //clean up duplicate packets of closed connections still in outOfSeq
			if (outOfSeq.containsKey(closedConn))
				outOfSeq.remove(closedConn);
		}
		for (Iterator<Long> conns = outOfSeq.keySet().iterator(); conns.hasNext();) { //clean up duplicate packets which are already sequenced in a connection
			long connection = conns.next();
			if (connectionsDict.containsKey(connection)) {
				for (Iterator<Integer> packets = outOfSeq.get(connection).keySet().iterator(); packets.hasNext();) {
					Integer packet = packets.next();
					if (packet < connectionsDict.get(connection)) 
						packets.remove();
				}
				if (outOfSeq.get(connection).isEmpty())
					conns.remove();
			}
		}

		System.out.println("\nIncomplete connections:");
		if (connectionsDict.isEmpty() && outOfSeq.isEmpty()) {
			System.out.println("None");
		} else {
			Set<Long> keyList = new HashSet(connectionsDict.keySet());
			keyList.addAll(outOfSeq.keySet());
			for (long key: keyList)
				System.out.println(longToString(key) + ((!connectionsDict.isEmpty()) ? (" expecting packet " + connectionsDict.get(key)) : ""));
		}
		// System.out.print("\nRemaining orphaned out-of-sequence packets:");
		// if (outOfSeq.isEmpty()) {
		// 	System.out.println("None");
		// } else {
		// 	for (long key: outOfSeq.keySet()) {
		// 		System.out.print("\n" + longToString(key) + " with packet no(s).  ");
		// 		for (Integer packetNum: outOfSeq.get(key).keySet()) {
		// 		System.out.print(packetNum + ", ");
		// 		}
		// 	}
		// }
		
		if (!outOfSeq.isEmpty()) {
			
			for (long connection: outOfSeq.keySet()) { //for each connection with out-of-sequence packets
				Set<Integer> keySetCopy = outOfSeq.get(connection).keySet();
				int nextSeqNum = Collections.min(keySetCopy);
				int prevSeqNum;
				System.out.println("\nMissing bytes from connection " + longToString(connection) + " as follows:");
				//calculate missing bytes between sequenced packets and first oOS packet
				if (!connectionsDict.containsKey(connection)) { 
					missingBytes += nextSeqNum;
					System.out.println(nextSeqNum + " missing bytes between start of connection and TCP sequence number " + nextSeqNum + 
										" at time " + outOfSeq.get(connection).get(nextSeqNum).getTimeStamp());
					//nextSeqNum = outOfSeq.get(connection).get(nextSeqNum).getNextSeqNum();
				} else {
					missingBytes += nextSeqNum - connectionsDict.get(connection);
					System.out.println((nextSeqNum - connectionsDict.get(connection))+ " missing bytes between TCP sequence numbers " + connectionsDict.get(connection) + " and " + nextSeqNum + 
										" at time " + outOfSeq.get(connection).get(nextSeqNum).getTimeStamp());
				}

				//calculate missing bytes between oOS packets
				while (!keySetCopy.isEmpty()) {
					do {
						prevSeqNum = nextSeqNum;
						nextSeqNum = outOfSeq.get(connection).get(prevSeqNum).getNextSeqNum();
						keySetCopy.remove(prevSeqNum);
					} while (outOfSeq.get(connection).containsKey(nextSeqNum));
					
					if (!keySetCopy.isEmpty()) {
						prevSeqNum = nextSeqNum;
						nextSeqNum = Collections.min(keySetCopy);
						missingBytes += nextSeqNum - prevSeqNum;
						System.out.println((nextSeqNum - prevSeqNum) + " missing bytes between TCP sequence numbers " + prevSeqNum + " and " + nextSeqNum + 
											" at time " + outOfSeq.get(connection).get(nextSeqNum).getTimeStamp());
					}
				}
				
			}
		}
		System.out.println("\n\nSummary:");
		System.out.println(packetCounter + " packets checked containing a total of " + byteCounter + " bytes from " + connectionCounter + " connections.\n");
		System.out.printf("%d / %d bytes missing from trace sequence (%.2f%% loss).%n%n", missingBytes, byteCounter, missingBytes / (double) byteCounter);
		System.out.println("Subsequent missing packets from " + connectionsDict.size() + " connections could not be determined.\n");
	}
	

	/**
	 * Function for creating a 64-bit long identifier for each connection. The 64-bits are allocated as follows:
	 * Source IP: 16 bits for last 2 numbers of IP address
	 * Source port: 16 bits
	 * Dest IP: 16 bits for last 2 numbers of IP address
	 * Dest port: 16 bits
	 * NOTE: This implementation assumes source IP always starts with "192.168." and dest IP always 
	 * starts with "10.0." so only last 16 bits are used to identify IP addresses.
	 * @param line String array from a line of the trace file
	 */	
	public long makeLongID(String[] line) {
		long longID;
        String[] sourceIP = line[2].split("\\.");
        String[] destIP = line[4].split("\\.");
		longID = 	Long.parseLong(sourceIP[2]) * 0x0100000000000000L +
					Long.parseLong(sourceIP[3]) * 0x0001000000000000L +
					Long.parseLong(line[3]) * 0x0000000100000000L +
					Long.parseLong(destIP[2]) * 0x0000000001000000L +
					Long.parseLong(destIP[3]) * 0x0000000000010000L +
					Long.parseLong(line[5]);
		return longID;
	}

	/**
	 * Function for changing the connection long ID to a human readable string.
	 * @param longID long number representing the connection IPs and ports
	 */	
	public String longToString(long longID) {
        long[] longA = new long[6];
		longA[0] = Long.divideUnsigned(longID, 0x0100000000000000L);
		longA[1] = Long.divideUnsigned(longID, 0x0001000000000000L) & 0xFF;
		longA[2] = Long.divideUnsigned(longID, 0x0000000100000000L) & 0xFFFF;
		longA[3] = Long.divideUnsigned(longID, 0x0000000001000000L) & 0xFF;
		longA[4] = Long.divideUnsigned(longID, 0x0000000000010000L) & 0xFF;
		longA[5] = longID & 0xFFFF;
		return String.format("From 192.168.%d.%d:%d to 10.0.%d.%d:%d", longA[0], longA[1], longA[2], longA[3], longA[4], longA[5]);
    }

	/**
	 * Function for updating the next expected TCP sequence number when the current expected packet is found.
	 * Subsequently checks for any packets in the outOfSeq record for any oOS packets that can now be sequenced.
	 * Removes the connection from connectionsDict, if the connection is closed (with all packets in sequence).
	 * @param seqNum current sequence number
	 * @param nextSeqNum next sequence number to replace the current sequence number in the chain
	 * @param connID identifier for the connection
	 * @param closedflag flag for whether the packet is closing the connection
	 */	
	public void updateSeqNum(int seqNum, int nextSeqNum, long connID, int closedFlag) {
		//System.out.println("For " + connID + " " + connectionsDict.get(connID) + " is replaced by " + nextSeqNum);
		connectionsDict.replace(connID, nextSeqNum); //update value to next expected TCP sequence num
		
		//after updating, check if any outOfSeq packets can be reordered
		if (outOfSeq.containsKey(connID) && outOfSeq.get(connID).containsKey(nextSeqNum)) {
			oOSPacket outOfSeqInfo = outOfSeq.get(connID).remove(nextSeqNum); //remove the reordered packet info from outOfSeq
			if (outOfSeq.get(connID).isEmpty())
				outOfSeq.remove(connID); //remove the connection from outOfSeq if no more outOfSeq packets
			//System.out.println(nextSeqNum + " packet from " + connID + " which was out of sequence, removed from outOfSeq HashMap");
			updateSeqNum(nextSeqNum, outOfSeqInfo.getNextSeqNum(), connID, outOfSeqInfo.getFinFlag());
		}
		if (closedFlag == 2) {
			//System.out.println(connID + " closed");
			connectionsDict.remove(connID); //remove connection if closed from in-sequence packet
			closedConnections.add(connID);
		}
	}

/**
 * Out-of-sequence packet inner class to store data on each out of sequence packet.
 */
private class oOSPacket {
	private int seqNum;
	private int nextSeqNum;
	private int finFlag;
	private double timeStamp;

/**
 * Constructor for oOSPacket object. Takes the TCP sequence number, next expected TCP sequence number,
 * close connection flag, and time stamp of the packet and stores them into the class
 * @param seqNum TCP sequence number
 * @param nextSeqNum next expected TCP sequence number
 * @param finFlag close connection flag of the packet
 * @param timeStamp time stamp of the packet
 */
	private oOSPacket(int seqNum, int nextSeqNum, int finFlag, double timeStamp) {
		this.seqNum = seqNum;
		this.nextSeqNum = nextSeqNum;
		this.finFlag = finFlag;
		this.timeStamp = timeStamp;
	}
	private int getSeqNum() {
		return seqNum;
	}
	private int getNextSeqNum() {
		return nextSeqNum;
	}
	private int getFinFlag() {
		return finFlag;
	}
	private double getTimeStamp() {
		return timeStamp;
	}
}

/**
 * TraceData inner class to handle the instantiating and preparation of data from the trace file.
 */
	private class TraceData {
		private ArrayList<String[]> traceLines = new ArrayList<String[]>();

	/**
	 * Constructor for TraceData object. Takes the input file of trace data and uses a scanner object to 
	 * split the input data into String arrays for each record and adds it to an ArrayList
	 * @param f input file of the trace data.
	 */
		private TraceData(File f) {
			Scanner input = null;
			try {
				input = new Scanner(f);
			}
			catch (IOException ioExc) {
				return; // check if file isn't readable;
			}
			while (input.hasNext()) {
				String currentLine = input.nextLine();
				traceLines.add(currentLine.split("\t"));
			}
			input.close();
		}
	}

}
