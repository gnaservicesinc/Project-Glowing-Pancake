#include <pgp/persistence/sha256.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace pgp::persistence {
namespace {
constexpr std::array<std::uint32_t, 64> constants{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
void block(std::array<std::uint32_t, 8>& hash, const std::array<std::uint8_t, 64>& bytes) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t i=0;i<16;++i) {
        const auto base=i*4;
        words[i]=(std::uint32_t{bytes[base]}<<24U)|(std::uint32_t{bytes[base+1]}<<16U)|
                 (std::uint32_t{bytes[base+2]}<<8U)|std::uint32_t{bytes[base+3]};
    }
    for (std::size_t i=16;i<64;++i) {
        const auto a=words[i-15], b=words[i-2];
        const auto s0=std::rotr(a,7)^std::rotr(a,18)^(a>>3U);
        const auto s1=std::rotr(b,17)^std::rotr(b,19)^(b>>10U);
        words[i]=words[i-16]+s0+words[i-7]+s1;
    }
    auto a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4],f=hash[5],g=hash[6],h=hash[7];
    for (std::size_t i=0;i<64;++i) {
        const auto sum1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);
        const auto choice=(e&f)^((~e)&g);
        const auto t1=h+sum1+choice+constants[i]+words[i];
        const auto sum0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);
        const auto majority=(a&b)^(a&c)^(b&c);
        const auto t2=sum0+majority;
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;hash[4]+=e;hash[5]+=f;hash[6]+=g;hash[7]+=h;
}
}
std::string sha256(std::string_view bytes) {
    if (bytes.size()>std::numeric_limits<std::uint64_t>::max()/8U)
        throw std::length_error("SHA256 input length exceeds its bit-count field");
    std::array<std::uint32_t,8> hash{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                   0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    std::array<std::uint8_t,64> buffer{};
    std::size_t used=0;
    for (char value:bytes) {
        buffer[used++]=static_cast<std::uint8_t>(static_cast<unsigned char>(value));
        if (used==64) {block(hash,buffer);used=0;}
    }
    buffer[used++]=0x80;
    if (used>56) {
        while (used<64) buffer[used++]=0;
        block(hash,buffer);used=0;
    }
    while (used<56) buffer[used++]=0;
    const auto bits=static_cast<std::uint64_t>(bytes.size())*8U;
    for (std::size_t i=0;i<8;++i) buffer[63-i]=static_cast<std::uint8_t>(bits>>(i*8U));
    block(hash,buffer);
    constexpr char digits[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for (auto word:hash) for (int shift=28;shift>=0;shift-=4)
        result.push_back(digits[(word>>static_cast<unsigned>(shift))&15U]);
    return result;
}
}
