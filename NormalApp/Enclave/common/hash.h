#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdio.h>

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);
void hashlittle2(const void *key, size_t length, uint32_t *pc, uint32_t *pb);

#include "hash.cpp"

#endif