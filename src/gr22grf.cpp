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

    std::vector< decltype(relocation) >  relocations;
    std::vector< decltype(marshalling) > marshallings;
    std::vector< uint8_t >               data;
    std::vector< size_t >                section_offsets;

    for (size_t i = 0; i < info.sections_count; ++i) {
      input.read(reinterpret_cast< char* >(&section), sizeof(section));

      auto position = input.tellg();

      input.seekg(section.relocations_offset, input.beg);
      section.relocations_offset = relocations.size();
      relocations.resize(section.relocations_offset + section.relocations_count);
      input.read(reinterpret_cast< char* >(relocations.data() + section.relocations_offset), sizeof(relocation) * section.relocations_count);

      input.seekg(section.marshallings_offset, input.beg);
      section.marshallings_offset = marshallings.size();
      marshallings.resize(section.marshallings_offset + section.marshallings_count);
      input.read(reinterpret_cast< char* >(marshallings.data() + section.marshallings_offset), sizeof(marshalling) * section.marshallings_count);

      input.seekg(section.data_offset, input.beg);
      section.data_offset = data.size();
      data.resize(section.data_offset + section.decompressed_size);

      if (section.compression == 0) {
        input.read(reinterpret_cast< char* >(data.data() + section.data_offset), section.data_size);
      } else if (section.compression == 2) {
        auto cdata = std::vector< uint8_t >((section.data_size + 3) & ~3);
        input.read(reinterpret_cast< char* >(cdata.data()), section.data_size);
        gr2::decompress(cdata.size(), cdata.data(), section.steps[0], section.steps[1], section.decompressed_size, data.data() + section.data_offset);
      } else {
        assert(false);
      }

      input.seekg(position, input.beg);

      section_offsets.push_back(section.data_offset);
      for (size_t i = 0; i < section.relocations_count; ++i) {
        assert(relocations[section.relocations_offset + i].offset < 0xffffffff - section.data_offset);
        relocations[section.relocations_offset + i].offset += section.data_offset;
      }

      for (size_t i = 0; i < section.marshallings_count; ++i) {
        assert(marshallings[section.marshallings_offset + i].offset < 0xffffffff - section.data_offset);
        marshallings[section.marshallings_offset + i].offset += section.data_offset;
      }
    }

    info.type_offset += section_offsets[info.type_section];
    info.type_section = 0;

    info.root_offset += section_offsets[info.root_section];
    info.root_section = 0;

    for (auto& r : relocations) {
      assert(r.target_offset < 0xffffffff - section_offsets[r.target_section]);
      r.target_offset += section_offsets[r.target_section];
      r.target_section = 0;
    }
    for (auto& m : marshallings) {
      assert(m.target_offset < 0xffffffff - section_offsets[m.target_section]);
      m.target_offset += section_offsets[m.target_section];
      m.target_section = 0;
    }

    info.file_size      = sizeof(header) + sizeof(info) + sizeof(section);
    info.crc32          = 0xffffffff;
    info.sections_count = 1;

    section.compression = 0;
    section.steps[0]    = 0;
    section.steps[1]    = 0;

    section.relocations_offset = info.file_size;
    section.relocations_count  = 0;
    info.file_size            += section.relocations_count * sizeof(relocation);

    section.marshallings_offset = info.file_size;
    section.marshallings_count  = marshallings.size();
    info.file_size             += section.marshallings_count * sizeof(marshalling);

    section.data_offset       = info.file_size;
    section.data_size         = data.size();
    section.decompressed_size = section.data_size;
    info.file_size           += section.data_size;

    info.crc32 = gr2::crc32(reinterpret_cast< char* >(&section), reinterpret_cast< char* >(&section + 1), info.crc32);
    info.crc32 = gr2::crc32(reinterpret_cast< char* >(marshallings.data()), reinterpret_cast< char* >(marshallings.data() + marshallings.size()), info.crc32);
    info.crc32 = gr2::crc32(std::begin(data), std::end(data), info.crc32);

    for (auto& r : relocations) {
      *reinterpret_cast< uint32_t* >(&data[r.offset]) = r.target_offset;
    }
    relocations.clear();

    output.write(reinterpret_cast< char* >(&header), sizeof(header));
    output.write(reinterpret_cast< char* >(&info), sizeof(info));
    output.write(reinterpret_cast< char* >(&section), sizeof(section));
    output.write(reinterpret_cast< char* >(marshallings.data()), sizeof(marshalling) * marshallings.size());
    output.write(reinterpret_cast< char* >(data.data()), data.size());
  } // decompress

} // namespace gr2

int main(int argc, char const** argv) {
  assert(argc > 1);

  auto const input  = argv[1];
  auto const output = argc < 3 ? std::string(input).substr(0, std::strlen(input) - 3) + "grf" : argv[2];

  gr2::decompress(input, output);
}
