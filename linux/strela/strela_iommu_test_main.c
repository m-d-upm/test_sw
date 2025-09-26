// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include <stdio.h>

#include "conv2d.h"
#include "relu.h"

int main(int argc, char* argv[])
{
    printf("Running conv2d test...\r\n");
    conv2d_test();
    
    //printf("Running relu test...\r\n");
    //relu_test();

    return 0;
}
