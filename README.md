Copyright (c) 2015 The University of Edinburgh.
 
This software was developed as part of the                       
EC FP7 funded project Adept (Project ID: 610490)                 
    www.adept-project.eu                                            

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Adept Nano Benchmark

To compile use:
make clean && make [HWMEAS='1'] [MEAS='xyz']

HWMEAS and MEAS are optional values.
If HWMEAS is defined, the Adept Measurement System library is linked and the relevant calls are enabled in the code.
If MEAS is defined then MEAS will be inserted into the code as the instruction to measure. If MEAS is not defined, an add of eax and ebx is inserted. If MEAS is defined as blank, no instruction is inserted but memory is marked as clobbered.

Example:
make clean && make MEAS='asm volatile("vaddpd %%ymm0,%%ymm1,%%ymm1\n\t" ::: "ymm1", "memory");'

