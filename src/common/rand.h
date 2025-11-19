#include <iostream>
#include <string>
#include <random>
#include <chrono>

class RandomGenerator {
private:
    std::random_device rd;
    std::mt19937 generator;
    
public:
    RandomGenerator() : generator(rd()) {}
    
    std::string generate(int length) {
        std::string characters = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::uniform_int_distribution<int> distribution(0, characters.size() - 1);
        std::string result;
        
        for (int i = 0; i < length; ++i) {
            result += characters[distribution(generator)];
        }
        
        return result;
    }
};

