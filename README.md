Fast Approximate Filtering of Search Results Sorted by Attribute
====

This code was used in the experiments of the following paper.

* Franco Maria Nardini, Roberto Trani, Rossano Venturini, _"Fast Approximate Filtering of Search Results Sorted by Attribute"_, ACM SIGIR 2019.

In addition to the strategies used in the paper above, we add here an optimized version of the topk pruning, which came out just after the publication to SIGIR.
This optimized version of the topk is slightly faster (in practice) than the previous one and suffers of the same weak performance guarantees.

Building the code
-----------------

The code is tested on Linux with GCC 5.4 and OSX Mojave with Clang.

The following dependency is needed for the build.

* CMake >= 3.5, for the build system

To build the code:

    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ make


Usage
-----------------------

    ./assessment [OPTION...] [FILES...]
    
      -h, --help                Print this help message
      -m, --metric arg          The search quality metric to use. Available options are: dcg, dcglz (default: dcg)
      -n, --n_cut_list arg      Truncate all lists to the first n elements, if n is greater than zero (default: 0,10000)
      -k, --k_list arg          Maximum number of elements to return (default: 50,100)
      -e, --epsilon_list arg    Target approximation factor (default: 0.1,0.01)
          --skip-shorter-lists  Skips the lists shorter than n elements (default: true)
          --test-cutoff         Test the cutoff-opt strategy (default: true)
          --test-topk           Test the topk-opt strategy (default: false)
          --test-topk-optimized Test the topk(optimized)-opt strategy (default: true)
          --test-epsfiltering   Test the epsilon filtering strategy (default: true)
          --runs arg            Number of times each test must be repeated (default: 5)
          --cpu-affinity arg    Set the cpu affinity of the process (default: -1)
          --show-progress       Show the computation progress (default: true)
      -o, --output arg          Write result to FILE instead of standard output


Input formats
-----------------------

The lists of results to filter can passed to the program either via file(s) or via standard input.

In the first case it is sufficient to pass the file paths as arguments, e.g., `./assessment FILE1 [FILE2]...`.
The files must be in the tsv format where each row contains `document id`, `attribute value`, and `estimated relevance` separated by a tab.

The second case, i.e., input lists passed via standard input, is the standard behaviour when no file paths are passed, e.g., `./assessment`.
In this case, the input must be formatted as follows:
* number `n` of lists in the first line
* `n` blocks with the following format
    * number `ni` of results in the first line of the block
    * `ni` rows containing `document id`, `attribute value`, and `estimated relevance` separated by a tab.

For both previous cases, `attribute value` and `estimated relevance` must be floating point values, while `document id` can be any string.


Authors
-------

* Franco Maria Nardini, ISTI-CNR, <francomaria.nardini@isti.cnr.it>
* Roberto Trani, ISTI-CNR, <roberto.trani@isti.cnr.it>
* Rossano Venturini, Dept. of Computer Science, University of Pisa, <rossano@di.unipi.it>
