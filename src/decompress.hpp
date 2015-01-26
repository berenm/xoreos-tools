/**
 * @file
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef __GR2_DECOMPRESS_HPP__
#define __GR2_DECOMPRESS_HPP__

#include <cstdint>

namespace gr2 {
  void decompress(uint32_t csize, uint8_t* cbuf, uint32_t step1, uint32_t step2, uint32_t dsize, uint8_t* dbuf);
}

#endif // ifndef __GR2_DECOMPRESS_HPP__
