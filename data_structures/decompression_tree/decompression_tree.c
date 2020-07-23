#include "decompression_tree.h"

DecompressionTreeNode *init_decompression_tree(CompressionSegment *comp_dict) {
    if (!comp_dict) {
        return NULL;
    }

    DecompressionTreeNode *root = init_decompression_tree_node();

    for (size_t i = 0; i < COMPRESSION_DICT_LEN; ++i) {
        decompression_tree_insert(root, comp_dict[i]);
    }

    return root;
}

DecompressionTreeNode *init_decompression_tree_node() {
    DecompressionTreeNode *root = safe_malloc(sizeof(DecompressionTreeNode));
    root->left = NULL;
    root->right = NULL;

    return root;
}

void decompression_tree_insert(DecompressionTreeNode *root, CompressionSegment cs) {
    if (!root) {
        return;
    }

    DecompressionTreeNode *node = root;
    // iterate over each compression bit
    for (size_t b = 0; b < cs.compressed_len; ++b) {
        if ((cs.compressed >> (cs.compressed_len - 1 - b)) & 0x1) {
            if (!node->right) {
                node->right = init_decompression_tree_node();
            }
            // go right, bit is 1
            node = node->right;
        } else {
            if (!node->left) {
                node->left = init_decompression_tree_node();
            }
            // go left, bit is 0
            node = node->left;
        }
    }

    // at leaf node
    node->data = cs.uncompressed;
    return;
}

void destroy_decompression_tree(DecompressionTreeNode *root) {
    if (!root) {
        return;
    }

    // destroy in post order
    destroy_decompression_tree(root->left);
    destroy_decompression_tree(root->right);
    free(root);
    return;
}