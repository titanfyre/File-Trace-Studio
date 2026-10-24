#pragma once
#include "FileTrace.h"
#include <array>
#include <deque>
#include <string>

namespace Studio {
struct Summary { uint64_t count=0,errors=0,bytes=0;long double ticks=0;uint64_t maximum=0; };
class Model {
public:
    std::array<Summary,5> operations{};
    Summary total{};
    FT_HEADER status{};
    FT_EVENT last{};
    bool hasLast=false;
    std::deque<double> history;
    void Add(const FileTrace::Batch& batch);
    void Clear();
    double MeanMicroseconds(const Summary& value) const;
};
DWORD Export(const std::wstring& path,const Model& model,bool synthetic);
bool ParsePid(const std::wstring& value,uint32_t& pid);
}
