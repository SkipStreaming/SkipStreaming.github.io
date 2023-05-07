// multi-index hashing
// a simple implementation of
// Fast Exact Search in Hamming Space with Multi-Index Hashing, M. Norouzi, A. Punjani, D. J. Fleet, IEEE TPAMI 2014

#pragma once

#include <vector>
#include "vhash.h"

#define MAX_HASH_TABLE_M 32	// m upper bound
#define MAX_XOR_BIT 32
#define MAX_HASH_TABLE_INDEXING 0xFFFF // max indexing in each table
#define MAX_HASH_ITEM 102400

/*struct mi_hash_table
{
    int code_q_bit;	// the length of each code (in bit)
    int hash_table_m; // the number of hash table. m hash tables for m substrings
    int substring_s_bit; // the length of each substring (in bit)
    int hamming_r; // search radius
    int hamming_r0;	// r0 = r / m
    std::vector<int> tables[MAX_HASH_TABLE_M]; // only support up to int32 indexing
};*/

int mih_codes_n = 0; // the number of hash codes (of frame)
int mih_code_q_bit = SCALE_HEIGHT * SCALE_WIDTH;	// the length of each code (in bit)
int mih_hash_table_m = SCALE_HEIGHT * SCALE_WIDTH / 16; // the number of hash table. m hash tables for m substrings
int mih_substring_s_bit = 16; // the length of each substring (in bit)
int mih_hamming_r = SCALE_HEIGHT * SCALE_WIDTH * HAMMING_THRESHOLD; // search radius
//int mih_hamming_r0 = mih_hamming_r / mih_hash_table_m; // r0 = r / m

vhash_item vhash_by_frame_index[MAX_HASH_ITEM];
bool mih_frame_touched[MAX_HASH_ITEM];
int mih_frame_hamming_dis[MAX_HASH_ITEM];
//std::map<int, std::vector<int>> mih_tables[MAX_HASH_TABLE_M]; // only support up to int32 indexing
std::vector<int> mih_tables[MAX_HASH_TABLE_M][MAX_HASH_TABLE_INDEXING]; // only support up to int32 indexing
std::vector<int> mih_history[SCALE_HEIGHT * SCALE_WIDTH + 1];

__inline int __builtin_ctz(int v)
{
    int pos;
    if (v == 0)
        return 0;

    __asm
    {
        bsf eax, dword ptr[v];
    }
}

__inline void mih_insert_table(const vhash_item& vhash)
{
    for (int i = 0; i < mih_hash_table_m; i++)
    {
        int index = (vhash.h[2 * i] << 8) + vhash.h[2 * i + 1];
        /*if (mih_tables[i].find(index) == mih_tables[i].end())
        {
            mih_tables[i].insert(std::map<int, std::vector<int>>::value_type(index, std::vector<int>()));
        }*/

        mih_tables[i][index].push_back(vhash.frame_index);
    }
}

__inline std::vector<int> mih_query(const vhash_item& vhash)
{
    int r = 0;
    std::vector<int> ret;
    memset(mih_frame_hamming_dis, -1, sizeof(mih_frame_hamming_dis));
    for (int i = 0; i <= SCALE_HEIGHT * SCALE_WIDTH; i++)
        mih_history[i].clear();

    while (r <= mih_hamming_r)
        //while (true)
    {
        int r0 = r / mih_hash_table_m;
        int a = r - r0 * mih_hash_table_m;
        ret = mih_history[r];

        for (int i = 0; i < mih_hash_table_m; i++)
        {
            unsigned int bitstr = (vhash.h[2 * i] << 8) + vhash.h[2 * i + 1];
            unsigned int mask = (1 << r0) - 1;
            unsigned int end_mask = mask << (mih_substring_s_bit - r0);


            while (true)
            {
                for (int j = 0; j < mih_tables[i][bitstr ^ mask].size(); j++)
                {
                    int dis = -1;

                    /*if (mih_frame_hamming_dis[mih_tables[i][bitstr ^ mask][j]] != -1)
                    {
                        dis = mih_frame_hamming_dis[mih_tables[i][bitstr ^ mask][j]];
                    }*/
                    if (mih_frame_hamming_dis[mih_tables[i][bitstr ^ mask][j]] == -1)
                    {
                        dis = get_hamming_distance(vhash, vhash_by_frame_index[mih_tables[i][bitstr ^ mask][j]]);
                        mih_frame_hamming_dis[mih_tables[i][bitstr ^ mask][j]] = dis;
                        mih_history[dis].push_back(mih_tables[i][bitstr ^ mask][j]);
                        if (dis == r)
                        {
                            ret.push_back(mih_tables[i][bitstr ^ mask][j]);
                        }
                    }

                }

                if (mask == end_mask)
                    break;

                // tricky permutation from https://graphics.stanford.edu/~seander/bithacks.html
                unsigned int t = mask | (mask - 1); // t gets v's least significant 0 bits set to 1
                // Next set to 1 the most significant bit to change,
                // set to 0 the least significant ones, and add the necessary 1 bits.
                mask = (t + 1) | (((~t & -~t) - 1) >> (__builtin_ctz(mask) + 1));
            }

        }

        if (ret.size() > 0)
            return ret;
        r++;
    }

    return ret;
}
