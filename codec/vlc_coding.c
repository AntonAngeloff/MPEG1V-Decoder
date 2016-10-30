#include "vlc_coding.h"
#include "..\mmfutil.h"

/* Adds a new node (branch) to given node (entry_node).
 */
MMFRES vlc_tree_add_node(VLCTreeNode *entry_node, char symbol, uint64_t path_bits, int8_t path_bit_count)
{
    if(path_bit_count <= 0) {
        #ifdef DEBUG
        //TODO: log
        #endif // DEBUG
        return RC_INVALIDARG;
    }

    if (path_bit_count == 1) {
        VLCTreeNode *node = mmf_allocz(sizeof(VLCTreeNode));

        //We have to insert node as branch of this one
        entry_node->branches[path_bits] = node;
        node->symbol = symbol;

        return RC_OK;
    }

    int dir = path_bits >> (path_bit_count-1);

    //Check if a node exist on this direction
    if(!entry_node->branches[dir]) {
        MMFRES rc = vlc_tree_add_node(entry_node, 0, dir, 1);
        if(failed(rc)) return rc;
    }

    //Recourse
    return vlc_tree_add_node(entry_node->branches[dir], symbol, path_bits & ((1 << (path_bit_count-1))-1), path_bit_count-1);
}

/* Find a node in a tree from a given path (the path is a bit sequence)
 */
MMFRES vlc_find_node(VLCTreeNode *entry, uint32_t path_bits, int8_t path_bit_count, VLCTreeNode **node)
{
    if(path_bit_count <= 0) {
        #ifdef DEBUG
        //TODO: log
        #endif // DEBUG
        return RC_INVALIDARG;
    }

    int dir = path_bits >> (path_bit_count-1);

    #ifdef DEBUG
    if(entry->branches[dir] == NULL) {
        //log
        return RC_FAIL;
    }
    #endif // DEBUG

    if(path_bit_count == 1) {
        (*node) = entry->branches[dir];
        return RC_OK;
    }

    //Invoke recursion
    return vlc_find_node(entry->branches[dir], path_bits & ((1 << (path_bit_count-1))-1), path_bit_count-1, node);
}

/* Recursively frees all tree nodes, including the root node.
 */
MMFRES vlc_tree_free(VLCTreeNode **root)
{
    VLCTreeNode *r = *root;

    if(r->branches[VLC_DIRECTION_LEFT]) {
        vlc_tree_free(&r->branches[VLC_DIRECTION_LEFT]);
    }

    if(r->branches[VLC_DIRECTION_RIGHT]) {
        vlc_tree_free(&r->branches[VLC_DIRECTION_RIGHT]);
    }

    mmf_free(r);
    *root = NULL;

    return RC_OK;
}

/*
 * Create VLC\Huffman tree from given array of symbol\bitcode pairs.
 */
MMFRES vlc_tree_create(char *symbols, uint64_t *bit_codes, uint8_t *bit_counts, int32_t element_count, VLCTreeNode **tree_root)
{
    int i=0;

    #ifdef DEBUG
    if(element_count <= 0) {
        return RC_INVALIDARG;
    }
    #endif // DEBUG

/*
    int k;

    //Create a copy of the symbols\bitcodes\bitlens and sort then by bit length.
    char *sym_tbl = mmf_alloc(sizeof(char) * element_count);
    uint64_t *bitcode_tbl = mmf_alloc(sizeof(uint64_t) * element_count);
    uint8_t *bitlen_tbl = mmf_alloc(sizeof(uint8_t) * element_count);

    //Sort-copy the arrays
    for(i=0; i<element_count-1; i++) {
        int k=i;

        //Find next lowest bitcount element
        for(j=i+1; j<element_count; j++) {
            if(bit_counts[j] < bit_counts[k]) {
                k = i;
            }
        }

        //Copy array element to local array
        sym_tbl[i] = symbols[k];
        bitcode_tbl[i] = bit_codes[k];
        bitlen_tbl[i] = bit_counts[k];
    }
*/

    //Allocate root node
    VLCTreeNode *root_node = mmf_allocz(sizeof(VLCTreeNode));
    MMFRES rc;

    //Add nodes
    for(i=0; i<element_count; i++) {
        rc = vlc_tree_add_node(root_node, symbols[i], bit_codes[i], bit_counts[i]);
        if(failed(rc)) goto fail;
    }

    //Success
    *tree_root = root_node;
    return RC_OK;

fail:
    vlc_tree_free(&root_node);
    return rc;
}

