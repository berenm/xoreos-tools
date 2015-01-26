#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "decompress.hpp"

namespace std {
  template< typename T > static auto begin(std::pair< T, T > const & p) {
    return p.first;
  }
  template< typename T > static auto end(std::pair< T, T > const & p) {
    return p.second;
  }
}

namespace gr2 {
  enum type_id {
    kGr2Struct     =1,
    kGr2RefStruct  =2,
    kGr2Array      =3,
    kGr2RefArray   =4,
    kGr2Custom     =5,
    kGr2CustomArray=7,
    kGr2RefString  =8,
    kGr2Transform  =9,
    kGr2Float32    =10,
    kGr2UInt8      =12,

    // kGr2Int8       =13,
    kGr2UInt16 =14,
    kGr2Int16  =15,
    kGr2UInt16b=16,

    // kGr2UInt32     =18,
    kGr2Int32=19,
  };

  static std::unordered_map< size_t, char const* > type_idname = {
    { kGr2Struct,      "struct"      },
    { kGr2RefStruct,   "refstruct"   },
    { kGr2Array,       "array"       },
    { kGr2RefArray,    "refarray"    },
    { kGr2Custom,      "custom"      },
    { kGr2CustomArray, "customarray" },
    { kGr2RefString,   "refstring"   },
    { kGr2Transform,   "transform"   },
    { kGr2Float32,     "float32"     },
    { kGr2UInt8,       "uint8"       },

    // { kGr2Int8,        "int8"        },
    { kGr2UInt16,      "uint16"      },
    { kGr2Int16,       "int16"       },
    { kGr2UInt16b,     "uint16"      },

    // { kGr2UInt32,      "uint32"       },
    { kGr2Int32,       "int32"       },
  };

  struct type_data {
    uint32_t type_id;
    uint32_t name_offset;
    uint32_t child_offset;
    uint32_t count;
    uint32_t padding[4];
  };

  struct type_info {
    type_data      type;
    uint32_t       offset;
    uint8_t const* data;
  };

  struct type_iterator : std::iterator< std::bidirectional_iterator_tag, type_info > {
    type_iterator(uint32_t offset, uint8_t const* data) : offset(offset), data(data) {}

    type_iterator() : offset(-1), data(nullptr) {}
    type_iterator(type_iterator const&)            = default;
    type_iterator& operator=(type_iterator const&) = default;
    type_iterator& operator++() { offset += sizeof(type_data); return *this; }
    type_iterator& operator--() { offset -= sizeof(type_data); return *this; }
    type_iterator& operator+(uint32_t n) { offset += n * sizeof(type_data); return *this; }

    type_info operator*() const { return { *reinterpret_cast< type_data const* >(data + offset), offset, data }; }

    bool invalid() const { return data == nullptr || offset == -1 || data[offset] == 0; }

    uint32_t       offset;
    uint8_t const* data;
  };

  bool operator==(type_iterator const& a, type_iterator const& b) {
    return a.offset == b.offset || (a.invalid() && b.invalid());
  }

  bool operator!=(type_iterator const& a, type_iterator const& b) { return !(a == b); }

  auto type_range(uint32_t offset, uint8_t const* data) {
    return std::make_pair(gr2::type_iterator(offset, data), gr2::type_iterator());
  }

  static std::string type_name(type_info const& info) {
    return info.type.name_offset == 0 ? "" : std::string(reinterpret_cast< char const* >(info.data + info.type.name_offset));
  }

