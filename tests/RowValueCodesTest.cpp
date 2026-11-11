#include "../src/RowValueCodes.h"
#include <QCoreApplication>
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    constexpr std::size_t rows = 51, columns = 7754;
    std::vector<float> data(rows * columns);
    for (std::size_t r = 0; r < rows; ++r) for (std::size_t c = 0; c < columns; ++c)
        data[r * columns + c] = c % 3 ? std::log1p(float(c % 31) * (1.0f + float(r) / 17)) : 0.0f;
    const std::uint32_t negativeZero = 0x80000000, nan = 0x7fc12345, infinity = 0x7f800000;
    std::memcpy(&data[1], &negativeZero, 4); std::memcpy(&data[2], &nan, 4); std::memcpy(&data.back(), &infinity, 4);
    const auto encoded = RowValueCodes::encode(reinterpret_cast<const char*>(data.data()), rows, columns, 4);
    require(encoded.valid, "Repeated row values must be coded");
    QByteArray output(data.size() * 4, Qt::Uninitialized);
    require(RowValueCodes::decode(encoded.codes, encoded.dictionary, rows, columns, output.data()), "Row dictionary must decode");
    require(std::memcmp(output.constData(), data.data(), output.size()) == 0, "Dictionary codes must preserve exact bits, NaN payloads and negative zero");
    require(encoded.codes.size() + encoded.dictionary.size() < data.size() * 2, "Repeated float rows should shrink before compression");
    std::vector<std::uint32_t> dense(1024);
    for (std::size_t i = 0; i < dense.size(); ++i) dense[i] = static_cast<std::uint32_t>(i);
    require(!RowValueCodes::encode(reinterpret_cast<const char*>(dense.data()), 1, dense.size(), 4).valid,
        "More than 256 unique values must use the existing lossless fallback, never truncate a dictionary");
    auto bad = encoded.codes; bad[0] = static_cast<char>(255);
    require(!RowValueCodes::decode(bad, encoded.dictionary, rows, columns, output.data()), "Out-of-range codes must be rejected");
    for (const unsigned range : {1U, 2U, 3U, 4U, 16U, 256U}) {
        QByteArray codes(7755, Qt::Uninitialized);
        for (qsizetype i = 0; i < codes.size(); ++i) codes[i] = static_cast<char>((i * 17) % range);
        const auto packed = RowValueCodes::pack(codes.constData(), codes.size());
        require(RowValueCodes::unpack(packed, codes.size()) == codes, "Adaptive 1/2/4/8-bit codes must roundtrip, including escape values and final partial bytes");
        require(RowValueCodes::unpack(packed.left(1), codes.size()).isEmpty(), "Truncated packed codes must be rejected");
    }
    QByteArray invalid(10, 0); invalid[0] = 7;
    require(RowValueCodes::unpack(invalid, 8).isEmpty(), "Unknown bit widths must not decode");
    std::cout << "PASS: exact row codes, floating-point bit patterns, high-entropy fallback and invalid-code rejection\n";
}
