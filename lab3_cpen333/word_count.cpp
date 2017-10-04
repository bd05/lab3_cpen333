#include "word_count.h"

// implementation details
int word_count(const std::string& line, int start_idx) {
	// YOUR IMPLEMENTATION HERE
	int count = 0;
	bool pastTrailing = false;

	for (int i = start_idx; i < line.length(); i++){
		if (line[i] != ' ')
			pastTrailing = true;
		if (line[i] == ' ' && pastTrailing == true)
			count++;
	}

	return count + 1;
}