  static size_t type_sizeof(type_info const& info) {
    typedef std::unordered_map< size_t, std::function< size_t(type_info const&) > > dispatcher;

    static dispatcher const dispatch = {
      { kGr2Struct,      [&](type_info const& info) {
          auto const children = gr2::type_range(info.type.child_offset, info.data);
          auto const sumsizes = [&](auto sum, auto const& info){ return sum + type_sizeof(info); };

          return std::accumulate(std::begin(children), std::end(children), 0u, sumsizes);
        }
      },
      { kGr2RefStruct,   [&](type_info const& info) { return sizeof(uint32_t);      } },
      { kGr2Array,       [&](type_info const& info) { return 2 * sizeof(uint32_t);  } },
      { kGr2RefArray,    [&](type_info const& info) { return 2 * sizeof(uint32_t);  } },
      { kGr2Custom,      [&](type_info const& info) { return 2 * sizeof(uint32_t);  } },
      { kGr2CustomArray, [&](type_info const& info) { return 3 * sizeof(uint32_t);  } },
      { kGr2RefString,   [&](type_info const& info) { return sizeof(uint32_t);      } },
      { kGr2Transform,   [&](type_info const& info) { return 17 * sizeof(uint32_t); } },
      { kGr2Float32,     [&](type_info const& info) { return sizeof(float);         } },
      { kGr2UInt8,       [&](type_info const& info) { return sizeof(uint8_t);       } },

      // { kGr2Int8,       [&](type_info const& info) { return sizeof(int8_t);       } },
      { kGr2UInt16,      [&](type_info const& info) { return sizeof(uint16_t);      } },
      { kGr2Int16,       [&](type_info const& info) { return sizeof(int16_t);       } },
      { kGr2UInt16b,     [&](type_info const& info) { return sizeof(uint16_t);      } },

      // { kGr2UInt32,      [&](type_info const& info) { return sizeof(uint32_t);       } },
      { kGr2Int32,       [&](type_info const& info) { return sizeof(int32_t);       } },
    };

    if (dispatch.find(info.type.type_id) == dispatch.end())
      std::cerr << info.type.type_id << " " << std::hex << (0xe64 + info.type.name_offset) << std::dec << std::endl;
    return std::max(info.type.count, 1u) * dispatch.at(info.type.type_id)(info);
  }

  static size_t range_sizeof(std::pair< type_iterator, type_iterator > const& types) {
    return std::accumulate(std::begin(types), std::end(types), 0u,
                           [&](auto size, auto const& info) { return size + type_sizeof(info); });
  }

  struct data_info {
    type_data      type;
    uint32_t       type_offset;
    uint32_t       offset;
    uint8_t const* data;
  };

  struct data_iterator : std::iterator< std::bidirectional_iterator_tag, data_info > {
    data_iterator(type_iterator types, uint32_t offset) : types(types), offset(offset) {}

    data_iterator() : types(), offset(-1) {}
    data_iterator(data_iterator const&)            = default;
    data_iterator& operator=(data_iterator const&) = default;
    data_iterator& operator++() { offset += type_sizeof(*types); ++types; return *this; }
    data_iterator& operator--() { --types; offset -= type_sizeof(*types); return *this; }

    data_info operator*() const { return { (*types).type, types.offset, offset, types.data }; }

    bool invalid() const { return types.invalid() || offset == -1; }

    type_iterator types;
    uint32_t      offset = -1;
  };

  bool operator==(data_iterator const& a, data_iterator const& b) {
    return (a.types == b.types && a.offset == b.offset) || (a.invalid() && b.invalid());
  }

  bool operator!=(data_iterator const& a, data_iterator const& b) { return !(a == b); }

  template< typename T >
  auto data_range(T && types, uint32_t offset) {
    return std::make_pair(gr2::data_iterator(std::begin(types), offset), gr2::data_iterator());
  }

  template< typename T > static auto reversed(T && c) {
    return std::make_pair(c.rbegin(), c.rend());
  }

  auto child_ranges(data_info const & info, uint8_t const* data) {
    auto ranges = std::vector< std::pair< data_iterator, data_iterator > >();

    if (type_idname.find(info.type.type_id) == type_idname.end())
      std::cerr << info.type.type_id << " " << std::hex << (0xe64 + info.type_offset) << std::dec << std::endl;
    assert(type_idname.find(info.type.type_id) != type_idname.end());
    if (info.type.type_id < 8)
      assert(info.type.count == 0);
    else
      assert(info.type.child_offset == 0);

    uint32_t type_offset = 0u;
    if (info.type.type_id <= 4)
      type_offset = info.type.child_offset;
    else if (info.type.type_id <= 7)
      type_offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset);

