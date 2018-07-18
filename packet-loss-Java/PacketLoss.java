import java.util.ArrayList;
import java.util.Set;
import java.util.HashMap;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
//import java.util.Comparator;
//import java.util.regex.*;
import java.util.Scanner;

import java.io.File;
import java.io.IOException;

public class PacketLoss {
	
	public static void main(String args[]) {
		PacketLoss plp = new PacketLoss();
		plp.start();
	}
	
	public void start() {
		File f = new File("trace-small.txt");
		System.out.println(f);
		TraceData traceInput = new TraceData(f);
		HashMap<List<String>, Integer> connectionsDict = new HashMap<>();
		int counter = 0;
		for (String[] line : traceInput.traceLines) {
			if (line[2].isEmpty() || line[3].isEmpty() || line[4].isEmpty() || line[5].isEmpty()) //if any IP or ports are empty, skip
				continue;
			List<String> connectionTuple = Collections.unmodifiableList(Arrays.asList(line[2], line[3], line[4], line[5]));
			counter += 1;
			if (counter % 1000 == 0) 
				System.out.println(connectionTuple.hashCode());
			//System.out.println(Arrays.toString(connectionTuple.toArray()));
			if (connectionsDict.containsKey(connectionTuple)) {
				System.out.println("contains key!");
			} else {
				connectionsDict.put(connectionTuple, Integer.parseInt(line[13]));
			}
			
		}
	}
	
/**
 * TraceData inner class of TraceViewer to handle the instantiating and processing of data from the trace file.
 */
	private class TraceData {
		private ArrayList<String[]> traceLines = new ArrayList<String[]>();

	/**
	 * Constructor for TraceData object. Takes the input file of trace data and processes it.
	 * @param f input file of the trace data.
	 */
		private TraceData(File f) {
			parseFile(f);
		}
	
	/**
	 * Takes the input trace data file and uses a scanner object to split the input data into
	 * String arrays for each record and adds it to the ArrayList of string arrays.
	 * @param file input file of the trace data passed by the constructor.
	 */
		private void parseFile(File file) {
			int counter = 0;
			Scanner input = null;
			try {
				input = new Scanner(file);
			}
			catch (IOException ioExc) {
				return; // check if file isn't readable;
			}
			while (input.hasNext()) {
				String currentLine = input.nextLine();
				traceLines.add(currentLine.split("\t"));
				counter += 1;
				if (counter % 10000 == 0) 
					System.out.println(counter + " lines parsed.");
			}
			input.close();
		}
	}

}
