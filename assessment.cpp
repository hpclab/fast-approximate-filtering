#include <algorithm>
#include <cfloat>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "assessment.hpp"
#include "cxxopts.hpp"
#include "filter.hpp"
#include "filter_spirin.hpp"
#include "pruner.hpp"
#include "pruner_cutoff.hpp"
#include "pruner_topk.hpp"
#include "pruner_epspruning.hpp"
#include "search_quality_metric.hpp"
#include "utils.hpp"

void
update_multiple_test_outcome(
        const SingleTestOutcome &single_test_outcome,
        MultipleTestOutcome &multiple_test_outcome,
        const score_type *optimal_score,
        const std::size_t num_lists_assessed
) {
    double new_multiplier = 1.0 / (num_lists_assessed + 1.0);
    double old_multiplier = num_lists_assessed / (num_lists_assessed + 1.0);

    double approximation_error = 0;
    if (optimal_score != nullptr && *optimal_score > 0) {
        approximation_error = 1.0 - (single_test_outcome.score / *optimal_score);
    }
    if (approximation_error > multiple_test_outcome.max_approximation_error) {
        multiple_test_outcome.max_approximation_error = approximation_error;
    }
    // This kind of averaging introduces some error in the computation, but guarantees to not overflow with a huge number of updates
    multiple_test_outcome.avg_score = new_multiplier * single_test_outcome.score + old_multiplier * multiple_test_outcome.avg_score;
    multiple_test_outcome.avg_approximation_error = new_multiplier * approximation_error + old_multiplier * multiple_test_outcome.avg_approximation_error;

    multiple_test_outcome.avg_num_elements_pruned = new_multiplier * single_test_outcome.num_elements_pruned + old_multiplier * multiple_test_outcome.avg_num_elements_pruned;
    multiple_test_outcome.avg_num_elements_not_pruned = new_multiplier * single_test_outcome.indices.size() + old_multiplier * multiple_test_outcome.avg_num_elements_not_pruned;
    multiple_test_outcome.avg_first_stage_time = new_multiplier * single_test_outcome.first_stage_time + old_multiplier * multiple_test_outcome.avg_first_stage_time;
    multiple_test_outcome.avg_second_stage_time = new_multiplier * single_test_outcome.second_stage_time + old_multiplier * multiple_test_outcome.avg_second_stage_time;
    multiple_test_outcome.avg_total_time = new_multiplier * single_test_outcome.total_time + old_multiplier * multiple_test_outcome.avg_total_time;
}

void
print_json_multiple_test_outcome(
        MultipleTestOutcome &test_outcome,
        std::ostream &ostream
) {
    ostream << "{";

    ostream << "\"avg_score\": " << test_outcome.avg_score;
    ostream << ", \"max_approximation_error\": " << test_outcome.max_approximation_error;
    ostream << ", \"avg_approximation_error\": " << test_outcome.avg_approximation_error;
    ostream << ", \"avg_num_elements_pruned\": " << test_outcome.avg_num_elements_pruned;
    ostream << ", \"avg_num_elements_not_pruned\": " << test_outcome.avg_num_elements_not_pruned;
    ostream << ", \"avg_first_stage_time\": " << test_outcome.avg_first_stage_time;
    ostream << ", \"avg_second_stage_time\": " << test_outcome.avg_second_stage_time;
    ostream << ", \"avg_total_time\": " << test_outcome.avg_total_time;

    ostream << "}";
}