MMFRES vlc_find_leaf(VLCTreeNode *entry, uint64_t path, int8_t path_bit_count, VLCTreeNode **result, int32_t *depth)
{
    if(path_bit_count < 0) {
        #ifdef DEBUG
        //TODO: log
        #endif // DEBUG
        return RC_FAIL;
    }

    int dir = path >> (path_bit_count-1);

    //Check if this node is a leaf
    if(!entry->branches[VLC_DIRECTION_LEFT] && !entry->branches[VLC_DIRECTION_RIGHT]) {
        //Found
        (*result) = entry;
        return RC_OK;
    }

    if(path_bit_count == 0) {
        //End of path. Yet we found no leaf. It's time to give up.
        return RC_FAIL;
    }

    (*depth) ++;

    //Invoke recursion
    return vlc_find_leaf(entry->branches[dir], path & ((1 << (path_bit_count-1))-1), path_bit_count-1, result, depth);
}

MMFRES vlc_decode_bitstream(MMFBitstream *bs, VLCTreeNode *vlc_tree, int32_t symbol_limit, void *dst, int32_t *len)
{
    uint8_t *target = (uint8_t*)dst;
    uint32_t bits;
    MMFRES rc;

    (*len) = 0;

    while (symbol_limit > 0 || symbol_limit == -1) {
        int bits_to_read = 32;

        bits = bitstream_peek_bits(bs, bits_to_read, &rc);

        //No enought bits in stream, try to read less
        if(rc == RC_NEED_MORE_INPUT) {
            bits_to_read = bitstream_get_size(bs);
            if(bits_to_read > 0) {
                bits = bitstream_peek_bits(bs, bits_to_read, &rc);
            }
        }

        if(rc == RC_END_OF_STREAM || rc == RC_NEED_MORE_INPUT) {
            return RC_OK;
        }else if(failed(rc))
            return rc;

        //find first leaf
        VLCTreeNode *leaf;
        int32_t depth = 0;

        //Find leaf in tree, by given code (path).
        rc = vlc_find_leaf(vlc_tree, bits, bits_to_read, &leaf, &depth);
        if(failed(rc)) return rc;

        //Flush that much bits, as the leaf's path is
        bitstream_discard_bits(bs, depth);

        *(target++) = leaf->symbol;
        (*len)++;

        if(symbol_limit != -1) {
            symbol_limit--;
        }
    }

    return RC_OK;
}

MMFRES vlc_tree_create2(const VLCPrefixEntry *prefixes, int32_t element_count, VLCTreeNode **tree_root)
{
    //Find number of elements if element_count=0
    if(element_count == 0) {
        VLCPrefixEntry *p = prefixes;
        while((p)->bit_count > 0) {
            element_count++;
            p++;
        }
    }

    char *sym_tbl = mmf_alloc(sizeof(char) * element_count);
    uint64_t *bitcode_tbl = mmf_alloc(sizeof(uint64_t) * element_count);
    uint8_t *bitlen_tbl = mmf_alloc(sizeof(uint8_t) * element_count);

    int i;
    for(i=0; i<element_count; i++) {
        sym_tbl[i] = prefixes[i].symbol;
        bitcode_tbl[i] = prefixes[i].bits;
        bitlen_tbl[i] = prefixes[i].bit_count;
    }

    MMFRES rc = vlc_tree_create(sym_tbl, bitcode_tbl, bitlen_tbl, element_count, tree_root);

    //Clean-up
    mmf_free(sym_tbl);
    mmf_free(bitcode_tbl);
    mmf_free(bitlen_tbl);

    return rc;
}
