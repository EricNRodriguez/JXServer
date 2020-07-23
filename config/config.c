#include "config.h"

/** @brief Coppies string from file.
 *
 *  String is coppied to heap, and NULL terminated.
 *
 *  @param f : Open file.
 *  @param n_bytes : String size.
 */
static char *read_string(FILE *f, size_t n_bytes);

Config *load_config(char *config_path) {
    if (access(config_path, F_OK)) {
        printf("unable to load configuration file | does not exist\n");
        exit(EXIT_FAILURE);
    }

    // read binary
    FILE *config_file = fopen(config_path, "rb");
    // get file size
    fseek(config_file, 0, SEEK_END);
    long file_size = ftell(config_file);
    fseek(config_file, 0, SEEK_SET);

    Config *config = safe_malloc(sizeof(Config));
    // read 32 bit ip
    fread(&config->ip_addr.s_addr, 4, 1, config_file);
    // read 2 byte tcp port
    fread(&config->port, 2, 1, config_file);
    // read and null terminate directory path
    size_t str_size = file_size - sizeof(config->port) - sizeof(config->ip_addr.s_addr);
    config->dir = read_string(config_file, str_size);

    fclose(config_file);
    return config;
}

static char *read_string(FILE *f, size_t n_bytes) {
    char *string = safe_calloc((n_bytes + 1), sizeof(char *));
    string[n_bytes] = '\0';
    // read string
    fread(string, n_bytes, 1, f);
    return string;
}

void destroy_config(Config *config) {
    if (!config) {
        return;
    }

    free(config->dir);
    free(config);
    return;
}