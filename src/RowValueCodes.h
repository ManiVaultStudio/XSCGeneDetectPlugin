#pragma once
#include <QByteArray>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

// Exact raw-bit dictionaries, not quantization. Single-cell rows commonly
// repeat the same expression value for many genes with the same count.
struct RowValueCodes {
    QByteArray codes, dictionary;
    bool valid = false, allZero = true, hasRawRows = false;
    static RowValueCodes encode(const char* source, std::size_t rows, std::size_t columns, std::size_t elementSize) {
        RowValueCodes out;
        if (elementSize != 4 || !rows || !columns) return out;
        out.codes.resize(static_cast<qsizetype>(rows * columns));
        char* codeBytes = out.codes.data();
        std::vector<std::uint32_t> offsets(rows + 1), values;
        values.reserve(rows * 32);
        std::array<std::uint32_t, 1024> keys{}, generations{};
        std::array<std::uint8_t, 1024> ids{};
        for (std::size_t row = 0; row < rows; ++row) {
            const auto generation = static_cast<std::uint32_t>(row + 1);
            offsets[row] = static_cast<std::uint32_t>(values.size());
            values.push_back(0); // positive zero has code zero in every row
            std::size_t count = 1;
            std::array<std::uint32_t, 256> frequencies{};
            for (std::size_t column = 0; column < columns; ++column) {
                std::uint32_t raw;
                std::memcpy(&raw, source + (row * columns + column) * 4, 4);
                auto& code = codeBytes[row * columns + column];
                if (!raw) { code = 0; continue; }
                out.allZero = false;
                auto slot = static_cast<std::size_t>((raw * 2654435761U) >> 22);
                while (generations[slot] == generation && keys[slot] != raw) slot = (slot + 1) & 1023;
                if (generations[slot] != generation) {
                    if (count == 256) {
                        // One exceptional cell must not force every cell in
                        // this tile back to four-byte values.
                        const auto first = offsets[row];
                        values.resize(first + columns);
                        std::memcpy(values.data() + first, source + row * columns * 4, columns * 4);
                        offsets[row] = first | 0x80000000U;
                        std::memset(codeBytes + row * columns, 0, columns);
                        out.hasRawRows = true;
                        break;
                    }
                    generations[slot] = generation; keys[slot] = raw; ids[slot] = static_cast<std::uint8_t>(count++);
                    values.push_back(raw);
                }
                code = static_cast<char>(ids[slot]);
                ++frequencies[ids[slot]];
            }
            if ((offsets[row] & 0x80000000U) || count <= 2) continue;
            // Stable frequent codes compress across rows even when each row
            // has a different normalization factor and raw float values.
            std::array<std::uint16_t, 256> order{};
            std::array<std::uint8_t, 256> remap{};
            std::array<std::uint32_t, 256> sorted{};
            for (std::size_t i = 0; i < count; ++i) order[i] = static_cast<std::uint16_t>(i);
            const auto first = offsets[row];
            std::sort(order.begin() + 1, order.begin() + count, [&](auto a, auto b) {
                return frequencies[a] != frequencies[b] ? frequencies[a] > frequencies[b] : values[first + a] < values[first + b];
            });
            for (std::size_t i = 0; i < count; ++i) { remap[order[i]] = static_cast<std::uint8_t>(i); sorted[i] = values[first + order[i]]; }
            for (std::size_t column = 0; column < columns; ++column) {
                auto& code = codeBytes[row * columns + column]; code = static_cast<char>(remap[static_cast<unsigned char>(code)]);
            }
            std::copy_n(sorted.begin(), count, values.begin() + first);
        }
        offsets[rows] = static_cast<std::uint32_t>(values.size());
        out.dictionary.resize(static_cast<qsizetype>((offsets.size() + values.size()) * 4));
        std::memcpy(out.dictionary.data(), offsets.data(), offsets.size() * 4);
        std::memcpy(out.dictionary.data() + offsets.size() * 4, values.data(), values.size() * 4);
        out.valid = true;
        return out;
    }
    static QByteArray pack(const char* source, std::size_t count) {
        std::array<std::size_t, 256> frequencies{};
        for (std::size_t i = 0; i < count; ++i) ++frequencies[static_cast<unsigned char>(source[i])];
        std::size_t best = count + 1; unsigned bits = 8;
        std::size_t escaped = count - frequencies[0];
        std::size_t firstEscape = 1;
        for (const unsigned candidate : {1U, 2U, 4U}) {
            const auto escape = (1U << candidate) - 1;
            while (firstEscape < escape) escaped -= frequencies[firstEscape++];
            const std::size_t size = 1 + (count * candidate + 7) / 8 + escaped;
            if (size < best) { best = size; bits = candidate; }
        }
        QByteArray result(static_cast<qsizetype>(best), 0);
        result[0] = static_cast<char>(bits);
        if (bits == 8) { std::memcpy(result.data() + 1, source, count); return result; }
        const auto escape = (1U << bits) - 1;
        auto position = 1 + (count * bits + 7) / 8;
        auto* output = reinterpret_cast<unsigned char*>(result.data());
        for (std::size_t i = 0; i < count; ++i) {
            const auto value = static_cast<unsigned char>(source[i]);
            const auto code = std::min<unsigned>(value, escape);
            output[1 + i * bits / 8] |= static_cast<unsigned char>(code << ((i * bits) % 8));
            if (value >= escape) output[position++] = value;
        }
        return result;
    }
    static QByteArray unpack(const QByteArray& packed, std::size_t count) {
        if (packed.isEmpty()) return {};
        const auto bits = static_cast<unsigned char>(packed[0]);
        if (bits != 1 && bits != 2 && bits != 4 && bits != 8) return {};
        if (bits == 8) return packed.size() == count + 1 ? packed.mid(1) : QByteArray();
        auto position = 1 + (count * bits + 7) / 8;
        if (position > static_cast<std::size_t>(packed.size())) return {};
        const auto escape = (1U << bits) - 1;
        QByteArray result(static_cast<qsizetype>(count), Qt::Uninitialized);
        for (std::size_t i = 0; i < count; ++i) {
            auto code = (static_cast<unsigned char>(packed[1 + i * bits / 8]) >> ((i * bits) % 8)) & escape;
            if (code == escape) { if (position >= static_cast<std::size_t>(packed.size())) return {}; code = static_cast<unsigned char>(packed[position++]); }
            result[static_cast<qsizetype>(i)] = static_cast<char>(code);
        }
        return position == static_cast<std::size_t>(packed.size()) ? result : QByteArray();
    }
    static bool decode(const QByteArray& codes, const QByteArray& dictionary, std::size_t rows, std::size_t columns, char* output,
        bool columnMajor = false, std::size_t columnBase = 0) {
        const auto header = (rows + 1) * 4;
        if (codes.size() != rows * columns || dictionary.size() < header) return false;
        const auto values = (dictionary.size() - header) / 4;
        for (std::size_t row = 0; row < rows; ++row) {
            std::uint32_t first, end;
            std::memcpy(&first, dictionary.constData() + row * 4, 4);
            std::memcpy(&end, dictionary.constData() + (row + 1) * 4, 4);
            const bool raw = (first & 0x80000000U) != 0;
            first &= 0x7fffffffU; end &= 0x7fffffffU;
            if (first >= end || end > values) return false;
            if (raw) {
                if (columnBase + columns > end - first) return false;
                std::memcpy(output + row * columns * 4, dictionary.constData() + header + (first + columnBase) * 4, columns * 4);
                continue;
            }
            for (std::size_t column = 0; column < columns; ++column) {
                const auto code = static_cast<unsigned char>(codes[columnMajor ? column * rows + row : row * columns + column]);
                if (code >= end - first) return false;
                std::memcpy(output + (row * columns + column) * 4, dictionary.constData() + header + (first + code) * 4, 4);
            }
        }
        return true;
    }
};
