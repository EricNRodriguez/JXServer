#ifndef ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_DECOMPRESSION_TREE_H
#define ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_DECOMPRESSION_TREE_H

#include "../compression_dictionary/compression_dict.h"

#include <stdbool.h>

#define INITIAL_TREE_SIZE 64
#define TREE_GROWTH_RATE 2

struct decompression_tree_node {
    uint8_t data;
    struct decompression_tree_node *left;
    struct decompression_tree_node *right;
};
typedef struct decompression_tree_node DecompressionTreeNode;

/** @brief Creates decompression tree from compression dictionary.
 *
 *  Assumes no one compression code is a prefix of another compression code.
 *  If comp_dict is NULL, nothing is done and NULL is returned.
 *
 *  @param comp_dict : Compression dictionary instance.
 *  @return Decompression tree corresponding to the provided compression
 *          dictionary.
 */
DecompressionTreeNode *init_decompression_tree(CompressionSegment *comp_dict);

/** @brief Initialises decompression tree node.
 *  @return Decompression tree node.
 */
DecompressionTreeNode *init_decompression_tree_node();

/** @brief Inserts node into tree.
 *
 *  Assumes no one compression code is a prefix of another compression code.
 *  Path followed through the tree corresponds to 'compressed' field of
 *  provided compression segment. Value of node is set to the uncompressed
 *  value.
 *
 *  @param root : Decompression tree root node.
 *  @param cs : Compression segment.
 */
void decompression_tree_insert(DecompressionTreeNode *root, CompressionSegment cs);

/** @brief Destroys decompression tree.
 *
 *  If root is NULL, nothing is done. All allocated memory, including nodes
 *  and fields of nodes, are released.
 *
 *  @param root : Decompression tree root node.
 */
void destroy_decompression_tree(DecompressionTreeNode *root);

#endif //ASSIGNMENT_3_JXSERVER_FINAL_SUBMISSION_DECOMPRESSION_TREE_H
