//
// Copyright (c) 2017 The Khronos Group Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "harness/compat.h"
#include "harness/testHarness.h"

#include "testBase.h"

#if DENSE_PACK_VECS
const int g_vector_aligns[] = {0, 1, 2, 3, 4,
                               5, 6, 7, 8,
                               9, 10, 11, 12,
                               13, 14, 15, 16};

#else
const int g_vector_aligns[] = {0, 1, 2, 4, 4,
                               8, 8, 8, 8,
                               16, 16, 16, 16,
                               16, 16, 16, 16};
#endif


const int g_vector_allocs[] = {0, 1, 2, 4, 4,
                               8, 8, 8, 8,
                               16, 16, 16, 16,
                               16, 16, 16, 16};


int main(int argc, const char *argv[])
{
    return runTestHarness(argc, argv, test_registry::getInstance().num_tests(),
                          test_registry::getInstance().definitions(), false, 0);
}

