#ifndef COMP2017_ASSIGNMENT_3_CONFIG_H
#define COMP2017_ASSIGNMENT_3_CONFIG_H

#include "../memory/memory.h"
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/stat.h>

typedef struct {
    struct in_addr ip_addr;
    uint16_t port;
    char *dir;
} Config;

/** @brief Reads configuration file.
 *
 *  Reads config binary. Data is stored in Config instance and returned.
 *
 * @param config_path : Configuration file path.
 * @param Config data parsed from file.
 */
Config *load_config(char *config_path);

/** @brief Destroys config instance.
 *
 *  All dynamically allocated memory is released.
 *
 * @param config : Config instance.
 */
void destroy_config(Config *config);

#endif //COMP2017_ASSIGNMENT_3_CONFIG_H
