#ifndef FILTERING_TYPES_HPP
#define FILTERING_TYPES_HPP

#include <cmath>


typedef std::float_t score_type;
typedef std::float_t relevance_type;
typedef std::uint32_t index_type;
typedef std::uint16_t k_type;

typedef struct {
    relevance_type min;
    relevance_type max;
} minmax_type;

#endif //FILTERING_TYPES_HPP
