
===============================================================================
IPI testing Documentation
===============================================================================

TEST:	Call smp_call_function/_single on each CPU in the system to test if
	all other CPUs could receive the interrupts and records maximum time taken
	to receive IPI for each core.

		smp_call_function	 : sends IPI to all cores at once.
		smp_call_function_single : sends IPI to one core at a time.

Usage: ipi_test.sh [OPTIONS]
       Runs the test for inter-processor interrupts (IPI)

OPTIONS:
  The kernel test support about the following parameters

  -t | --times <iterations> : Sets how many times to test IPI between every 2 CPUs.

  -r | --repeatability : Sets 1000000 times as a large iteration number to test IPI.

  -e | --test_type <test_type> : Sets how to send IPI between cores.

  -s | --stress :Sets times and use both test_types to send IPI.

  -v | --verbose : Display max time taken for IPI handling on each cpu.


TEST BEHAVIOR:
    * Online CPUs and ensure all the CPUs are not removed. Test will call
    * smp_call_function on each online CPU to test if all other CPUs could
    * receive the interrupts.

Target support: all

OUTPUT:

# ./ipi_test.sh --help
 Usage: ./ipi_test.sh [[(-t | --times) <iterations>] [(-e | --test_type) <test_type>]
                 [-s | --stress] [-v | --verbose]
 iterations: the times number of ipi to test
 test_type:
1. Use smp_single: send IPI one by one to all cores
2. Use smp_many: send IPI to all cores at once
3. Use Both: (1 & 2)
Recommended maximum iterations: 1000000
Maximum time to complete the test: 10 mins


#./ipi_test.sh -t 10000 -e 3 -v
Removing core_ctl..
IPI test starting
Test Result: PASS
Max time taken for IPI: (nsec)
823490

Time taken to handle IPI's:
Test type IPI Single
        CPU0  |  CPU1  |  CPU2  |  CPU3  |  CPU4  | CPU5  | CPU6  |  CPU7
CPU0        0   155260   152969   106094   116979   120052   125156   128125
CPU1    60365        0   152344    71355    60834    67969    62188    62812
CPU2    60209    47760        0    52500    64011    65781    65209    70573
CPU3   139583   208177   178594        0    58542    57083    66510    69896
CPU4   280990   279687   282760   323385        0    65781    67552    95729
CPU5   280261   279896   291563   321563    65260        0    70364    76302
CPU6   280208   278802   296198   324323    60104    69739        0    73177
CPU7   279114   277292   296094   321407    63855    77500    74688        0

IPI Nos:
   0 10000 10000 10000 10000 10000 10000 10000
10000    0 10000 10000 10000 10000 10000 10000
10000 10000    0 10000 10000 10000 10000 10000
10000 10000 10000    0 10000 10000 10000 10000
10000 10000 10000 10000    0 10000 10000 10000
10000 10000 10000 10000 10000    0 10000 10000
10000 10000 10000 10000 10000 10000    0 10000
10000 10000 10000 10000 10000 10000 10000    0

Test type IPI Many
        CPU0  |  CPU1  |  CPU2  |  CPU3  |  CPU4  | CPU5  | CPU6  |  CPU7
CPU0        0   137656   375729   721927   810417   815261   819531   823490
CPU1    44218        0   135364   140833   144687   147396   151875   156666
CPU2    40573    63854        0    84479    98906   105260   113438   122969
CPU3    42187   144062   262083        0   267395   274427   280520   286614
CPU4   143177   249114   421823   541615        0   546510   553958   557552
CPU5   120677   328594   410364   533177   539323        0   547760   551042
CPU6   121719   255625   426979   546823   552344   557083        0   563542
CPU7   120521   265156   426771   546562   553281   556823   559271        0

IPI Nos:
   0 10000 10000 10000 10000 10000 10000 10000
10000    0 10000 10000 10000 10000 10000 10000
10000 10000    0 10000 10000 10000 10000 10000
10000 10000 10000    0 10000 10000 10000 10000
10000 10000 10000 10000    0 10000 10000 10000
10000 10000 10000 10000 10000    0 10000 10000
10000 10000 10000 10000 10000 10000    0 10000
10000 10000 10000 10000 10000 10000 10000    0


Notes:
If the script returns 0 and prints "Test passed", then the test was successful.
If the script returns nonzero and prints "Test failed", then the test failed.
