#pragma once
#include "card_ops.h"

unsigned long no_trips_fast_branch(unsigned long pairs, unsigned long singles)
{
    auto two_pair = TWO_PAIR_RESULT | pairs << 16 | first_bit(singles);
    auto three_pair = TWO_PAIR_RESULT | (pairs & (pairs - 1)) << 16 | first_bit(singles);
    pairs &= pairs - 1;
    pairs &= pairs - 1;
    auto no_pair = pairs << 16 | first_bit(singles);

    if (!pairs) [[likely]]
        return no_pair;
    else if (std::popcount(pairs) == 1)
        return two_pair;
    else
        return three_pair;
}

unsigned long one_trips_fast_branch(unsigned long trips, unsigned long pairs, unsigned long singles)
{
    auto two_pairs = 1UL << 59 | trips << 16 | (pairs & -pairs);
    auto one_pair = 1UL << 59 | trips << 16 | pairs;

    auto out = trips << 32;
    auto mask = first_bit(singles);
    singles &= ~mask;
    mask |= first_bit(singles);

    if (std::popcount(pairs) == 0) [[likely]]
    {
        return out | mask;
    }
    else if (std::popcount(pairs) == 1)
    {
        return one_pair;
    }
    else
    {
        return two_pairs;
    }
}

unsigned long result_fast_branch(unsigned long trips, unsigned long pairs, unsigned long singles)
{
    switch (std::popcount(trips))
    {
    [[unlikely]] case 2:
        return FULL_HOUSE_RESULT | ((trips & (trips - 1)) << 16) | last_bit(trips);
    case 1:
        return one_trips_fast_branch(trips, pairs, singles);
    [[likely]] default:
        return no_trips_fast_branch(pairs, singles);
    }
}

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
        return STRAIGHT_RESULT | first_bit_idx(straight_mask) % 16;

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

    return result_fast_branch(trips, pairs, singles);
}
