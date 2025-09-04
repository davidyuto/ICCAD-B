#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <sstream>

// 表示一個端口連接
struct PortConnection {
    std::string port_name;    // 端口名稱 (如 .A1)
    std::string net_name;     // 網路名稱 (如 n115715)
};

// 表示一個模塊實例
struct ModuleInstance {
    std::string module_name;   // 模塊名稱 (如 SNPSSLOPT25_OR2_MM_2)
    std::string instance_name; // 實例名稱 (如 c_n115838)
    std::vector<PortConnection> connections;
};

// 表示一個網路及其連接
struct NetConnection {
    std::string net_name;
    std::vector<std::pair<std::string, std::string>> connections; // (instance_name, port_name)
};

class VerilogParser {
private:
    std::vector<ModuleInstance> instances_;
    std::map<std::string, NetConnection> nets_;
    bool debug_mode_ = false;  // 添加調試模式

    // 移除字符串前後的空白字符
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    // 清理和標準化字符串
    std::string cleanString(const std::string& str) {
        std::string cleaned = str;
        
        // 移除多餘的空白字符
        std::regex whitespace_regex("\\s+");
        cleaned = std::regex_replace(cleaned, whitespace_regex, " ");
        
        return trim(cleaned);
    }

    // 解析單個模塊實例
    void parseInstance(const std::string& instance_str) {
        if (debug_mode_) {
            //std::cout << "[Debug] Parsing instance: " << instance_str << std::endl;
        }
        
        ModuleInstance instance;
        
        // 修正的正則表達式：支持更寬泛的命名規則
        // ([A-Za-z][A-Za-z0-9_]*) - 模塊名稱
        // ([A-Za-z0-9_]+) - 實例名稱（支持數字開頭和雙下劃線）
        std::regex instance_regex(R"(([A-Za-z][A-Za-z0-9_]*)\s+([A-Za-z0-9_]+)\s*\(\s*(.*?)\s*\)\s*;)", 
                                 std::regex_constants::ECMAScript | std::regex_constants::multiline);
        std::smatch match;
        
        if (std::regex_search(instance_str, match, instance_regex)) {
            instance.module_name = match[1].str();
            instance.instance_name = match[2].str();
            std::string connections_str = match[3].str();
            
            if (debug_mode_) {
                //std::cout << "[Debug] Found: " << instance.module_name 
                        // << " " << instance.instance_name << std::endl;
                //std::cout << "[Debug] Connections: " << connections_str << std::endl;
            }
            
            // 解析端口連接
            parseConnections(connections_str, instance.connections);
            instances_.push_back(instance);
            
            if (debug_mode_) {
                //std::cout << "[Debug] Added instance with " 
                        // << instance.connections.size() << " connections" << std::endl;
            }
        } else {
            if (debug_mode_) {
                //std::cout << "[Debug] Failed to match instance pattern" << std::endl;
            }
        }
    }

    // 解析端口連接 - 支持特殊字符和復雜網路名稱
    void parseConnections(const std::string& connections_str, 
                         std::vector<PortConnection>& connections) {
        
        if (debug_mode_) {
            //std::cout << "[Debug] Parsing connections: " << connections_str << std::endl;
        }
        
        // 修正的正則表達式：
        // \.([A-Za-z0-9_]+) - 端口名稱（支持數字）
        // \(\s*([^)]*?)\s*\) - 網路名稱（支持任何字符除了右括號）
        std::regex conn_regex(R"(\.([A-Za-z0-9_]+)\s*\(\s*([^)]*?)\s*\))");
        std::sregex_iterator iter(connections_str.begin(), connections_str.end(), conn_regex);
        std::sregex_iterator end;
        
        while (iter != end) {
            PortConnection conn;
            conn.port_name = (*iter)[1].str();
            std::string raw_net = (*iter)[2].str();
            
            // 清理網路名稱
            conn.net_name = cleanNetName(raw_net);
            
            if (debug_mode_) {
                //std::cout << "[Debug] Connection: ." << conn.port_name 
                        // << " ( " << conn.net_name << " )" << std::endl;
            }
            
            connections.push_back(conn);
            ++iter;
        }
    }
    
    // 清理網路名稱，處理特殊字符
    std::string cleanNetName(const std::string& raw_name) {
        std::string cleaned = trim(raw_name);
        
        // 移除反斜杠（Verilog 中的轉義字符）
        if (!cleaned.empty() && cleaned[0] == '\\') {
            cleaned = cleaned.substr(1);
        }
        
        // 處理特殊情況
        if (cleaned == "UNCONNECTED") {
            return "UNCONNECTED";
        }
        
        return cleaned;
    }

public:
    // 設置調試模式
    void setDebugMode(bool enable) {
        debug_mode_ = enable;
    }
    
