#ifndef FILTERING_UTILS_HPP
#define FILTERING_UTILS_HPP

#include <cassert>
#include <sstream>
#include <sys/time.h>
#include <vector>
#include <numeric>


/**
 * Gets the distance in milliseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
 * @return The distance in milliseconds since the Epoch
 */
inline double
get_time_milliseconds() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return (double(tv.tv_sec) * 1000000 + double(tv.tv_usec)) / 1000.0;
}

/**
 * @author: Folly
 * @source: https://github.com/facebook/folly/blob/master/folly/Benchmark.h
 */
template<typename T>
void doNotOptimizeAway(const T &datum) {
    // The "r" constraint forces the compiler to make datum available
    // in a register to the asm block, which means that it must have
    // computed/loaded it.  We use this path for things that are <=
    // sizeof(long) (they have to fit), trivial (otherwise the compiler
    // doesn't want to put them in a register), and not a pointer (because
    // doNotOptimizeAway(&foo) would otherwise be a foot gun that didn't
    // necessarily compute foo).
    //
    // An earlier version of this method had a more permissive input operand
    // constraint, but that caused unnecessary variation between clang and
    // gcc benchmarks.
    asm volatile(""::"r"(datum));
}

/**
 * Computes the score of the given solution
 * @tparam ScoreFun Score function type
 * @param rel_list List containing the relevance scores, ordered according to some attribute
 * @param indices Ordered list of indices of the elements belonging to the solution
 * @param score_fun Score function used to score the solution
 * @return The score obtained by selecting the given indices within rel_list
 */
template<typename ScoreFun>
score_type
score_solution(
        const relevance_type *rel_list,
        const std::vector<index_type> &indices,
        const ScoreFun &score_fun
) {
    score_type score = 0.0;
    for (std::size_t i = 0, i_end = indices.size(); i < i_end; ++i) {
        score += score_fun(rel_list[indices[i]], i + 1);
    }
    for (std::size_t i = 1, i_end = indices.size(); i < i_end; ++i) {
        assert(indices[i - 1] < indices[i]);
    }
    return score;
}


/**
* Computes the score of the given solution
 * @tparam ScoreFun Score function type
 * @param rel_list List containing the relevance scores, ordered according to some attribute
 * @param solution_score The score of the solution
 * @param solution_indices The indices of the elements participating to the solution
 * @param score_fun Score function used to score the solution
 * @param optimal_score A reference to the optimal score if available (To check if the solution meets the given guarantees)
 * @param epsilon Maximum approximation error
 * @param epsilon_below True iff solution_score can be below optimal_score
 * @param epsilon_above True iff solution_score can be above optimal_score
 */
template<typename ScoreFun>
void
check_solution(
        const relevance_type *rel_list,
        const score_type solution_score,
        const std::vector<index_type> & solution_indices,
        const ScoreFun &score_fun,
        const score_type *optimal_score = nullptr,
        double epsilon = 0.0,
        bool epsilon_below = true,
        bool epsilon_above = true
) {
    score_type real_score = score_solution(rel_list, solution_indices, score_fun);

    if (epsilon_below && solution_score + 1.0e-12 < (1.0 - epsilon) * real_score) {
        throw std::runtime_error(
                "AssertionError: the solution score is less than (1-eps) times the real score");
    }
    if (!epsilon_below && solution_score + 1.0e-12 < real_score) {
        throw std::runtime_error(
                "AssertionError: the solution score is less than the real score");
    }
    if (epsilon_above && solution_score - 1.0e-12 > (1.0 + epsilon) * real_score) {
        throw std::runtime_error(
                "AssertionError: the solution score is greater than (1+eps) times the real score");
    }
    if (!epsilon_above && solution_score - 1.0e-12 > real_score) {
        throw std::runtime_error(
                "AssertionError: the solution score is greater than the real score");
    }

    if (optimal_score != nullptr) {
        if (epsilon_below && real_score + 1.0e-12 < (1.0 - epsilon) * (*optimal_score)) {
            throw std::runtime_error(
                    "AssertionError: the real score is less than (1-eps) times the optimal one");
        }
        if (!epsilon_below && real_score + 1.0e-12 < (*optimal_score)) {
            throw std::runtime_error(
                    "AssertionError: the real score is less than the optimal one");
        }
        if (epsilon_above && real_score - 1.0e-12 > (1.0 + epsilon) * (*optimal_score)) {
            throw std::runtime_error(
                    "AssertionError: the real score is greater than (1+eps) times the optimal one");
        }
        if (!epsilon_above && real_score - 1.0e-12 > (*optimal_score)) {
            throw std::runtime_error(
                    "AssertionError: the real score is greater than the optimal one");
        }
    }
}

