#include <random>
#include "random.h"

std::string Random::GetRandomString(int len)
{
    std::string code;

    const std::string chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    code.reserve(len);
    
    for (int i = 0; i < len; ++i) {
        code += chars[distribution(generator)];
    }

    return code;
}
