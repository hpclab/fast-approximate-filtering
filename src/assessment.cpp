#include <algorithm>
#include <cfloat>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unordered_set>

#include "filtering/filter.hpp"
#include "filters/filter_spirin.hpp"
#include "filtering/pruner.hpp"
#include "filtering/search_quality_metric.hpp"
#include "pruners/pruner_cutoff.hpp"
#include "pruners/pruner_epspruning.hpp"
#include "pruners/pruner_topk.hpp"
#include "utils/composition.hpp"
#include "utils/cxxopts.hpp"
#include "utils/utils.hpp"


template <typename ScoreFun>
int
assessment(
        const cxxopts::ParseResult &arguments
) {
    // parameters
    std::vector<std::string> param_file_path_list;
    std::vector<index_type>  param_n_cut_list;
    std::vector<k_type>      param_k_list;
    std::vector<score_type>  param_epsilon_list;
    const bool  param_skip_shorter_lists = arguments["skip-shorter-lists"].as<bool>();
    const int   param_num_runs = arguments["num-runs"].as<int>();
    const bool  param_check_solutions = arguments["check-solutions"].as<bool>();
    const int   param_show_progress = arguments["show-progress"].as<bool>();
    std::ofstream * param_ofstream = nullptr;

    // check the command line parameters
    try {
        if (arguments.count("positional")) {
            param_file_path_list = arguments["positional"].as<std::vector<std::string>>();
            for (const std::string &file_path: param_file_path_list) {
                std::ifstream infile(file_path);
                if (!infile.is_open()) {
                    throw std::runtime_error(std::string("Unable to open the file ") + file_path);
                }
            }
        }

        // param n
        param_n_cut_list = read_parameter_list<index_type>(arguments["n-cut-list"].as<std::string>());
        if (param_n_cut_list.empty()) {
            throw std::runtime_error("The parameter n_cut_list is empty");
        }
        std::sort(param_n_cut_list.begin(), param_n_cut_list.end());
        for (std::size_t ni=1; ni < param_n_cut_list.size(); ++ni) {
            if (param_n_cut_list[ni] == param_n_cut_list[ni-1]) {
                throw std::runtime_error("The parameter n_cut_list contains duplicates");
            }
            if (param_n_cut_list[ni] <= 0 && param_n_cut_list[ni-1] <= 0) {
                throw std::runtime_error("The parameter n_cut_list can contain only one non-positive number");
            }
        }
        if (param_n_cut_list[0] <= 0) {
            for (std::size_t ni=1; ni < param_n_cut_list.size(); ++ni) {
                param_n_cut_list[ni-1] = param_n_cut_list[ni];
            }
            param_n_cut_list[param_n_cut_list.size()-1] = 0;
        }
        // param k
        param_k_list = read_parameter_list<k_type>(arguments["k-list"].as<std::string>());
        if (param_k_list.empty()) {
            throw std::runtime_error("The parameter k_list is empty");
        }
        std::sort(param_k_list.begin(), param_k_list.end());
        for (std::size_t ki=0; ki < param_k_list.size(); ++ki) {
            if (param_k_list[ki] <= 0) {
                throw std::runtime_error("The parameter k_list must contain values strictly greater than 0");
            }
            if (ki > 0 && param_k_list[ki-1] == param_k_list[ki]) {
                throw std::runtime_error("The parameter k_list contains duplicates");
            }
        }
        if (param_n_cut_list[0] > 0 and param_k_list[0] > param_n_cut_list[0]) {
            throw std::runtime_error("The parameter k_list cannot be greater than n");
        }
        // param e
        param_epsilon_list = read_parameter_list<score_type>(arguments["epsilon-list"].as<std::string>());
        if (param_epsilon_list.empty()) {
            throw std::runtime_error("The parameter epsilon_list is empty");
        }
        std::sort(param_epsilon_list.begin(), param_epsilon_list.end(), [](score_type a, score_type b) {return b < a;});
        for (std::size_t ei=0; ei < param_epsilon_list.size(); ++ei) {
            if (param_epsilon_list[ei] <= 0 || param_epsilon_list[ei] >= 1) {
                throw std::runtime_error("The parameter epsilon_list must contain values between zero and one");
            }
            if (ei > 0 && param_epsilon_list[ei-1] == param_epsilon_list[ei]) {
                throw std::runtime_error("The parameter epsilon_list contains duplicates");
            }
        }

        // param num runs
        if (arguments.count("num-runs") && param_num_runs <= 0) {
            throw std::runtime_error("The parameter runs must be a number strictly greater than 0");
        }

        // set the cpu-affinity, if required
        if (arguments.count("cpu-affinity")) {
            int cpu_affinity = arguments["cpu-affinity"].as<int>();
            if (cpu_affinity > -1) {
#ifdef __linux__
                cpu_set_t mask;
                int status;

                CPU_ZERO(&mask);
                CPU_SET(cpu_affinity, &mask);
                status = sched_setaffinity(0, sizeof(mask), &mask);
                if (status != 0) {
                    throw std::runtime_error(std::string("Unable to set the cpu affinity: ") + std::strerror(errno));
                }
#else
                throw std::runtime_error("The cpu affinity can be set only on linux");
#endif
            }
        }

        // param output
        if (arguments.count("output")) {
            std::string output_file_path = arguments["output"].as<std::string>();
            param_ofstream = new std::ofstream(output_file_path);
            if (!param_ofstream->is_open()) {
                throw std::runtime_error(std::string("Unable to open the output file ") + output_file_path);
            }
        }
    } catch (std::runtime_error & e) {
        std::cerr << e.what() << "." << std::endl;
        return -1;
    }

    const std::size_t n_cut_list_size = param_n_cut_list.size();
    const std::size_t k_list_size = param_k_list.size();

    // TEST CONFIGURATION
    std::shared_ptr<ScoreFun> score_fun = std::make_shared<ScoreFun>(*std::max_element(param_k_list.begin(), param_k_list.end()));
    std::vector<std::shared_ptr<FilterSpirin<ScoreFun>>> filters_list;
    for (auto k: param_k_list) {
        filters_list.push_back(std::shared_ptr<FilterSpirin<ScoreFun>>(new FilterSpirin<ScoreFun>(k, score_fun)));
    }

    typedef PrunerFilterCompositionTest<ScoreFun> composition_test;
    typedef std::shared_ptr<composition_test> sh_composition_test;

    sh_composition_test tests_opt[k_list_size];
    std::vector<sh_composition_test> tests_list[k_list_size];

    // loop over the different values of k
    for (std::size_t ki=0; ki < k_list_size; ++ki) {
        k_type k = param_k_list[ki];

        tests_opt[ki] = sh_composition_test(
                new composition_test("OPT", nullptr, filters_list[ki], param_num_runs)
        );

        if (arguments["test-cutoff"].as<bool>()) {
            tests_list[ki].emplace_back(sh_composition_test(
                    new composition_test("Cutoff-OPT", std::make_shared<PrunerCutoff<ScoreFun>>(score_fun), filters_list[ki], param_num_runs, 1.0)
            ));
        }

        if (arguments["test-topk"].as<bool>()) {
            tests_list[ki].emplace_back(sh_composition_test(
                    new composition_test("Topk-OPT", std::make_shared<PrunerTopk<ScoreFun>>(score_fun, k), filters_list[ki], param_num_runs, 0.5)
            ));
        }

        if (arguments["test-epsfiltering"].as<bool>()) {
            for (auto epsilon: param_epsilon_list) {
                std::ostringstream name; name << "EpsFiltering (epsilon=" << epsilon << ")";
                tests_list[ki].emplace_back(sh_composition_test(
                        new composition_test(name.str(), std::make_shared<PrunerEpsPruning<ScoreFun>>(score_fun, k, epsilon), filters_list[ki], param_num_runs, epsilon)
                ));
            }
        }
    }

    // read the number of input lists from the input stream
    std::size_t num_lists;
    const bool use_files = param_file_path_list.size();
    if (use_files) {
        std::vector<std::string> new_file_list;
        for (const std::string &file_path: param_file_path_list) {
            struct stat s;
            if (stat(file_path.c_str(), &s) == 0) {
                if (s.st_mode & S_IFDIR) {
                    // it's a directory
                    throw std::runtime_error(std::string("The following file is a directory: ") + file_path);
                } else if (s.st_mode & S_IFREG) {
                    // it's a file
                    new_file_list.push_back(file_path);
                } else {
                    // something else
                    throw std::runtime_error(std::string("Unable to recognize the file: ") + file_path);
                }
            } else {
                throw std::runtime_error(std::string("Unable to access the stats of the file: ") + file_path);
            }
        }
        param_file_path_list.swap(new_file_list);
        num_lists = param_file_path_list.size();
    } else {
        if (!(std::cin >> num_lists)) {
            throw std::runtime_error(
                    "The input stream is not properly formatted. Unable to extract the number of lists");
        }
        if (std::cin.peek() != '\n') {
            throw std::runtime_error(std::string(
                    "The input is not properly formatted. A new line is missing after the number of lists (first line)"));
        }
        std::cin.ignore();
    }


    // PROCESS a list at a time
    TestsAggregationOutcome aggregated_outcome_opt[n_cut_list_size][k_list_size];
    TestsAggregationOutcome aggregated_outcome_list[n_cut_list_size][k_list_size][tests_list[0].size()];
    std::size_t aggregated_num_lists_assessed[n_cut_list_size][k_list_size];
    double aggregated_avg_reading_time[n_cut_list_size][k_list_size];
    for (std::size_t ni = 0; ni < n_cut_list_size; ++ni) {
        for (std::size_t ki = 0; ki < k_list_size; ++ki) {
            aggregated_num_lists_assessed[ni][ki] = 0;
            aggregated_avg_reading_time[ni][ki] = 0.0;
        }
    }

    for (std::size_t i=0; i < num_lists; ++i) {
        if (param_show_progress) {
            std::cout << i << " of " << num_lists << "\r";
            std::cout.flush();
        }

        // read the input
        std::ifstream istream_file(nullptr);
        if (use_files) {
            istream_file = std::ifstream(param_file_path_list[i]);
        }

        ResultsList resultsList = read_results_list(
                (!use_files) ? std::cin : istream_file,
                use_files
        );

        if (use_files) {
            istream_file.close();
        }

        const relevance_type *rel_list = resultsList.relevances.data();
        const std::size_t rel_list_len = resultsList.size();

        // loop over the different cuts of n
        for (std::size_t ni = 0; ni < n_cut_list_size; ++ni) {
            index_type n_cut = param_n_cut_list[ni];

            // check the skip_shorter_lists constraint
            const std::size_t n = (n_cut > 0) ? std::min(rel_list_len, static_cast<std::size_t>(n_cut)) : rel_list_len;
            if (param_skip_shorter_lists and n_cut > n) {
                continue;
            }
            if (n == 0) {
                continue;
            }

            // compute min and max elements of the list (this is something that could be done during the sort by attribute)
            minmax_type minmax_element;
            minmax_element.min = minmax_element.max = rel_list[0];
            for (index_type j = 1; j < n; ++j) {
                if (rel_list[j] < minmax_element.min) {
                    minmax_element.min = rel_list[j];
                } else if (rel_list[j] > minmax_element.max) {
                    minmax_element.max = rel_list[j];
                }
            }

            // read time
            double reading_time = get_time_milliseconds();
            for (int attempt=0; attempt < param_num_runs; ++attempt) {
                for (std::size_t i = 0; i < n; ++i) {
                    doNotOptimizeAway(rel_list[i]);
                }
            }
            reading_time = (get_time_milliseconds() - reading_time) / param_num_runs;

            // loop over the different values of k
            for (std::size_t ki = 0; ki < k_list_size; ++ki) {
                // skip the combination n_cut smaller than k
                if (n_cut > 0 && param_k_list[ki] > n_cut) {
                    continue;
                }

                TestOutcome outcome = tests_opt[ki]->operator()(rel_list, n, minmax_element);
                score_type optimal_score = outcome.score;

                // optimal filtering
                aggregated_outcome_opt[ni][ki].update_aggregation(outcome, aggregated_num_lists_assessed[ni][ki], -1);
                if (param_check_solutions) {
                    try {
                        check_solution(outcome.score, rel_list, outcome.indices, score_fun.get(), -1);
                    } catch (CheckSolutionException e) {
                        std::ostringstream error;
                        error << e.what() << ". " << tests_opt[ki]->name << " with n=" << param_n_cut_list[ni] << " and k=" << param_k_list[ki] << " on the list ";
                        if (use_files) {
                            error << "'" <<param_file_path_list[i] << "'";
                        } else {
                            error << i;
                        }
                        throw CheckSolutionException(error.str());
                    }
                }
                // all others
                for (std::size_t j=0; j < tests_list[ki].size(); ++j) {
                    outcome = tests_list[ki][j]->operator()(rel_list, n, minmax_element);
                    aggregated_outcome_list[ni][ki][j].update_aggregation(outcome, aggregated_num_lists_assessed[ni][ki], optimal_score);
                    if (param_check_solutions) {
                        try {
                            check_solution(outcome.score, rel_list, outcome.indices, score_fun.get(), optimal_score, tests_list[ki][j]->epsilon_below, tests_list[ki][j]->epsilon_above);
                        } catch (CheckSolutionException e) {
                            std::ostringstream error;
                            error << e.what() << ". " << tests_list[ki][j]->name << " with n=" << param_n_cut_list[ni] << " and k=" << param_k_list[ki] << " on the list ";
                            if (use_files) {
                                error << "'" <<param_file_path_list[i] << "'";
                            } else {
                                error << i;
                            }
                            throw CheckSolutionException(error.str());
                        }
                    }
                }

                // update reading time and aggregated_num_lists_assessed
                {
                    double new_multiplier = 1.0 / (aggregated_num_lists_assessed[ni][ki] + 1);
                    double old_multiplier = aggregated_num_lists_assessed[ni][ki] * new_multiplier;

                    aggregated_num_lists_assessed[ni][ki] += 1;
                    aggregated_avg_reading_time[ni][ki] = old_multiplier * aggregated_avg_reading_time[ni][ki] + new_multiplier * reading_time;
                }

            }
        }
    }
    if (param_show_progress) {
        std::cout << num_lists << " of " << num_lists << "\r";
        std::cout << std::endl;
        std::cout.flush();
    }


    // WRITE the output
    // select the output stream
    std::ostream & ostream = (param_ofstream != nullptr) ? *param_ofstream : std::cout;

    ostream << "[" << std::endl;
    // loop over the different cuts of n
    for (std::size_t ni = 0; ni < n_cut_list_size; ++ni) {
        // loop over the different values of k
        for (std::size_t ki = 0; ki < k_list_size; ++ki) {
            // skip the combination n_cut smaller than k
            if (param_n_cut_list[ni] > 0 && param_k_list[ki] > param_n_cut_list[ni]) {
                continue;
            }

            ostream << "{" << std::endl;
            ostream << "\t\"n_cut\": " << param_n_cut_list[ni];
            ostream << ", \"k\": " << param_k_list[ki];
            ostream << ", \"avg_reading_time\": " << aggregated_avg_reading_time[ni][ki];
            ostream << ", \"num_lists_assessed\": " << aggregated_num_lists_assessed[ni][ki];
            ostream << ", \"strategies\": {";

            // optimal filtering
            ostream << std::endl << "\t\t\"" << tests_opt[ki]->name << "\": " << aggregated_outcome_opt[ni][ki];
            // all others
            for (std::size_t j=0; j < tests_list[ki].size(); ++j) {
                ostream << "," << std::endl << "\t\t\"" << tests_list[ki][j]->name << "\": " << aggregated_outcome_list[ni][ki][j];
            }

            ostream << std::endl << "\t}" << std::endl;
            ostream << "}";
            if (ki < (k_list_size-1) || ni < (n_cut_list_size-1)) {
                ostream << ",";
            }
            ostream << std::endl;
        }
    }
    ostream << "]" << std::endl;

    // close the file output stream
    if (param_ofstream != nullptr) {
        param_ofstream->close();
        delete(param_ofstream);
    }

    return 0;
}


