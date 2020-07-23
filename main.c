//
// Created by eric on 15/5/20.
//

#include "./config/config.h"
#include "./jxserver/server.h"
#include "./data_structures/compression_dictionary/compression_dict.h"
#include "./data_structures/decompression_tree/decompression_tree.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    Config *config = load_config(argv[1]);
    CompressionSegment *dict = parse_compression_dictionary();
    DecompressionTreeNode *decom_tree = init_decompression_tree(dict);
    listen_and_serve(config, dict, decom_tree);
    return 0;
}