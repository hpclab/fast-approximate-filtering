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
    const std::string param_file_path = arguments.count("positional") ? arguments["positional"].as<std::vector<std::string>>()[0] : std::string("");
    const k_type      param_k = arguments["k"].as<int>();
    const index_type  param_n_cut = arguments["n-cut"].as<int>();
    const score_type  param_epsilon = arguments["epsilon"].as<float>();
    std::ofstream * param_ofstream = nullptr;

    typedef PrunerFilterCompositionTest<ScoreFun> composition_type;
    composition_type * composition = nullptr;
    const bool use_files = arguments.count("positional");

    // check the command line parameters
    try {
        if (arguments.count("positional")) {
            if (arguments.count("positional") > 1) {
                throw std::runtime_error(std::string("This program runs on just one file at a time"));
            }

            std::ifstream infile(param_file_path);
            if (!infile.is_open()) {
                throw std::runtime_error(std::string("Unable to open the file ") + param_file_path);
            }
        }

        // check n and k
        if (param_n_cut > 0 and param_n_cut < param_k) {
            throw std::runtime_error(std::string("The parameter n-cut is smaller than the parameter k"));
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

        // TEST CONFIGURATION
        std::shared_ptr<ScoreFun> score_fun = std::make_shared<ScoreFun>(param_k);
        std::shared_ptr<FilterSpirin<ScoreFun>> filter = std::shared_ptr<FilterSpirin<ScoreFun>>(new FilterSpirin<ScoreFun>(param_k, score_fun));

        if (arguments["test-cutoff"].as<bool>()) {
            if (composition != nullptr) throw std::runtime_error(std::string("Unable to select more than one test at a time"));
            composition = new composition_type("Cutoff-OPT", std::make_shared<PrunerCutoff<ScoreFun>>(score_fun), filter, 1, 1.0);
        }

        if (arguments["test-topk"].as<bool>()) {
            if (composition != nullptr) throw std::runtime_error(std::string("Unable to select more than one test at a time"));
            composition = new composition_type("Topk-OPT", std::make_shared<PrunerTopk<ScoreFun>>(score_fun, param_k), filter, 1, 0.5);
        }

        if (arguments["test-epsfiltering"].as<bool>()) {
            if (composition != nullptr) throw std::runtime_error(std::string("Unable to select more than one test at a time"));
            std::ostringstream name; name << "EpsFiltering (epsilon=" << param_epsilon << ")";
            composition = new composition_type(name.str(), std::make_shared<PrunerEpsPruning<ScoreFun>>(score_fun, param_k, param_epsilon), filter, 1, param_epsilon);
        }

        if (composition == nullptr) {
            composition = new composition_type("OPT", nullptr, filter, 1);
        }

        // check the input file
        if (use_files) {
            struct stat s;
            if (stat(param_file_path.c_str(), &s) == 0) {
                if (s.st_mode & S_IFDIR) {
                    // it's a directory
                    throw std::runtime_error(std::string("The following file is a directory: ") + param_file_path);
                } else if (s.st_mode & S_IFREG) {
                    // it's a file
                } else {
                    // something else
                    throw std::runtime_error(std::string("Unable to recognize the file: ") + param_file_path);
                }
            } else {
                throw std::runtime_error(std::string("Unable to access the stats of the file: ") + param_file_path);
            }
        }
    } catch (std::runtime_error & e) {
        std::cerr << e.what() << "." << std::endl;
        return -1;
    }

    // read the input
    std::ifstream istream_file(nullptr);
    if (use_files) {
        istream_file = std::ifstream(param_file_path);
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
    const std::size_t n = (param_n_cut > 0) ? std::min(rel_list_len, static_cast<std::size_t>(param_n_cut)) : rel_list_len;

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

    TestOutcome outcome = composition->operator()(rel_list, n, minmax_element);


    // WRITE the output
    // select the output stream
    std::ostream & ostream = (param_ofstream != nullptr) ? *param_ofstream : std::cout;

    for (std::size_t i=0, i_end=outcome.indices.size(); i < i_end; ++i) {
        ostream << resultsList.ids[outcome.indices[i]] << std::endl;
    }

    // close the file output stream
    if (param_ofstream != nullptr) {
        param_ofstream->close();
        delete(param_ofstream);
    }

    return 0;
}


int main(int argc, char *argv[]) {
    // command line options
    cxxopts::Options options(argv[0], "Applies a filtering strategy to the input data and prints the list of ids to select");
    options
            .add_options()
            ("h, help", "Print this help message")
            ("m, metric", "The search quality metric to use. Available options are: dcg, dcglz", cxxopts::value<std::string>()->default_value("dcg"))
            ("n, n-cut", "Truncate all lists to the first n elements, if n is greater than zero", cxxopts::value<int>()->default_value("0"))
            ("k", "Maximum number of elements to return", cxxopts::value<int>()->default_value("50"))
            ("e, epsilon", "Target approximation factor", cxxopts::value<float>()->default_value("0.01"))
            ("a, cpu-affinity", "Set the cpu affinity of the process", cxxopts::value<int>()->default_value("-1"))
            ("o, output", "Write result to FILE instead of standard output", cxxopts::value<std::string>())
            ("test-cutoff", "Test the cutoff-opt strategy", cxxopts::value<bool>()->default_value("false"))
            ("test-topk", "Test the topk-opt strategy", cxxopts::value<bool>()->default_value("false"))
            ("test-epsfiltering", "Test the epsilon filtering strategy", cxxopts::value<bool>()->default_value("false"));
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
