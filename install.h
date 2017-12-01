#ifndef __INSTALL_H__
#define __INSTALL_H__

/** @file install.h
 *  @brief Defines default directory where LC3 OS files are located
 *  @details Allows install location to be changed without recompiling
 *  the lc3sim.c file. Thus, code can be provided as an archive instead of
 *  as source.
 * @author Fritz Sieker
 */

extern const char *install_dir; /**< default path to LC3 OS object file */

#endif

