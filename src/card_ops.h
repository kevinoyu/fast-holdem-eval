#pragma once

#include <bit>
#include <array>
#include <numeric>

constexpr unsigned long SUIT_MASK = 0b0011111111111110UL; // represented A-2, excluding the bottom A
constexpr unsigned long PAIR_RESULT = 1UL << 56;
constexpr unsigned long TWO_PAIR_RESULT = 1UL << 57;
constexpr unsigned long TRIPS_RESULT = 1UL << 58;
constexpr unsigned long STRAIGHT_RESULT = 1UL << 59;
constexpr unsigned long FLUSH_RESULT = 1UL << 60;
constexpr unsigned long FULL_HOUSE_RESULT = 1UL << 61;
constexpr unsigned long QUADS_RESULT = 1UL << 62;
constexpr unsigned long STRAIGHT_FLUSH_RESULT = 1UL << 63;

inline unsigned long first_bit_idx(unsigned long x)
{
    return 63 - std::countl_zero(x);
}

inline unsigned long first_bit(unsigned long x)
{
    return 1UL << first_bit_idx(x);
}

inline unsigned long last_bit(unsigned long x)
{
    return 1UL <<  std::countr_zero(x);
}

inline unsigned long detect_straight(unsigned long x)
{
    auto out = x >> 2;
    out &= out >> 1;
    out &= out >> 1;
    return out;
}

inline unsigned long one_trips(unsigned long trips, unsigned long pairs, unsigned long singles)
{
    switch (std::popcount(pairs))
    {
    [[unlikely]] case 2:
        return FULL_HOUSE_RESULT | trips << 16 | (pairs & -pairs);
    case 1:
        return FULL_HOUSE_RESULT | trips << 16 | pairs;
    [[likely]] default:
        auto out = trips << 32;
        auto mask = 1UL << first_bit_idx(singles);
        singles &= ~mask;
        mask |= 1UL << first_bit_idx(singles);
        return out | mask;
    }
}

inline unsigned long no_trips(unsigned long pairs, unsigned long singles)
{
    switch (std::popcount(pairs))
    {
    case 3: // if there are three pairs, unset the smallest one
        pairs &= pairs - 1;
    case 2: // deliberate fallthrough - reduces compiled asm by a couple lines
        return TWO_PAIR_RESULT | pairs << 16 | first_bit(singles);
    [[likely]] default: // either one or no pair. in one pair case, there are 5 singles.
                        // in no pair case, there are 7 singles. in both cases, can return pairs and singles with bottom two bits dropped
        auto out = singles & (singles - 1);
        out &= (out - 1);
        return pairs << 16 | out;
    }
}

inline unsigned long result(unsigned long trips, unsigned long pairs, unsigned long singles)
{
    switch (std::popcount(trips))
    {
    [[unlikely]] case 2:
        return FULL_HOUSE_RESULT | ((trips & (trips - 1)) << 16) | last_bit(trips);
    case 1:
        return one_trips(trips, pairs, singles);
    [[likely]] default:
        return no_trips(pairs, singles);
    }
}

inline unsigned long evaluate_hand(const unsigned long &cards)
{
    // bitwise or across each 16 bit suit to find all present ranks
    auto or_ranks = cards | (cards >> 32);
    or_ranks |= or_ranks >> 16;

    switch (std::popcount(or_ranks & SUIT_MASK))
    {
        // if flush or straight is possible, check that first since it doesn't depend on single/pair/trip finding
    case 7:
    case 6:
    case 5:
    {
        auto straight_flush_mask = detect_straight(cards);
        if (straight_flush_mask) [[unlikely]]
            return STRAIGHT_FLUSH_RESULT | (1UL << (first_bit_idx(straight_flush_mask) % 16));

#pragma unroll
        for (int i = 0; i < 4; ++i)
        {
            auto suit = cards & (SUIT_MASK << (i * 16));

            switch (std::popcount(suit))
            {
            [[unlikely]] case 7:
                suit &= (suit - 1);
            [[unlikely]] case 6:
                suit &= (suit - 1);
            [[unlikely]] case 5:
                return FLUSH_RESULT | (suit >> (i * 16));
            [[likely]] default:
                continue;
            }
        }
        // check for straights - if there is, return index of first set bit in the straight mask % 16 (which should be fast) in order to make straights across suits comparable
        auto straight_mask = detect_straight(or_ranks);
        if (straight_mask) [[unlikely]]
            return STRAIGHT_RESULT | first_bit(straight_mask);
    }
    [[likely]] case 4:
    case 3:
    case 2:
    {
        // bitwise and across each 16 bit suit - any set bits represent quads
        auto and_ranks = cards & (cards >> 32);
        and_ranks &= and_ranks >> 16;

        // if quads exist, return the bit representing quad ranks as well as the highest non-quad high card
        if (and_ranks)
            return QUADS_RESULT | and_ranks << 16 | first_bit(or_ranks ^ and_ranks);

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
    // should never get here
    default:
        return -1;
    }
}
