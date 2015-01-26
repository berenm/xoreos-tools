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

#include "decompress.hpp"

namespace gr2 {

  std::array< uint32_t, 256 > generate_crc32_table() noexcept {
    auto const reversed_polynomial = 0xedb88320u;

    struct byte_checksum {
      uint32_t operator()() noexcept {
        auto checksum = static_cast< uint32_t >(n++);

        for (auto i = 0; i < 8; ++i) {
          checksum = (checksum >> 1) ^ ((checksum & 0x1u) ? reversed_polynomial : 0);
        }

        return checksum;
      }

      unsigned n = 0;
    };

    auto table = std::array< uint32_t, 256 >();
    std::generate(table.begin(), table.end(), byte_checksum {});

    return table;
  }

  template< typename It >
  uint32_t crc32(It first, It last, uint32_t initial=0xffffffff) {
    static auto const table = generate_crc32_table();

    return ~std::accumulate(first, last, initial, [](uint32_t checksum, uint8_t value) { return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8); });
  }

  void decompress(std::string infile, std::string outfile) {
    std::cerr << "I: decompressing " << infile << " to " << outfile << std::endl;

    std::ifstream input(infile, std::ios::in | std::ios::binary);
    std::ofstream output(outfile, std::ios::out | std::ios::binary | std::ios::trunc);

    struct {
      uint32_t magic[4];
      uint32_t size;
      uint32_t format;
      uint32_t reserved[2];
    } header;

    input.read(reinterpret_cast< char* >(&header), sizeof(header));

    struct {
      uint32_t version;
      uint32_t file_size;
      uint32_t crc32;

      uint32_t sections_offset;
      uint32_t sections_count;

      uint32_t type_section;
      uint32_t type_offset;

      uint32_t root_section;
      uint32_t root_offset;

      uint32_t tag;
      uint32_t extra[4];
    } info;

    input.read(reinterpret_cast< char* >(&info), sizeof(info));

    struct  {
      uint32_t compression;

      uint32_t data_offset;
      uint32_t data_size;

      uint32_t decompressed_size;
      uint32_t alignment;

      uint32_t steps[2];

      uint32_t relocations_offset;
      uint32_t relocations_count;

      uint32_t marshallings_offset;
      uint32_t marshallings_count;
    } section;

    struct {
      uint32_t offset;
      uint32_t target_section;
      uint32_t target_offset;
    } relocation;

    struct {
      uint32_t count;
      uint32_t offset;
      uint32_t target_section;
      uint32_t target_offset;
    } marshalling;

    std::vector< std::tuple< decltype(section),
                             std::vector< decltype(relocation) >,
                             std::vector< decltype(marshalling) >,
                             std::vector< uint8_t > >
                 > sections;

    for (size_t i = 0; i < info.sections_count; ++i) {
      input.read(reinterpret_cast< char* >(&section), sizeof(section));

      auto position = input.tellg();

      std::vector< decltype(relocation) > relocations;
      relocations.resize(section.relocations_count);
      input.seekg(section.relocations_offset, input.beg);
      input.read(reinterpret_cast< char* >(relocations.data()), sizeof(relocation) * relocations.size());

      std::vector< decltype(marshalling) > marshallings;
      marshallings.resize(section.marshallings_count);
      input.seekg(section.marshallings_offset, input.beg);
      input.read(reinterpret_cast< char* >(marshallings.data()), sizeof(marshalling) * marshallings.size());

      std::vector< uint8_t > data;
      data.resize(section.decompressed_size);
      input.seekg(section.data_offset, input.beg);

      if (section.compression == 0) {
        input.read(reinterpret_cast< char* >(data.data()), data.size());
      } else if (section.compression == 2) {
        auto cdata = std::vector< uint8_t >((section.data_size + 3) & ~3);
        input.read(reinterpret_cast< char* >(cdata.data()), section.data_size);
        gr2::decompress(cdata.size(), cdata.data(), section.steps[0], section.steps[1], data.size(), data.data());
      } else {
        assert(false);
      }

      input.seekg(position, input.beg);

      sections.emplace_back(std::make_tuple(section, std::move(relocations), std::move(marshallings), std::move(data)));
    }

    info.file_size = sizeof(header) + sizeof(info) + sections.size() * sizeof(section);
    info.crc32     = 0xffffffff;
    for (auto& s: sections) {
      auto& section      = std::get< 0 >(s);
      auto& relocations  = std::get< 1 >(s);
      auto& marshallings = std::get< 2 >(s);
      auto& data         = std::get< 3 >(s);

      section.compression = 0;
      section.steps[0]    = 0;
      section.steps[1]    = 0;

      section.relocations_offset = info.file_size;
      section.relocations_count  = relocations.size();
      info.file_size            += section.relocations_count * sizeof(relocation);

      section.marshallings_offset = info.file_size;
      section.marshallings_count  = marshallings.size();
      info.file_size             += section.marshallings_count * sizeof(marshalling);

      section.data_offset       = info.file_size;
      section.data_size         = data.size();
      section.decompressed_size = section.data_size;
      info.file_size           += section.data_size;

      info.crc32 = gr2::crc32(reinterpret_cast< char* >(&section), reinterpret_cast< char* >(&section + 1), info.crc32);
    }

    for (auto& s: sections) {
      auto& relocations  = std::get< 1 >(s);
      auto& marshallings = std::get< 2 >(s);
      auto& data         = std::get< 3 >(s);

      info.crc32 = gr2::crc32(reinterpret_cast< char* >(relocations.data()), reinterpret_cast< char* >(relocations.data() + relocations.size()), info.crc32);
      info.crc32 = gr2::crc32(reinterpret_cast< char* >(marshallings.data()), reinterpret_cast< char* >(marshallings.data() + marshallings.size()), info.crc32);
      info.crc32 = gr2::crc32(std::begin(data), std::end(data), info.crc32);
    }

    for (auto& s: sections) {
      auto& relocations = std::get< 1 >(s);
      auto& data        = std::get< 3 >(s);

      for (auto& r : relocations) {
        *reinterpret_cast< uint32_t* >(&data[r.offset]) = std::get< 0 >(sections[r.target_section]).data_offset + r.target_offset;
      }
    }

    output.write(reinterpret_cast< char* >(&header), sizeof(header));
    output.write(reinterpret_cast< char* >(&info), sizeof(info));

    for (auto& s: sections) {
      auto& section = std::get< 0 >(s);
      output.write(reinterpret_cast< char* >(&section), sizeof(section));
    }

    for (auto& s: sections) {
      auto& relocations  = std::get< 1 >(s);
      auto& marshallings = std::get< 2 >(s);
      auto& data         = std::get< 3 >(s);

      output.write(reinterpret_cast< char* >(relocations.data()), sizeof(relocation) * relocations.size());
      output.write(reinterpret_cast< char* >(marshallings.data()), sizeof(marshalling) * marshallings.size());
      output.write(reinterpret_cast< char* >(data.data()), data.size());
    }
  } // decompress

} // namespace gr2

int main(int argc, char const** argv) {
  assert(argc > 1);

  auto const input  = argv[1];
  auto const output = argc < 3 ? std::string(input).substr(0, std::strlen(input) - 3) + "grd" : argv[2];

  gr2::decompress(input, output);
}