// source: https://stackoverflow.com/questions/17074324/how-can-i-sort-two-vectors-in-the-same-way-with-criteria-that-uses-only-one-of
template <typename T, typename Compare>
std::vector<std::size_t> sort_permutation(
        const std::vector<T>& vec,
        Compare compare)
{
    std::vector<std::size_t> p(vec.size());
    std::iota(p.begin(), p.end(), 0);
    std::sort(p.begin(), p.end(),
              [&](std::size_t i, std::size_t j){ return compare(vec[i], vec[j]); });
    return p;
}
// source: https://stackoverflow.com/questions/17074324/how-can-i-sort-two-vectors-in-the-same-way-with-criteria-that-uses-only-one-of
template <typename T>
void apply_permutation_in_place(
        std::vector<T>& vec,
        const std::vector<std::size_t>& p)
{
    std::vector<bool> done(vec.size());
    for (std::size_t i = 0; i < vec.size(); ++i)
    {
        if (done[i])
        {
            continue;
        }
        done[i] = true;
        std::size_t prev_j = i;
        std::size_t j = p[i];
        while (i != j)
        {
            std::swap(vec[prev_j], vec[j]);
            done[j] = true;
            prev_j = j;
            j = p[j];
        }
    }
}

class ResultsList {
public:
    ResultsList(std::vector<std::string> && ids, std::vector<double> && attributes, std::vector<relevance_type > && relevances) :
            ids(ids),
            attributes(attributes),
            relevances(relevances) {
        if (ids.size() != attributes.size() or attributes.size() != relevances.size()) {
            throw std::runtime_error("The arguments ids, attributes and relevances must have the same size");
        }
    }

    std::size_t
    size() const {
        return this->relevances.size();
    }

public:
    const std::vector<std::string> ids;
    const std::vector<double> attributes;
    const std::vector<relevance_type> relevances;
};

/**
 * Reads a list of results from the given istream. The format must be: first line number n of elements, then n lines
 * in the format idelement <tab> attribute <tab> estimated_relevance <new_line>
 * @param istream The input stream to use for reading the triples
 * @return A list of relevances (ids and attributes are discarded in this test)
 */
ResultsList
read_results_list(
        std::istream &istream,
        bool is_file
) {
    double last_attribute_value = -DBL_MAX;

    std::size_t n;
    if (is_file) {
        n = static_cast<std::size_t>(-1);
    } else {
        if (!(istream >> n)) {
            throw std::runtime_error("The input stream is not properly formatted. Unable to extract the number of rows");
        }
        if (istream.peek() != '\n') {
            throw std::runtime_error(
                    "The input stream is not properly formatted. A new line is missing after the list length");
        }
        istream.ignore();
    }

    bool is_sorted = true;
    // vectors storing the input list properties
    std::vector<std::string> ids;
    std::vector<double> attributes;
    std::vector<relevance_type > relevances;
    // reserve enough space
    if (!is_file) {
        ids.reserve(n);
        attributes.reserve(n);
        relevances.reserve(n);
    }

    // read the input stream line by line
    for (std::size_t i=0; i < n; ++i) {
        std::string id;
        double attribute;
        relevance_type relevance;

        if (!(istream >> id)) {
            if (is_file && istream.eof()) {
                break;
            }
            throw std::runtime_error("The input stream is not properly formatted. Unable to extract the id value");
        }
        if (istream.peek() != '\t') {
            throw std::runtime_error("The input stream is not properly formatted. A tab character is missing after the id");
        }
        istream.ignore();

        if (!(istream >> attribute)) {
            throw std::runtime_error("The input stream is not properly formatted. Unable to extract the attribute value");
        }
        if (istream.peek() != '\t') {
            throw std::runtime_error("The input stream is not properly formatted. A tab character is missing after the attribute");
        }
        istream.ignore();

        if (!(istream >> relevance)) {
            throw std::runtime_error("The input stream is not properly formatted. Unable to extract the relevance value");
        }
        if (!(!is_file && i == n && !istream.eof())) {
            if (istream.peek() != '\n') {
                throw std::runtime_error(
                        "The input stream is not properly formatted. A new line character is missing after the relevance");
            }
            istream.ignore();
        }

        // check the attribute value order
        if (attribute < last_attribute_value) {
            is_sorted = false;
        }
        last_attribute_value = attribute;

        // save the triple
        if (relevance > 0) {
            ids.push_back(id);
            attributes.push_back(attribute);
            relevances.push_back(relevance);
        }
    }

    if (!is_sorted) {
        std::vector<std::size_t> permutation = sort_permutation(attributes, [](double a, double b){ return a < b; });
        apply_permutation_in_place(ids, permutation);
        apply_permutation_in_place(attributes, permutation);
        apply_permutation_in_place(relevances, permutation);
    }

    return ResultsList(std::move(ids), std::move(attributes), std::move(relevances));
}

template <typename T>
std::vector<T>
read_parameter_list(
        const std::string & str
) {
    std::istringstream istream(str);

    std::vector<T> result;
    bool met_comma = true;
    while (!istream.eof()) {
        while (istream.peek() == ' ') {
            istream.ignore();
        }
        T el;
        if (!(istream >> el)) {
            if (istream.eof()) {
                break;
            }
            throw std::runtime_error("Unable to read one of the values of the parameter list");
        } else if (!met_comma) {
            throw std::runtime_error("The parameter list is not in csv format");
        }
        result.push_back(el);
        while (istream.peek() == ' ') {
            istream.ignore();
        }
        met_comma = (istream.peek() == ',');
        if (met_comma) {
            istream.ignore();
        }
    }
    return result;
}
#endif //FILTERING_UTILS_HPP
