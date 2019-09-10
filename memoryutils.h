#ifndef MEMORYUTILS_H
#define MEMORYUTILS_H

#pragma once

extern void *LookupSignature(void *address, char *signature);
extern void *GetFuncAtOffset(void *ptr, int offset);

#endif