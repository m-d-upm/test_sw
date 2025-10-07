// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include <stdio.h>

#include <pthread.h>

#include "conv2d.h"
#include "relu.h"

int main(int argc, char* argv[])
{
    pthread_t conv2d_test_th;
    pthread_t relu_test_th;

    //printf("Running conv2d test...\r\n");
    //conv2d_test();
    
    //printf("Running relu test...\r\n");
    //relu_test();

    pthread_create(&conv2d_test_th, NULL, conv2d_test, NULL);
    pthread_create(&relu_test_th, NULL, relu_test, NULL);
    
    pthread_join(conv2d_test_th, NULL);
    pthread_join(relu_test_th, NULL);

    return 0;
}
