/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#ifndef _KERNEL_PLATFORM_H
#define _KERNEL_PLATFORM_H
#include <stdbool.h>

#include <drivers/pci-msi.h>

int platform_allocate_msi_interrupts(unsigned int num_vectors, bool addr64, 
                                     struct pci_msi_data *data);
#endif