template <typename ScoreFun>
void
assessment(
        const std::vector<std::string> & file_path_list,
        std::ostream & ostream,
        const bool show_progress,
        const std::vector<index_type> & n_cut_list,
        const std::vector<k_type> & k_list,
        const std::vector<score_type> & epsilon_list,
        bool skip_shorter_lists,
        bool test_cutoff,
        bool test_topk,
        bool test_epsfiltering,
        int num_runs
) {
    ScoreFun score_fun(*std::max_element(k_list.begin(), k_list.end()));
    std::vector<FilterSpirin<ScoreFun>> filterSpirin_list;
    PrunerCutoff<ScoreFun> prunerCutoff(&score_fun);
    std::vector<PrunerTopk<ScoreFun>> prunerTopk_list;
    std::vector<
            std::vector<PrunerEpsPruning<ScoreFun>>
    > prunerEpsPruning_list_list(epsilon_list.size());

    for (k_type k: k_list) {
        filterSpirin_list.emplace_back(FilterSpirin<ScoreFun>(k, &score_fun));
        prunerTopk_list.emplace_back(PrunerTopk<ScoreFun>(&score_fun, k));
    }
    for (std::size_t ei=0; ei < epsilon_list.size(); ++ei) {
        for (k_type k: k_list) {
            prunerEpsPruning_list_list[ei].emplace_back(PrunerEpsPruning<ScoreFun>(&score_fun, k, epsilon_list[ei]));
        }
    }

    // test strategies
    typedef PrunerFilterCompositionTest<ScoreFun> Test;
    std::vector<Test> comp_opt_list;
    std::vector<Test> comp_cutoff_opt_list;
    std::vector<Test> comp_topk_opt_list;
    std::vector<std::vector<Test>> comp_epsfiltering_list_list(epsilon_list.size());

    for (std::size_t ki=0; ki < k_list.size(); ++ki) {
        comp_opt_list.emplace_back(Test(nullptr, &filterSpirin_list[ki], num_runs));
        comp_cutoff_opt_list.emplace_back(Test(&prunerCutoff, &filterSpirin_list[ki], num_runs));
        comp_topk_opt_list.emplace_back(Test(&prunerTopk_list[ki], &filterSpirin_list[ki], num_runs));
    }
    for (std::size_t ei=0; ei < epsilon_list.size(); ++ei) {
        for (std::size_t ki=0; ki < k_list.size(); ++ki) {
            comp_epsfiltering_list_list[ei].emplace_back(Test(&(prunerEpsPruning_list_list[ei][ki]), &filterSpirin_list[ki], num_runs));
        }
    }

    // outcomes
    std::size_t num_tests = n_cut_list.size() * k_list.size();
    std::vector<MultipleTestOutcome> test_opt_outcome_list(num_tests);
    std::vector<MultipleTestOutcome> test_cutoff_opt_outcome_list(num_tests);
    std::vector<MultipleTestOutcome> test_topk_opt_outcome_list(num_tests);
    std::vector<std::vector<MultipleTestOutcome>> test_epsfiltering_outcome_list_list(epsilon_list.size());
    for (std::size_t ei=0; ei < epsilon_list.size(); ++ei) {
        test_epsfiltering_outcome_list_list[ei].resize(num_tests);
    }

    // read the number of input lists from the input stream
    std::size_t num_lists;
    const bool use_files = file_path_list.size();
    if (use_files) {
        num_lists = file_path_list.size();
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

    // process a list at a time
    std::vector<std::size_t> num_lists_assessed_list(num_tests);
    for (std::size_t i=0; i < num_tests; ++i) {
        num_lists_assessed_list[i] = 0;
    }
    for (std::size_t i=0; i < num_lists; ++i) {
        if (show_progress) {
            std::cout << i << " of " << num_lists << "\r";
            std::cout.flush();
        }

        // read the input
        std::ifstream istream_file(nullptr);
        if (use_files) {
            istream_file = std::ifstream(file_path_list[i]);
        }

        ResultsList resultsList = read_results_list(
                (!use_files) ? std::cin : istream_file,
                use_files
        );

        if (use_files) {
            istream_file.close();
        }

        const relevance_type * relevances = resultsList.relevances.data();

        std::size_t index = 0;
        // loop over the different cuts of n
        for (index_type n_cut: n_cut_list) {
            // check the skip_shorter_lists constraint
            const std::size_t n = (n_cut > 0) ? std::min(resultsList.size(), static_cast<std::size_t>(n_cut)) : resultsList.size();
            if (skip_shorter_lists and n_cut > n) {
                index += k_list.size();
                continue;
            }
            if (n == 0) {
                index += k_list.size();
                continue;
            }

            // compute min and max elements of the list (this is something that could be done during the sort by attribute)
            minmax_type minmax_element;
            minmax_element.min = minmax_element.max = relevances[0];
            for (index_type j=1; j < n; ++j) {
                if (relevances[j] < minmax_element.min) {
                    minmax_element.min = relevances[j];
                } else if (relevances[j] > minmax_element.max) {
                    minmax_element.max = relevances[j];
                }
            }

            // loop over the different values of k
            for (std::size_t ki=0; ki < k_list.size(); ++ki) {
                // skip the combination n_cut smaller than k
                if (n_cut > 0 && k_list[ki] > n_cut) {
                    continue;
                }

                SingleTestOutcome outcome;
                score_type optimal_score;

                // OPT
                {
                    outcome = comp_opt_list[ki](relevances, n, minmax_element);
                    update_multiple_test_outcome(outcome, test_opt_outcome_list[index], nullptr,
                                                 num_lists_assessed_list[index]);
#ifdef DEBUG
                    check_solution(relevances, outcome.score, outcome.indices, score_fun);
#endif
                    optimal_score = outcome.score;
                }

                // Cutoff-OPT
                if (test_cutoff) {
                    outcome = comp_cutoff_opt_list[ki](relevances, n, minmax_element);
                    update_multiple_test_outcome(outcome, test_cutoff_opt_outcome_list[index], &optimal_score,
                                                 num_lists_assessed_list[index]);
                }

                // Topk-OPT
                if (test_topk) {
                    outcome = comp_topk_opt_list[ki](relevances, n, minmax_element);
                    update_multiple_test_outcome(outcome, test_topk_opt_outcome_list[index], &optimal_score,
                                                 num_lists_assessed_list[index]);
#ifdef DEBUG
                    check_solution(relevances, outcome.score, outcome.indices, score_fun, &optimal_score, 0.5, true, false);
#endif
                }

                // EpsPruning-OPT: EpsFiltering
                if (test_epsfiltering) {
                    for (std::size_t ei=0; ei < epsilon_list.size(); ++ei) {
                        outcome = comp_epsfiltering_list_list[ei][ki](relevances, n, minmax_element);
                        update_multiple_test_outcome(outcome, test_epsfiltering_outcome_list_list[ei][index],
                                                     &optimal_score, num_lists_assessed_list[index]);
#ifdef DEBUG
                        check_solution(relevances, outcome.score, outcome.indices, score_fun, &optimal_score, epsilon_list[ei], true, false);
#endif
                    }
                }

                ++num_lists_assessed_list[index];
                ++index;
            }
        }
    }
    if (show_progress) {
        std::cout << num_lists << " of " << num_lists << "\r";
        std::cout << std::endl;
        std::cout.flush();
    }

    ostream << "[" << std::endl;

    std::size_t index = 0;
    // loop over the different cuts of n
    for (index_type n_cut: n_cut_list) {
        // loop over the different values of k
        for (std::size_t ki=0; ki < k_list.size(); ++ki) {
            if (n_cut > 0 && k_list[ki] > n_cut) {
                continue;
            }

            ostream << "{" << std::endl;
            ostream << "\t\"n_cut\": " << n_cut;
            ostream << ", \"k\": " << k_list[ki];
            ostream << ", \"num_lists_assessed\": " << num_lists_assessed_list[index];
            ostream << ", \"strategies\": {";

            ostream << std::endl << "\t\t\"OPT\": ";
            print_json_multiple_test_outcome(test_opt_outcome_list[index], ostream);

            if (test_cutoff) {
                ostream << "," << std::endl << "\t\t\"Cutoff-OPT\": ";
                print_json_multiple_test_outcome(test_cutoff_opt_outcome_list[index], ostream);
            }

            if (test_topk) {
                ostream << "," << std::endl << "\t\t\"Topk-OPT\": ";
                print_json_multiple_test_outcome(test_topk_opt_outcome_list[index], ostream);
            }

            if (test_epsfiltering) {
                for (std::size_t ei=0; ei < epsilon_list.size(); ++ei) {
                    ostream << "," << std::endl << "\t\t\"EpsFiltering (epsilon=" << epsilon_list[ei] << ")\": ";
                    print_json_multiple_test_outcome(test_epsfiltering_outcome_list_list[ei][index], ostream);
                }
            }

            ostream << std::endl << "\t}" << std::endl;
            ostream << "}";
            if (ki < (k_list.size()-1)) {
                ostream << ",";
            }
            ostream << std::endl;

            ++index;
        }
    }

    ostream << "]" << std::endl;
}

int main(int argc, char *argv[]) {
    // command line options
    cxxopts::Options options(argv[0], "Tests the many filtering strategies and prints the performance results");
    options
            .add_options()
            ("h,help", "Print this help message")
            ("m,metric", "The search quality metric to use. Available options are: dcg, dcglz", cxxopts::value<std::string>()->default_value("dcg"))
            ("n,n_cut_list", "Truncate all lists to the first n elements, if n is greater than zero", cxxopts::value<std::string>()->default_value("0,10000"))
            ("k,k_list", "Maximum number of elements to return", cxxopts::value<std::string>()->default_value("50,100"))
            ("e,epsilon_list", "Target approximation factor", cxxopts::value<std::string>()->default_value("0.1,0.01"))
            ("skip-shorter-lists", "Skips the lists shorter than n elements", cxxopts::value<bool>()->default_value("true"))
            ("test-cutoff", "Test the cutoff-opt strategy", cxxopts::value<bool>()->default_value("true"))
            ("test-topk", "Test the topk-opt strategy", cxxopts::value<bool>()->default_value("true"))
            ("test-epsfiltering", "Test the epsilon filtering strategy", cxxopts::value<bool>()->default_value("true"))
            ("runs", "Number of times each test must be repeated", cxxopts::value<int>()->default_value("5"))
            ("cpu-affinity", "Set the cpu affinity of the process", cxxopts::value<int>()->default_value("-1"))
            ("show-progress", "Show the computation progress", cxxopts::value<bool>()->default_value("true"))
            ("o,output", "Write result to FILE instead of standard output", cxxopts::value<std::string>());
    options
            .add_options("hidden")
            ("positional", "Positional arguments: these are the arguments that are entered without an option", cxxopts::value<std::vector<std::string>>());

    options.parse_positional({"positional"});

    // command line parsing
    auto arguments = options.parse(argc, argv);

    // help
    if (arguments.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // set of available metrics
    std::unordered_set<std::string> available_metrics({"dcg", "dcglz"});

    // parameters
    std::vector<std::string> param_file_path_list;
    std::string param_metric = arguments["metric"].as<std::string>();
    std::vector<index_type>  param_n_cut_list;
    std::vector<k_type>      param_k_list;
    std::vector<score_type>  param_epsilon_list;
    bool        param_skip_shorter_lists;
    bool        param_test_epsfiltering;
    bool        param_test_topk;
    bool        param_test_cutoff;
    int         param_runs;
    int         param_show_progress;
    std::ofstream param_ofstream(nullptr);

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

        if (arguments.count("metric")) {
            if (!available_metrics.count(param_metric)) {
                throw std::runtime_error("The given metric is unavailable");
            }
        }

        // param n
        param_n_cut_list = read_parameter_list<index_type>(arguments["n_cut_list"].as<std::string>());
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
        param_k_list = read_parameter_list<k_type>(arguments["k_list"].as<std::string>());
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
        param_epsilon_list = read_parameter_list<score_type>(arguments["epsilon_list"].as<std::string>());
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

        // param tests details
        param_skip_shorter_lists = arguments["skip-shorter-lists"].as<bool>();
        param_test_epsfiltering = arguments["test-epsfiltering"].as<bool>();
        param_test_topk = arguments["test-topk"].as<bool>();
        param_test_cutoff = arguments["test-cutoff"].as<bool>();

        // param runs
        param_runs = arguments["runs"].as<int>();
        if (arguments.count("runs") && param_runs <= 0) {
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

        // show progress
        param_show_progress = arguments["show-progress"].as<bool>();

        // param output
        if (arguments.count("output")) {
            std::string output_file_path = arguments["output"].as<std::string>();
            param_ofstream = std::ofstream(output_file_path);
            if (!param_ofstream.is_open()) {
                throw std::runtime_error(std::string("Unable to open the output file ") + output_file_path);
            }
        }
    } catch (std::runtime_error & e) {
        std::cerr << e.what() << "." << std::endl;
        return -1;
    }

    // select the output stream
    std::ostream & ostream = (arguments.count("output")) ? param_ofstream : std::cout;

    // perform the test
    if (param_metric == "dcg") {
        assessment<dcg_metric>(
                param_file_path_list, ostream, param_show_progress,
                param_n_cut_list, param_k_list, param_epsilon_list,
                param_skip_shorter_lists, param_test_cutoff, param_test_topk, param_test_epsfiltering, param_runs
        );
    } else if (param_metric == "dcglz") {
        assessment<dcglz_metric>(
                param_file_path_list, ostream, param_show_progress,
                param_n_cut_list, param_k_list, param_epsilon_list,
                param_skip_shorter_lists, param_test_cutoff, param_test_topk, param_test_epsfiltering, param_runs
        );
    } else {
        std::cerr << "The given metric has not been programmed yet" << std::endl;
        return -1;
    }

    // close the file output stream
    if (arguments.count("output")) {
        param_ofstream.close();
    }

    return 0;
}
