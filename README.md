# fast-holdem-eval

### Function for fast evaluation of Texas Hold'em Poker Hands

### Background:

Texas Hold'em is a poker game in which each player is dealt 2 private cards, with 5 cards dealt face-up. Each player therefore gets 7 cards total, any 5 of which can be played to make the best "hand" possible.

The evaluation problem requires assigning a value to all 2,598,960 separate 5-card combos (or 133,784,560 7 card-combos), enforcing a global ordering with straight flushes > quads > full house > flush > straight > trips > two pair > single pair > no pair, with high card "kickers" as tie-breakers between hands of otherwise equal strength.

Public domain (i.e. a quick google search) shows several ways in which this has been approached. 
1. naive methods (hand is scanned for each hand possibility in order of strength) 
2. improved methods using histograms (count of occurrences of each card rank) to speedup the naive approach
3. further improved methods using bitmask hand representation and bit manipulation to speed up the histogram approach 
4. hashtable methods

Online discussions seem to point towards 4.) as the "state of the art", with well optimized examples boasting throughput of 60mm+ hands/s. These methods generally use a 52bit mask representation of poker hands (similar to mine, discussed below). Since it would be impossible to fit a table with 2^52 entries into RAM, they compute a collision-free hash of each of the 133mm hands to index into a more compact lookup table. However, this approach is bound by memory latency limits as this generally doesn't fit into L1 cache. 

Assume 32KB L1 cache access has 4 cpu clock cycle latency (~1ns @ 5ghz), 256KB L2 cache 12 cycles (3ns), 16MB L3 cache 42 cycles (11ns), RAM ~240 cycles (60ns).

A perfect hashtable with all 133,784,560 7-card combos with a compact 2-byte hand strength representation (~8000 different hand strengths) takes up at minimum ~266MB of memory. For sequential random evaluations with a warmed cache, ~1% of all evals will hit L1/L2 cache, while 6% will hit L3 cache, and the rest will hit main memory. Theoretically, this averages to ~56ns per evaluation, or ~17mm evals/s. Practically an equivalent operation is accessing random members of a 133mm entry array, which benchmarked at 21ns or about 50mm/s on my hardware (probably some pipelining or faster memory access than my assumptions), matching up roughly with performance claims of other solutions. 

In real use, I assume that evaluations would not be totally randomly distributed, but rather grouped in some order (e.g. a poker solver may evaluate a "range" of possible two card holdings using the same 5 community cards), and some clever hashing optimizations that order entries in the hashtable by relative proximity could improve cache hit rates in the lookup table for these use cases. Some smart precomputation to collapse equivalent hands to a single hash can reduce table size can also increase hit rates by allowing more of the table to fit into the cache, but additional precomputation comes at a cost of additional cpu cycles. I would argue that this approach's dependence on cache is still a problem, as it is sensitive to other running processes (e.g. say you're using this in a deep learning pipeline to learn a poker strategy, I'm sure most of the LUT in cache would get clobbered before the next eval). And a tuned hash function for one purpose may result in poor cache usage in other cases (e.g. perhaps you tune a perfect hash to make range equity calculations on the river utilize cache efficiently - 2 degrees of freedom - but this may not work well calculating range equity on the flop with 2 cards to go - 4 degrees of freedom).

This repo holds my attempt to implement approach 3.) a histogramy, bit-hacky, "naive" approach that can be done completely in registers with no penalties from memory access latency. Compiled -Ofast -mbmi2 -mavx2 on i9-9900k running single core at 5ghz averages 6ns per evaluation, or up to 167mm evals/s. This is roughly equivalent to averaging an L2 or L3 cache hit every time, 3x faster than other approaches. I'm sure there's some room for further optimization, probably along the lines of reducing branch prediction misses. Coding on WSL2 though which doesn't support hardware counters for perf...

### Details:
A hand is represented in a single 64byte ulong consisting of packed 16bit words. Each 16bit word represents a suit (spade -> heart -> diamond -> club), with each bit representing the presence of a particular rank in that suit.

Each suit looks like this: [ N | N | A | K | Q | J | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | A ].

There are two padding bits at the front, followed by the ranks in descending order with a repeated A at the bottom bit (for easier straight detection). Both top and bottom A bits should be set if that particular Ace is present. 

[0001000010000000 0000000100000010 0001001000000000 0010000000000001] is the 64bit repr of a hand consisting of Ks 8s 9h 2h Kd 10d Ac. This represents a pair of kings with A-10-9 kickers.

Note: This seems to be a common compact representation for a hand of cards, and you can see how this doesn't get great cache performance. For example [ 0000000010000000 0000000100000010 0001001000000000 0011000000000001 ] (8s 9h 2h Kd 10d Ac Ks) is a hand that is very similar to the previous (Ks switched to Kc), and would often be evaluated contemporaneously with the previous hand in range equity calculations, but it's not clear to me that these two entries would be hashed in such a way to be cache friendly in commonly used hash functions.

Results are generally encoded with a result flag set in the top bits to indicate category, with bottom bits used to distinguish within the category.
1. Straight flush: 0x800000000000XXXX. Bottom 16 bits indicate the rank of lowest card. Royal: 0x8000000000000200 (10 bit set). Wheel straight flush: 0x8000000000000001 (bottom A bit set).
2. Quads: 0x400000000000XXXX. Bottom 16 bits indicate the rank of the quads. Quad aces: 0x8000000000001000 (top A bit set).
3. Full house: 0x20000000XXXXXXXX. Bottom 16 bits indicate the pair. Next 16 bits indicate the trips.
4. Flush: 0x100000000000XXXX. Bottom 16 bits indicate the 5 cards that hold the flush using suit encoding above
5. Straight: 0x080000000000XXXX. Bottom 16 bits indicate the rank of lowest card. A-high straight: 0x2000000000000200 (10 bit set). Wheel straight: 0x2000000000000001 (bottom A bit set).
6. Trips: 0x0000XXXX0000XXXX. Bottom 16 bits have two bits set with the kickers. Next 16 empty. Next 16 after that has 1 bit set indicating the trips.
7. Pair/High Card: 0x00000000XXXXXXXX. Bottom 16 bits have bits set with kickers. Next set with up to 2 pairs.