int main(int argc, char *argv[]) {
    // command line options
    cxxopts::Options options(argv[0], "Tests the many filtering strategies and prints the performance results");
    options
            .add_options()
            ("h, help", "Print this help message")
            ("m, metric", "The search quality metric to use. Available options are: dcg, dcglz", cxxopts::value<std::string>()->default_value("dcg"))
            ("n, n-cut-list", "Truncate all lists to the first n elements, if n is greater than zero", cxxopts::value<std::string>()->default_value("0,10000"))
            ("k, k-list", "Maximum number of elements to return", cxxopts::value<std::string>()->default_value("50,100"))
            ("e, epsilon-list", "Target approximation factor", cxxopts::value<std::string>()->default_value("0.1,0.01"))
            ("s, skip-shorter-lists", "Skips the lists shorter than n elements", cxxopts::value<bool>()->default_value("true"))
            ("r, num-runs", "Number of times each test must be repeated", cxxopts::value<int>()->default_value("5"))
            ("a, cpu-affinity", "Set the cpu affinity of the process", cxxopts::value<int>()->default_value("-1"))
            ("c, check-solutions", "Check all solutions", cxxopts::value<bool>()->default_value("false"))
            ("p, show-progress", "Show the computation progress", cxxopts::value<bool>()->default_value("true"))
            ("o, output", "Write result to FILE instead of standard output", cxxopts::value<std::string>())
            ("test-cutoff", "Test the cutoff-opt strategy", cxxopts::value<bool>()->default_value("true"))
            ("test-topk", "Test the topk-opt strategy", cxxopts::value<bool>()->default_value("true"))
            ("test-epsfiltering", "Test the epsilon filtering strategy", cxxopts::value<bool>()->default_value("true"));
    options
            .add_options("hidden")
            ("positional", "Positional arguments: these are the arguments that are entered without an option", cxxopts::value<std::vector<std::string>>());

    options.parse_positional({"positional"});

    // command line parsing
    cxxopts::ParseResult arguments = options.parse(argc, argv);

    // help
    if (arguments.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // call the templated proxy based on the selected metric function
    std::string param_metric = arguments["metric"].as<std::string>();
    if (param_metric == "dcg") {
        return assessment<dcg_metric>(arguments);
    } else if (param_metric == "dcglz") {
        return assessment<dcglz_metric>(arguments);
    } else {
        std::cerr << "The given metric is unavailable." << std::endl;
        return -1;
    }
}