    if (type_offset == 0)
      return std::move(ranges);

    auto types = gr2::type_range(type_offset, &data[0]);
    switch (info.type.type_id) {
      case kGr2Struct:
        ranges.emplace_back(data_range(types, info.offset));
        break;

      case kGr2RefStruct: {
        auto offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
        if (offset == 0)
          break;

        ranges.emplace_back(data_range(types, offset));
        break;
      }

      case kGr2Array: {
        auto count  = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
        auto offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));

        if (offset == 0)
          break;

        auto size = range_sizeof(types);
        for (size_t j = 0; j < count; ++j) {
          ranges.emplace_back(data_range(types, offset));
          offset += size;
        }
        break;
      }

      case kGr2RefArray: {
        auto count    = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
        auto elements = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));

        if (elements == 0)
          break;

        for (size_t j = 0; j < count; ++j) {
          auto offset = *reinterpret_cast< uint32_t const* >(data + elements);
          if (offset == 0)
            continue;

          ranges.emplace_back(data_range(types, offset));
          elements += sizeof(uint32_t);
        }
        break;
      }

      case kGr2Custom: {
        auto offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));
        if (offset == 0)
          break;

        ranges.emplace_back(data_range(types, offset));
        break;
      }

      case kGr2CustomArray: {
        auto count  = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));
        auto offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset + 2 * sizeof(uint32_t));
        if (offset == 0)
          break;

        auto size = range_sizeof(types);
        for (size_t j = 0; j < count; ++j) {
          ranges.emplace_back(data_range(types, offset));
          offset += size;
        }
        break;
      }
    } // switch

    return std::move(ranges);
  }

  template< typename It >
  std::string base64(It first, It last) {
    static auto const        m1      = 63 << 18;
    static auto const        m2      = 63 << 12;
    static auto const        m3      = 63 << 6;
    static std::string const charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;

    auto it = first;
    auto l  = std::distance(it, last);

    while (l > 2) {
      uint32_t d = 0;
      d |= *it++ << 16;
      d |= *it++ << 8;
      d |= *it++;
      result.append(1, charset.at((d & m1) >> 18));
      result.append(1, charset.at((d & m2) >> 12));
      result.append(1, charset.at((d & m3) >> 6));
      result.append(1, charset.at(d & 63));
      l -= 3;
    }

    if (l == 2) {
      uint32_t d = 0;
      d |= *it++ << 16;
      d |= *it++ << 8;
      result.append(1, charset.at((d & m1) >> 18));
      result.append(1, charset.at((d & m2) >> 12));
      result.append(1, charset.at((d & m3) >> 6));
      result.append(1, '=');
    } else if (l == 1) {
      uint32_t d = 0;
      d |= *it++ << 16;
      result.append(1, charset.at((d & m1) >> 18));
      result.append(1, charset.at((d & m2) >> 12));
      result.append("==", 2);
    }

    return result;
  }

  static std::ostream& operator<<(std::ostream& output, data_info const& info) {
    switch (info.type.type_id) {
      case kGr2Float32:
        for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
          auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
          auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
          output << strbeg << *reinterpret_cast< float const* >(info.data + info.offset + i * sizeof(float)) << strend;
        }
        break;

      case kGr2UInt8:
        for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
          auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
          auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
          output << strbeg << static_cast< uint32_t >(info.data[i + info.offset]) << strend;
        }
        break;

      // case kGr2Int8:
      // for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
      // auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
      // auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
      // output << strbeg << static_cast< int32_t >(info.data[i + info.offset]) << strend;
      // }
      // break;

      case kGr2Int16:
        for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
          auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
          auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
          output << strbeg << *reinterpret_cast< int16_t const* >(info.data + info.offset + i * sizeof(int16_t)) << strend;
        }
        break;

      case kGr2UInt16:
      case kGr2UInt16b:
        for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
          auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
          auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
          output << strbeg << *reinterpret_cast< uint16_t const* >(info.data + info.offset + i * sizeof(uint16_t)) << strend;
        }
        break;

      // case kGr2UInt32:
      // for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
      // auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
      // auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
      // output << strbeg << *reinterpret_cast< uint32_t const* >(info.data + info.offset + i * sizeof(uint32_t)) << strend;
      // }
      // break;

      case kGr2Int32:
        for (size_t i = 0, count = std::max(info.type.count, 1u); i < count; ++i) {
          auto strbeg = count > 1 ? (i == 0 ? "[" : "") : "";
          auto strend = count > 1 ? (i == count - 1 ? "]" : ", ") : "";
          output << strbeg << *reinterpret_cast< int32_t const* >(info.data + info.offset + i * sizeof(int32_t)) << strend;
        }
        break;
    } // switch

    return output;
  } // <<

  void yamlize(std::string infile, std::string outfile) {
    std::cerr << "I: YAMLing " << infile << " to " << outfile << std::endl;

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

    for (auto& r : relocations) {
      *reinterpret_cast< uint32_t* >(&data[r.offset]) = r.target_offset;
    }
    relocations.clear();

    auto added        = std::unordered_set< uint32_t >();
    auto type_offsets = std::vector< uint32_t >();
    type_offsets.push_back(info.type_offset);

    for (auto m : marshallings) {
      if (added.find(m.target_offset) != added.end())
        continue;

      type_offsets.push_back(m.target_offset);
      added.insert(m.target_offset);
    }

    {
      auto range_queue = std::vector< std::pair< data_iterator, data_iterator > >();
      for (auto o : type_offsets) {
        auto types = gr2::type_range(info.type_offset, &data[0]);
        auto datas = gr2::data_range(types, info.root_offset);
        range_queue.emplace_back(datas);
      }

      while (!range_queue.empty()) {
        auto const range = range_queue.back();
        range_queue.pop_back();

        std::for_each(std::begin(range), std::end(range), [&](auto&& info) {
          if ((info.type.type_id == kGr2Custom) ||
              (info.type.type_id == kGr2CustomArray)) {
            uint32_t type_offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
            if (type_offset != 0)
              type_offsets.push_back(type_offset);
          }

          for (auto && range : gr2::child_ranges(info, &data[0])) {
            range_queue.emplace_back(range);
          }
        });
      }
    }

    output << "header:\n";
    output << "  magic: [0x" << std::hex << header.magic[0] << ", 0x" << header.magic[1] << ", 0x" << header.magic[2] << ", 0x" << header.magic[3] << "]\n" << std::dec;
    output << "  size: " << header.size << "\n";
    output << "  format: " << header.format << "\n";
    output << "  reserved: [" << header.reserved[0] << ", " << header.reserved[0] << "]\n";

    output << "info:\n";
    output << "  version: " << info.version << "\n";
    output << "  file_size: " << info.file_size << "\n";
    output << "  crc32: 0x" << std::hex << info.crc32 << std::dec << "\n";
    output << "  sections_offset: " << info.sections_offset << "\n";
    output << "  sections_count: " << info.sections_count << "\n";
    output << "  type_section: " << info.type_section << "\n";
    output << "  type_offset: " << info.type_offset << "\n";
    output << "  root_section: " << info.root_section << "\n";
    output << "  root_offset: " << info.root_offset << "\n";
    output << "  tag: 0x" << std::hex << info.tag << std::dec << "\n";
    output << "  extra: [" << info.extra[0] << ", " << info.extra[1] << ", " << info.extra[2] << ", " << info.extra[3] << "]\n";

    output << "section:\n";
    output << "  compression: " << section.compression << "\n";
    output << "  data_offset: " << section.data_offset << "\n";
    output << "  data_size: " << section.data_size << "\n";
    output << "  decompressed_size: " << section.decompressed_size << "\n";
    output << "  alignment: " << section.alignment << "\n";
    output << "  steps: [" << section.steps[0] << ", " << section.steps[1] << "]\n";
    output << "  relocations_offset: " << section.relocations_offset << "\n";
    output << "  relocations_count: " << section.relocations_count << "\n";
    output << "  marshallings_offset: " << section.marshallings_offset << "\n";
    output << "  marshallings_count: " << section.marshallings_count << "\n";

    output << "relocations:\n";
    for (auto& r: relocations) {
      output << "  - {source_section: 0, source_offset: " << r.offset << ", target_section: " << r.target_section << ", target_offset: " << r.target_offset << " }\n";
    }

    output << "marshallings:\n";
    for (auto& m: marshallings) {
      output << "  - { count: " << m.count << ", source_section: 0, source_offset: " << std::hex << m.offset << std::dec << ", target_section: " << m.target_section << ", target_offset: " << std::hex << m.target_offset << std::dec << " }\n";
    }

    output << "structure:\n";
    auto printed = std::unordered_set< uint32_t >();
    for (auto && offset : type_offsets) {
      if (printed.find(offset) != printed.end())
        continue;

      auto       offsets    = std::vector< std::tuple< uint32_t, uint32_t > >();
      auto const push_range = [&](uint32_t offset, uint32_t depth) {
                                auto children = decltype(offsets)();
                                auto types    = gr2::type_range(offset, &data[0]);
                                std::transform(std::begin(types), std::end(types), std::back_inserter(children),
                                               [&](auto const& info) { return std::make_tuple(info.offset, depth); });
                                std::copy(children.rbegin(), children.rend(), std::back_inserter(offsets));
                              };

      output << "  " << std::hex << std::setw(8) << std::setfill('0') << (0xe64 + offset) << std::dec << ":\n";
      push_range(offset, 1);

      while (!offsets.empty()) {
        auto const offset = std::get< 0 >(offsets.back());
        auto const depth  = std::get< 1 >(offsets.back());
        offsets.pop_back();

        auto const indent = std::string(depth * 2, ' ');
        auto const info   = *gr2::type_iterator(offset, &data[0]);

        if (printed.find(offset) != printed.end()) {
          output << indent << "- *" << std::hex << std::setw(8) << std::setfill('0') << (0xe64 + offset) << std::dec << "\n";
          continue;
        }

        printed.insert(offset);

        output << indent << "- &" << std::hex << std::setw(8) << std::setfill('0') << (0xe64 + offset) << std::dec << "\n";
        output << indent << "  name: " << type_name(info) << "\n";
        output << indent << "  type: " << type_idname[info.type.type_id];
        if (info.type.count > 1)
          output << "[" << info.type.count << "]\n";
        else
          output << "\n";

        if (info.type.child_offset == 0)
          continue;

        output << indent << "  children:\n";
        push_range(info.type.child_offset, depth + 1);
      }
    }

    output << "data:\n";
    {
      {
        auto       array_starts = std::unordered_set< uint32_t >();
        auto       offsets      = std::vector< std::tuple< uint32_t, uint32_t, uint32_t > >();
        auto const push_range   = [&](auto range, uint32_t depth) {
                                    auto children = decltype(offsets)();
                                    std::transform(std::begin(range), std::end(range), std::back_inserter(children),
                                                   [&](auto const& info) { return std::make_tuple(info.type_offset, info.offset, depth); });
                                    std::copy(children.rbegin(), children.rend(), std::back_inserter(offsets));
                                  };

        auto types = gr2::type_range(info.type_offset, &data[0]);
        auto range = gr2::data_range(types, info.root_offset);
        push_range(range, 1);
        while (!offsets.empty()) {
          output.flush();
          auto const type_offset = std::get< 0 >(offsets.back());
          auto const data_offset = std::get< 1 >(offsets.back());
          auto const depth       = std::get< 2 >(offsets.back());
          offsets.pop_back();

          auto const indent = std::string(depth * 2, ' ');
          auto const type   = gr2::type_iterator(type_offset, &data[0]);
          auto const info   = *gr2::data_iterator(type, data_offset);
          auto const name   = type_name(*type);

          if (array_starts.find(data_offset) != array_starts.end())
            output << indent.substr(0, indent.size() - 2) + "- ";
          else
            output << indent;

          output << name << ":";

          if (printed.find(data_offset) != printed.end()) {
            output << " *" << std::hex << std::setw(8) << std::setfill('0') << (0xe64 + data_offset) << std::dec << "\n";
            continue;
          }

          switch (info.type.type_id) {
            case kGr2RefStruct:
              output << " &" << std::hex << std::setw(8) << std::setfill('0') << (0xe64 + data_offset) << std::dec;
              printed.insert(data_offset);
              break;

            case kGr2Array: {
              if (info.type.child_offset == 0)
                break;

              auto types       = gr2::type_range(info.type.child_offset, &data[0]);
              auto child_count = std::distance(std::begin(types), std::end(types));

              if (child_count > 1)
                break;

              auto count    = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
              auto elements = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));

              if (count < 5)
                break;

              auto child = *std::begin(types);
              if ((child.type.type_id == 12) && (count > 1024)) {
                auto base64 = gr2::base64(&data[elements], &data[elements + count * type_sizeof(child)]);
                auto lines  = (base64.size() - 1) / 128;

                output << " !binary |\n";
                for (size_t i = 0; i < lines; ++i) {
                  output << indent << "  " << base64.substr(i * 128, 128) << "\\\n";
                }
                output << indent << "  " << base64.substr(lines * 128, 128) << "\n";
                continue;

              } else if (child.type.type_id >= kGr2Float32) {
                output << " [ ";

                auto lines = (count - 1) / 8;
                for (size_t i = 0; i < lines; ++i) {
                  for (size_t j = 0; j < 8; ++j) {
                    output << *data_iterator(std::begin(types), info.offset + (i * 8 + j) * type_sizeof(child)) << (j == 7 ? "," : ", ");
                  }
                  output << "\n" << indent << std::string(name.size(), ' ') << "    ";
                }
                for (size_t j = 8 * lines; j < count; ++j) {
                  output << *data_iterator(std::begin(types), info.offset + j * type_sizeof(child)) << (j == count - 1 ? "" : ", ");
                }

                output << " ]\n";
                continue;
              }
            }

            case kGr2CustomArray: {
              auto child_offset = *reinterpret_cast< uint32_t const* >(info.data + info.offset);

              auto types       = gr2::type_range(child_offset, &data[0]);
              auto child_count = std::distance(std::begin(types), std::end(types));
              auto first       = *std::begin(types);

              auto count    = *reinterpret_cast< uint32_t const* >(info.data + info.offset + sizeof(uint32_t));
              auto elements = *reinterpret_cast< uint32_t const* >(info.data + info.offset + 2 * sizeof(uint32_t));

              if (child_count == 3) {
                auto secnd = *(std::begin(types) + 1);
                auto third = *(std::begin(types) + 2);

                if ((type_name(first) != "Position") ||
                    (type_name(secnd) != "Normal") ||
                    (type_name(third) != "TextureCoordinates0"))
                  break;

                if ((first.type.count != 3) ||
                    (secnd.type.count != 3) ||
                    (third.type.count != 2))
                  break;

                if ((first.type.type_id != kGr2Float32) ||
                    (secnd.type.type_id != kGr2Float32) ||
                    (third.type.type_id != kGr2Float32))
                  break;

                auto base64 = gr2::base64(&data[elements], &data[elements + count * range_sizeof(types)]);
                auto lines  = (base64.size() - 1) / 128;

                output << " !binary {float position[3], float normal[3], float texcoord[2]} |\n";
                for (size_t i = 0; i < lines; ++i) {
                  output << indent << "  " << base64.substr(i * 128, 128) << "\\\n";
                }
                output << indent << "  " << base64.substr(lines * 128, 128) << "\n";
                continue;
              } else if (child_count == 5) {
                auto secnd = *(std::begin(types) + 1);
                auto third = *(std::begin(types) + 2);
                auto fourt = *(std::begin(types) + 3);
                auto fivet = *(std::begin(types) + 4);

                if ((type_name(first) != "Position") ||
                    (type_name(secnd) != "BoneWeights") ||
                    (type_name(third) != "BoneIndices") ||
                    (type_name(fourt) != "Normal") ||
                    (type_name(fivet) != "TextureCoordinates0"))
                  break;

                if ((first.type.count != 3) ||
                    (secnd.type.count != 4) ||
                    (third.type.count != 4) ||
                    (fourt.type.count != 3) ||
                    (fivet.type.count != 2))
                  break;

                if ((first.type.type_id != kGr2Float32) ||
                    (secnd.type.type_id != kGr2UInt16) ||
                    (third.type.type_id != kGr2UInt8) ||
                    (fourt.type.type_id != kGr2Float32) ||
                    (fivet.type.type_id != kGr2Float32))
                  break;

                auto base64 = gr2::base64(&data[elements], &data[elements + count * range_sizeof(types)]);
                auto lines  = (base64.size() - 1) / 128;

                output << " !binary {float position[3], uint16_t bonewght[4], uint8_t boneidx[4], float normal[3], float texcoord[2]} |\n";
                for (size_t i = 0; i < lines; ++i) {
                  output << indent << "  " << base64.substr(i * 128, 128) << "\\\n";
                }
                output << indent << "  " << base64.substr(lines * 128, 128) << "\n";
                continue;
              }

              break;
            }

            case kGr2RefString: {
              auto address = *reinterpret_cast< uint32_t const* >(info.data + info.offset);
              if (address == 0) {
                output << " null";
                break;
              }

              auto string = std::string(reinterpret_cast< char* >(&data[address]));
              if (string.find('\n') == std::string::npos) {
                output << " \"" << string << "\"";
                break;
              }

              output << " |\n";
              while (!string.empty()) {
                output << indent << "  " << string.substr(0, string.find('\n') - 1) << "\n";
                if (string.find('\n') == std::string::npos)
                  string = "";
                else
                  string = string.substr(string.find('\n') + 1);
              }

              break;
            }

            case kGr2Transform:
              output << " { dimensions: " << *reinterpret_cast< uint32_t const* >(info.data + info.offset) << ", origin: [";
              for (size_t i = 0; i < 3; ++i) {
                output << (i > 0 ? ", " : "") << *reinterpret_cast< float const* >(info.data + info.offset + sizeof(uint32_t) + i * sizeof(float));
              }
              output << "], rotation: [";
              for (size_t i = 0; i < 4; ++i) {
                output << (i > 0 ? ", " : "") << *reinterpret_cast< float const* >(info.data + info.offset + sizeof(uint32_t) + (3 + i) * sizeof(float));
              }
              output << "], transform: [";
              for (size_t i = 0; i < 3; ++i) {
                output << (i > 0 ? ", " : "") << "[";
                for (size_t j = 0; j < 3; ++j) {
                  output << (j > 0 ? ", " : "") << *reinterpret_cast< float const* >(info.data + info.offset + sizeof(uint32_t) + (7 + i * 3 + j) * sizeof(float));
                }
                output << "]";
              }
              output << "] }";
              break;

            case kGr2Float32:
            case kGr2UInt8:

            // case kGr2Int8:
            case kGr2Int16:
            case kGr2UInt16:
            case kGr2UInt16b:

            // case kGr2UInt32:
            case kGr2Int32:
              output << " " << info;
          } // switch

          output << "\n";

          auto children = decltype(offsets)();
          for (auto && range : gr2::child_ranges(info, &data[0])) {
            if ((info.type.type_id == kGr2Array) ||
                (info.type.type_id == kGr2RefArray) ||
                (info.type.type_id == kGr2CustomArray))
              array_starts.insert((*std::begin(range)).offset);

            std::transform(std::begin(range), std::end(range), std::back_inserter(children),
                           [&](auto const& info) { return std::make_tuple(info.type_offset, info.offset, depth + 1); });
          }
          std::copy(children.rbegin(), children.rend(), std::back_inserter(offsets));
        }
      }
    }
  } // yamlize

} // namespace gr2

int main(int argc, char const** argv) {
  assert(argc > 1);

  auto const input  = argv[1];
  auto const output = argc < 3 ? std::string(input).substr(0, std::strlen(input) - 3) + "yml" : argv[2];

  gr2::yamlize(input, output);
}
