#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "../src/decompress.hpp"

namespace gr2 {

#include "N_KoS_UnA_cutsceesdeath01.hpp"

} // namespace gr2

int main(int argc, char const** argv) {
  auto cbuf = (uint8_t*) aligned_alloc(4, sizeof(gr2::cdata) + 4);
  std::memcpy(cbuf, gr2::cdata, sizeof(gr2::cdata));

  auto dbuf = (uint8_t*) aligned_alloc(4, sizeof(gr2::ddata));
  std::memset(dbuf, 0, sizeof(gr2::ddata));

  auto ebuf = (uint8_t*) aligned_alloc(4, sizeof(gr2::ddata));
  std::memcpy(ebuf, gr2::ddata, sizeof(gr2::ddata));

  gr2::decompress(sizeof(gr2::cdata), cbuf, gr2::pdata[0], gr2::pdata[1], sizeof(gr2::ddata), dbuf);

  {
    auto file = std::fopen("ddata-01.bin", "w");
    std::fwrite(dbuf, sizeof(*dbuf), sizeof(gr2::ddata), file);
    std::fclose(file);
  }

  {
    auto file = std::fopen("edata-01.bin", "w");
    std::fwrite(gr2::ddata, sizeof(*gr2::ddata), sizeof(gr2::ddata), file);
    std::fclose(file);
  }

  assert(std::memcmp(dbuf, ebuf, sizeof(gr2::ddata)) == 0);

  std::free(cbuf);
  std::free(dbuf);
  std::free(ebuf);
}
