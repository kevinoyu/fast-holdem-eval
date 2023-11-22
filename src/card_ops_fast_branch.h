#pragma once
#include "card_ops.h"

inline unsigned long evaluate_hand_fast_branch(const unsigned long &cards)
{
    // bitwise or across each 16 bit suit to find all present ranks
    auto or_ranks = cards | (cards >> 32);
    or_ranks |= or_ranks >> 16;

    // bitwise and across each 16 bit suit - any set bits represent quads
    auto and_ranks = cards & (cards >> 32);
    and_ranks &= and_ranks >> 16;

    auto straight_flush_mask = detect_straight(cards);
    if (straight_flush_mask) [[unlikely]]
        return STRAIGHT_FLUSH_RESULT | first_bit_idx(straight_flush_mask) % 16;

    // if quads exist, return the bit representing quad ranks as well as the highest non-quad high card
    if (and_ranks)
        return QUADS_RESULT | and_ranks << 16 | first_bit_idx(or_ranks ^ and_ranks);

#pragma unroll
    for (int i = 0; i < 4; ++i)
    {
        auto suit = cards & (SUIT_MASK << (i * 16));
        if (std::popcount(suit) >= 5) [[unlikely]]
        {
            auto result = FLUSH_RESULT;
            suit >>= (i * 16);

#pragma unroll
            for (int j = 0; j < 5; ++j)
            {
                auto bit = first_bit(suit);
                result |= first_bit(suit);
                suit &= ~bit;
            }
            return result;
        }
    }

    // check for straights - if there is, return index of first set bit in the straight mask % 16 (which should be fast) in order to make straights across suits comparable
    auto straight_mask = detect_straight(or_ranks);
    if (straight_mask) [[unlikely]]
        return STRAIGHT_RESULT | first_bit(straight_mask);

    unsigned long singles = 0, pairs = 0, trips = 0;

    // #pragma unroll
    for (int i = 0; i < 4; ++i)
    {
        auto card = cards >> (i * 16);
        singles = (singles ^ card) & ~pairs;
        pairs = (pairs ^ card) & ~singles;
    }

    singles ^= and_ranks;
    trips = or_ranks ^ (singles | pairs);

    singles &= SUIT_MASK;
    pairs &= SUIT_MASK;
    trips &= SUIT_MASK;

    return result(trips, pairs, singles);
}
