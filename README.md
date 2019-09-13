Fast Approximate Filtering of Search Results Sorted by Attribute
====

This is the C++ library implementing the ε-Filtering algorithm and all the baselines described in the paper [Fast Approximate Filtering of Search Results Sorted by Attribute](http://pages.di.unipi.it/rossano/wp-content/uploads/sites/7/2019/07/SIGIR19.pdf) by Franco Maria Nardini, Roberto Trani, and Rossano Venturini, published in ACM SIGIR 2019 [1].

The two datasets used to assess all filtering algorithms can be downloaded here: [AmazonRel](http://hpc.isti.cnr.it/~nardini/datasets/AmazonRel.tar.gz), [GoogleLocalRec](http://hpc.isti.cnr.it/~nardini/datasets/GoogleLocalRec.tar.gz).


Table of contents
-----------------------
- [Building the code](#building-the-code)
- [Usage assessment](#usage-assessment)
- [Usage filter](#usage-filter)
- [Input formats](#input-formats)
- [Datasets description](#datasets-description)
- [Datasets format](#datasets-format)
- [Acknowledgments](#acknowledgments)
- [Authors](#authors)
- [Bibliography](#bibliography)


Building the code
-----------------------

The code is tested on Linux with GCC 5.4 and OSX Mojave with Clang.

The following dependency is needed for the build.

* CMake >= 3.5, for the build system

To build the code:

```bash
git clone git@github.com:hpclab/fast-approximate-filtering.git
cd fast-approximate-filtering
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```


Usage `assessment`
-----------------------

The `assessment` command tests the many filtering strategies and prints the performance results.

Command line parameters are listed below.

    assessment [OPTION...] [FILES...]
    
      -h, --help                Print this help message
      -m, --metric arg          The search quality metric to use. Available options are: dcg, dcglz (default: dcg)
      -n, --n_cut_list arg      Truncate all lists to the first n elements, if n is greater than zero (default: 0,10000)
      -k, --k_list arg          Maximum number of elements to return (default: 50,100)
      -e, --epsilon_list arg    Target approximation factor (default: 0.1,0.01)
          --skip-shorter-lists  Skips the lists shorter than n elements (default: true)
          --test-cutoff         Test the cutoff-opt strategy (default: true)
          --test-topk           Test the topk-opt strategy (default: true)
          --test-epsfiltering   Test the epsilon filtering strategy (default: true)
          --runs arg            Number of times each test must be repeated (default: 5)
          --cpu-affinity arg    Set the cpu affinity of the process (default: -1)
          --show-progress       Show the computation progress (default: true)
      -o, --output arg          Write result to FILE instead of standard output

An example of output is the following one.

    [
    {
            "n_cut": 10000, "k": 10, "num_lists_assessed": 222, "strategies": {
                    "OPT": {"avg_score": 9.41093, "max_approximation_error": 0, "avg_approximation_error": 0, "avg_num_elements_pruned": 0, "avg_num_elements_not_pruned": 10000, "avg_first_stage_time": 0.00367355, "avg_second_stage_time": 0.0577718, "avg_total_time": 0.0614454},
                    "Cutoff-OPT": {"avg_score": 9.40668, "max_approximation_error": 0.0623043, "avg_approximation_error": 0.000586275, "avg_num_elements_pruned": 8101.18, "avg_num_elements_not_pruned": 1898.82, "avg_first_stage_time": 0.0169158, "avg_second_stage_time": 0.0111549, "avg_total_time": 0.0280707},
                    "Topk-OPT": {"avg_score": 9.31001, "max_approximation_error": 0.0953211, "avg_approximation_error": 0.0115233, "avg_num_elements_pruned": 9990, "avg_num_elements_not_pruned": 10, "avg_first_stage_time": 0.00979444, "avg_second_stage_time": 0.000198172, "avg_total_time": 0.00999261},
                    "EpsFiltering (epsilon=0.5)": {"avg_score": 6.28308, "max_approximation_error": 0.43983, "avg_approximation_error": 0.33194, "avg_num_elements_pruned": 9990.01, "avg_num_elements_not_pruned": 9.99099, "avg_first_stage_time": 0.00399522, "avg_second_stage_time": 0.000176727, "avg_total_time": 0.00417195},
                    "EpsFiltering (epsilon=0.1)": {"avg_score": 9.20887, "max_approximation_error": 0.0683661, "avg_approximation_error": 0.019979, "avg_num_elements_pruned": 9956.08, "avg_num_elements_not_pruned": 43.9234, "avg_first_stage_time": 0.00280179, "avg_second_stage_time": 0.000499718, "avg_total_time": 0.00330151},
                    "EpsFiltering (epsilon=0.01)": {"avg_score": 9.41088, "max_approximation_error": 0.000405073, "avg_approximation_error": 3.98411e-06, "avg_num_elements_pruned": 9928.27, "avg_num_elements_not_pruned": 71.7297, "avg_first_stage_time": 0.00524528, "avg_second_stage_time": 0.000675126, "avg_total_time": 0.00592041},
                    "EpsFiltering (epsilon=0.001)": {"avg_score": 9.41093, "max_approximation_error": 0, "avg_approximation_error": 0, "avg_num_elements_pruned": 9923.29, "avg_num_elements_not_pruned": 76.7117, "avg_first_stage_time": 0.00715013, "avg_second_stage_time": 0.000661049, "avg_total_time": 0.00781118},
            }
    },
    {
            "n_cut": 10000, "k": 20, "num_lists_assessed": 222, "strategies": {
                    "OPT": {"avg_score": 11.3355, "max_approximation_error": 0, "avg_approximation_error": 0, "avg_num_elements_pruned": 0, "avg_num_elements_not_pruned": 10000, "avg_first_stage_time": 0.00354752, "avg_second_stage_time": 0.108199, "avg_total_time": 0.111746},
                    ...
            }
    },
    ...
    ]


Usage `filter`
-----------------------

The `filter` command applies a filtering strategy to the input data and prints the list of ids to select. The default filtering strategy, i.e., when no other strategies have been selected, is the optimal filtering with no pruning.

Command line parameters are listed below.

    filter [OPTION...] positional parameters

      -h, --help               Print this help message
      -m, --metric arg         The search quality metric to use. Available
                               options are: dcg, dcglz (default: dcg)
      -n, --n-cut arg          Truncate all lists to the first n elements, if n
                               is greater than zero (default: 0)
      -k, arg                  Maximum number of elements to return (default: 50)
      -e, --epsilon arg        Target approximation factor (default: 0.01)
      -a, --cpu-affinity arg   Set the cpu affinity of the process (default: -1)
      -o, --output arg         Write result to FILE instead of standard output
          --test-cutoff        Test the cutoff-opt strategy
          --test-topk          Test the topk-opt strategy
          --test-epsfiltering  Test the epsilon filtering strategy


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

The `filter` command accepts a single list as input.
Thus only a single file can be passed or, in case of feeding from standard input, the number `n` of lists must be skipped from the input.



Datasets description
-----------------------

We evaluate the performance of ε-Filtering and its competitors on two real datasets, namely AmazonRel and GoogleLocalRec. The two datasets are built around two different real-world use cases and by exploiting state-of-the-art relevance estimators. Moreover, they present a high number of results per query thus allowing us to perform a comprehensive assessment of the efficiency of the filtering methods.

The **[AmazonRel](http://hpc.isti.cnr.it/~nardini/datasets/AmazonRel.tar.gz)** dataset is built by using one of the [winning solutions](https://github.com/geffy/kaggle-crowdflower) of the [Crowdflower Search Results Relevance](https://www.kaggle.com/c/crowdflower-search-relevance) Kaggle competition, whose goal was to create an open-source model to help small e-commerce sites to measure the relevance of the search results they provide. However, the test set distributed as part of the Kaggle competition contains only few results associated with each query. Therefore, we extended the test set using the Amazon dataset introduced by [4], whose data come from the [Amazon](https://www.amazon.com) e-commerce website and span from May 1996 to July 2014. We added to the original result list of each query all Amazon items having at least one of the query terms in the title or in the description. Then, we sorted the results by price and we computed their relevance by using the winning solution of the Crowdflower Search Results Relevance competition. The resulting AmazonRel dataset is composed of 260 queries. We then removed 10 queries characterized by less than 500 results. The resulting dataset is made up of 250 queries and each query comes with 100,000 results on average.

The **[GoogleLocalRec](http://hpc.isti.cnr.it/~nardini/datasets/GoogleLocalRec.tar.gz)** dataset is built by using the Google Local data and a state-of-the-art recommender system (TransFM) that recently achieved the best performance in the task of sequential recommendation [2], i.e., the task of predicting users' future behaviour given their historical interactions. The GoogleLocalRec dataset is made up of a large collection of temporal and geographical information of businesses and reviews [3]. We used the GoogleLocalRec dataset to produce, given a user, a list of relevant businesses to recommend to her. In particular, we focused on the city of New York, which is the city with the highest density of data.
Therefore, we preprocessed the dataset by following the methodology described in the original paper [2] and we trained a recommendation model (TransFM<sub>content</sub>) using the reviews of the businesses within a radius of 50Km from the center of New York. Then, we randomly selected 10,000 test users. For each test user, we sorted the businesses in New York by distance and we estimated their relevance by predicting the recommendation score with the TransFM<sub>content</sub> model. The GoogleLocalRec dataset we built is thus composed of 10,000 users, also referred to as queries. Each user comes with a list of about 16,000 recommendations.


Datasets format
-----------------------

The two datasets [AmazonRel](http://hpc.isti.cnr.it/~nardini/datasets/AmazonRel.tar.gz) and [GoogleLocalRec](http://hpc.isti.cnr.it/~nardini/datasets/GoogleLocalRec.tar.gz) have been compressed using the command `tar`, thus to download and decompress them use the following commands:

```bash
mkdir -p datasets
cd datasets

wget http://hpc.isti.cnr.it/~nardini/datasets/AmazonRel.tar.gz
tar -zxf AmazonRel.tar.gz
rm AmazonRel.tar.gz

wget http://hpc.isti.cnr.it/~nardini/datasets/GoogleLocalRec.tar.gz
tar -zxf GoogleLocalRec.tar.gz
rm GoogleLocalRec.tar.gz
```

The commands above will create a folder `datasets/AmazonRel` and a folder `datasets/GoogleLocalRec` containing one list of results per file, in `tsv` format.
The format of the files for two datasets is as follows.

**AmazonRel**:
The name of the file is the `query` performed on the amazon catalog. The file contains the list of results sorted by `price` and each row contains: `product_id<tab>price<tab>estimated_relevance`.

**GoogleLocalRec**:
The name of the file is the `user_id` of the user performing the request. The file contains the list of results sorted by `distance` and each row contains: `place_id<tab>distance<tab>estimated_relevance`. The distance refers to the distance between the `place_id` and the last place visited by the user.


Acknowledgments
-----------------------
If you use ε-Filtering, our code, or our datasets, please acknowledge [1].


Authors
-----------------------

* Franco Maria Nardini, ISTI-CNR, <francomaria.nardini@isti.cnr.it>
* Roberto Trani, ISTI-CNR, <roberto.trani@isti.cnr.it>
* Rossano Venturini, Dept. of Computer Science, University of Pisa, <rossano@di.unipi.it>


Bibliography
-----------------------

[1] Franco Maria Nardini, Roberto Trani, and Rossano Venturini. 2019. Fast Approximate Filtering of Search Results Sorted by Attribute. In Proceedings of the 42nd International ACM SIGIR Conference on Research and Development in Information Retrieval, SIGIR 2019, Paris, France, July 21-25, 2019., Benjamin Piwowarski, Max Chevalier, Éric Gaussier, Yoelle Maarek, Jian-Yun Nie, and Falk Scholer (Eds.). ACM, 815–824. DOI: [10.1145/3331184.3331227](http://dx.doi.org/10.1145/3331184. 3331227)

[2] Rajiv Pasricha and Julian McAuley. 2018. Translation-based factorization machines for sequential recommendation. In Proceedings of the 12th ACM Conference on Recommender Systems, RecSys 2018, Vancouver, BC, Canada, October 2-7, 2018, Sole Pera, Michael D. Ekstrand, Xavier Amatriain, and John O’Donovan (Eds.). ACM, 63–71. DOI: [10.1145/3240323.3240356](http://dx.doi.org/10.1145/3240323.3240356)

[3] Ruining He, Wang-Cheng Kang, and Julian McAuley. 2017. Translation-based Recommendation. In Proceedings of the Eleventh ACM Conference on Recommender Systems, RecSys 2017, Como, Italy, August 27-31, 2017, Paolo Cremonesi, Francesco Ricci, Shlomo Berkovsky, and Alexander Tuzhilin (Eds.). ACM, 161–169. DOI: [10.1145/3109859.3109882](http://dx.doi.org/10.1145/3109859.3109882)

[4] Julian J. McAuley, Christopher Targett, Qinfeng Shi, and Anton van den Hengel. 2015. Image-Based Recommendations on Styles and Substitutes. In Proceedings of the 38th International ACM SIGIR Conference on Research and Development in Information Retrieval, Santiago, Chile, August 9-13, 2015, Ricardo A. Baeza-Yates, Mounia Lalmas, Alistair Moffat, and Berthier A. Ribeiro-Neto (Eds.). ACM, 43–52. DOI: [10.1145/2766462.2767755](http://dx.doi.org/10.1145/2766462.2767755)