    // 解析 Verilog 文件 - 改進的多行處理
    bool parseFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return false;
        }
        
        std::string line;
        std::string current_instance = "";
        bool in_instance = false;
        int line_number = 0;
        
        if (debug_mode_) {
            //std::cout << "[Debug] Starting to parse file: " << filename << std::endl;
        }
        
        while (std::getline(file, line)) {
            line_number++;
            line = trim(line);
            
            // 跳過空行和註釋
            if (line.empty() || line[0] == '/' || line[0] == '#') continue;
            
            if (debug_mode_) {
                //std::cout << "[Debug] Line " << line_number << ": " << line << std::endl;
            }
            
            // 檢查是否是模塊實例的開始
            if (!in_instance) {
                // 更精確的模塊實例檢測：
                // 1. 以大寫字母開頭的標識符
                // 2. 後跟空白和另一個標識符  
                // 3. 然後是開括號
                if (std::regex_search(line, std::regex(R"([A-Z][A-Za-z0-9_]*\s+[A-Za-z0-9_]+\s*\()"))) {
                    current_instance = line;
                    if (line.find(';') != std::string::npos) {
                        // 單行實例
                        if (debug_mode_) {
                            //std::cout << "[Debug] Single-line instance found" << std::endl;
                        }
                        parseInstance(current_instance);
                        current_instance = "";
                    } else {
                        // 多行實例開始
                        if (debug_mode_) {
                            //std::cout << "[Debug] Multi-line instance started" << std::endl;
                        }
                        in_instance = true;
                    }
                }
            } else if (in_instance) {
                // 繼續收集多行實例
                current_instance += " " + line;
                if (line.find(';') != std::string::npos) {
                    // 實例結束
                    if (debug_mode_) {
                        //std::cout << "[Debug] Multi-line instance completed" << std::endl;
                    }
                    parseInstance(current_instance);
                    current_instance = "";
                    in_instance = false;
                }
            }
        }
        
        file.close();
        
        if (debug_mode_) {
            //std::cout << "[Debug] Parsed " << instances_.size() << " instances" << std::endl;
        }
        
        buildNetConnections();
        return true;
    }

    // 建立網路連接映射
    void buildNetConnections() {
        nets_.clear();
        
        if (debug_mode_) {
            //std::cout << "[Debug] Building net connections..." << std::endl;
        }
        
        for (const auto& instance : instances_) {
            for (const auto& conn : instance.connections) {
                // 跳過電源和地線，但不跳過 UNCONNECTED
                if (conn.net_name == "VDD" || conn.net_name == "VSS") {
                    continue;
                }
                
                // 對於 UNCONNECTED，可以選擇跳過或創建特殊網路
                if (conn.net_name == "UNCONNECTED") {
                    if (debug_mode_) {
                        //std::cout << "[Debug] Skipping UNCONNECTED port: " 
                                // << instance.instance_name << "." << conn.port_name << std::endl;
                    }
                    continue;  // 跳過未連接的端口
                }
                
                if (nets_.find(conn.net_name) == nets_.end()) {
                    nets_[conn.net_name] = NetConnection{conn.net_name, {}};
                }
                
                nets_[conn.net_name].connections.push_back(
                    std::make_pair(instance.instance_name, conn.port_name)
                );
                
                if (debug_mode_) {
                    //std::cout << "[Debug] Added connection: " << conn.net_name 
                            // << " -> " << instance.instance_name << "." << conn.port_name << std::endl;
                }
            }
        }
        
        if (debug_mode_) {
            //std::cout << "[Debug] Built " << nets_.size() << " nets" << std::endl;
        }
    }

    // 獲取所有網路連接
    const std::map<std::string, NetConnection>& getNets() const {
        return nets_;
    }

    // 獲取所有實例
    const std::vector<ModuleInstance>& getInstances() const {
        return instances_;
    }

    // // 調試特定實例
    // void debugInstance(const std::string& instance_name) {
    //     //std::cout << "\n=== DEBUG: Searching for instance " << instance_name << " ===" << std::endl;
        
    //     bool found = false;
    //     for (const auto& inst : instances_) {
    //         if (inst.instance_name == instance_name) {
    //             std::cout << "Found instance: " << inst.module_name 
    //                     << " " << inst.instance_name << std::endl;
    //             std::cout << "Connections:" << std::endl;
                
    //             for (const auto& conn : inst.connections) {
    //                 //std::cout << "  ." << conn.port_name << " ( " << conn.net_name << " )" << std::endl;
    //             }
    //             found = true;
    //             break;
    //         }
    //     }
        
    //     if (!found) {
    //         //std::cout << "Instance " << instance_name << " NOT FOUND!" << std::endl;
    //     }
    // }
    
    // // 調試特定網路
    // void debugNet(const std::string& net_name) {
    //     //std::cout << "\n=== DEBUG: Searching for net " << net_name << " ===" << std::endl;
        
    //     auto it = nets_.find(net_name);
    //     if (it != nets_.end()) {
    //         const auto& net = it->second;
    //         //std::cout << "Found net: " << net.net_name << std::endl;
    //         //std::cout << "Connections:" << std::endl;
            
    //         for (const auto& conn : net.connections) {
    //             //std::cout << "  " << conn.first << "." << conn.second << std::endl;
    //         }
    //     } else {
    //         //std::cout << "Net " << net_name << " NOT FOUND!" << std::endl;
            
    //         // 搜索相似的網路名稱
    //         //std::cout << "Similar nets found:" << std::endl;
    //         for (const auto& net_pair : nets_) {
    //             if (net_pair.first.find(net_name) != std::string::npos ||
    //                 net_name.find(net_pair.first) != std::string::npos) {
    //                 //std::cout << "  " << net_pair.first << std::endl;
    //             }
    //         }
    //     }
    // }

    // 打印解析結果（用於調試）
    void printNets() const {
        //std::cout << "\n=== All Networks ===" << std::endl;
        for (const auto& net_pair : nets_) {
            const auto& net = net_pair.second;
            //std::cout << "Net: " << net.net_name << std::endl;
            //std::cout << std::endl;
        }
    }
    
    // // 統計信息
    // void printStatistics() const {
    //     // std::cout << "\n=== Parser Statistics ===" << std::endl;
    //     // std::cout << "Total instances: " << instances_.size() << std::endl;
    //     // std::cout << "Total nets: " << nets_.size() << std::endl;
        
    //     // 統計不同模塊類型
    //     std::map<std::string, int> module_count;
    //     for (const auto& inst : instances_) {
    //         module_count[inst.module_name]++;
    //     }
        
    //     // std::cout << "Module types:" << std::endl;
    //     // for (const auto& kv : module_count) {
    //     //     std::cout << "  " << kv.first << ": " << kv.second << std::endl;
    //     // }
    // }
};