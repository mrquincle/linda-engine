#include <bits.h>

/**
 * Returns the first RAISED bit. Perhaps assembly? 
 */
uint8_t FIRST(uint8_t bitseq) {														 
	uint8_t i;											
	for (i = 1; i<17; i++) {							
		if (RAISED(bitseq, i)) return i; 				
	}													
	return 0;											
}

uint8_t RANDOM() {
	return 0;
}
