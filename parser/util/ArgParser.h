#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

/**
 * A simple argument parser.
 */
class ArgParser
{
private:
    std::vector<std::string> tokens_;   /**< Contains tokens in argv. */

    void check_arguments(void)
    {
        // You can implement additional validation here if needed
    }

    ArgParser() {}
    ArgParser(const ArgParser& a) = delete;
    ArgParser& operator=(const ArgParser& a) = delete;

public:
    static ArgParser& get()
    {
        static ArgParser ap;
        return ap;
    }

    void initialize(int &argc, char **argv)
    {
        for (int i = 1; i < argc; ++i) {
            tokens_.emplace_back(argv[i]);
        }
        check_arguments();
    }

    /**
     * Get single value after a flag (e.g., -out resultName)
     */
    const std::string get_argument(const std::string &argument) const
    {
        auto it = std::find(tokens_.begin(), tokens_.end(), argument);
        if (it != tokens_.end() && ++it != tokens_.end()) {
            return *it;
        }
        return "";
    }

    /**
     * Check if a flag exists
     */
    bool exists_argument(const std::string &argument) const
    {
        return std::find(tokens_.begin(), tokens_.end(), argument) != tokens_.end();
    }

    /**
     * Get all values after a flag until the next flag (multi-value support)
     * e.g. -lef file1.lef file2.lef → returns {file1.lef, file2.lef}
     */
    std::vector<std::string> get_multiple_arguments(const std::string& key) const
    {
        std::vector<std::string> result;
        auto it = std::find(tokens_.begin(), tokens_.end(), key);
        if (it == tokens_.end()) return result;

        ++it;
        while (it != tokens_.end() && it->at(0) != '-') {
            result.push_back(*it);
            ++it;
        }
        return result;
    }

    static void print_help_messages(void)
    {
        std::cerr << "Usage: ./cadb_XXXX_stage "
                  << "-weight <file> -lib <f1> <f2> ... -lef <f1> <f2> ... "
                  << "-db <...> -tf <...> -sdc <...> -v <...> -def <...> -out <name>\n";
    }
};

#endif
