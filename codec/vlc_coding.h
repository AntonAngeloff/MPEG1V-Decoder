#ifndef VLC_CODING_H_INCLUDED
#define VLC_CODING_H_INCLUDED

#include "..\generic\bitstream.h"

#define VLC_DIRECTION_LEFT  0x0
#define VLC_DIRECTION_RIGHT 0x1

typedef struct {
    char symbol;
    void* branches[2];
} VLCTreeNode;

/**
 * Struct for single VLC prefix code and it's corresponding symbol
 */
typedef struct {
    uint64_t bits;
    int8_t bit_count;
    char symbol;
} VLCPrefixEntry;

MMFRES vlc_tree_create(char *symbols, uint64_t *bit_codes, uint8_t *bit_counts, int32_t element_count, VLCTreeNode **tree_root);

/**
 * Creates a Huffman tree from a given table of prefixes.
 *
 * @param prefixes Array of prefixes
 * @param element_count Length of <i>prefixes</i>. It can be <i>0</i>, in this case it will thread the array as NULL terminated.
 * @param tree_root Pointer to a variable which will receive pointer to the root of the tree.
 * @return RC_OK on success, error otherwise.
 */
MMFRES vlc_tree_create2(const VLCPrefixEntry *prefixes, int32_t element_count, VLCTreeNode **tree_root);

/**
 * Releases a Huffman tree and it's inner resources.
 *
 * @param root Double pointer to tree's root node.
 * @return RC_OK on success, error otherwise.
 */
MMFRES vlc_tree_free(VLCTreeNode **root);


MMFRES vlc_decode_bitstream(MMFBitstream *bs, VLCTreeNode *vlc_tree, int32_t symbol_limit, void *dst, int32_t *len);
/*
MMFRES vlc_decode_buffer()
*/
#endif // VLC_CODING_H_INCLUDED
