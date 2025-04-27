#pragma once

// Builder 基类
template<typename T>
class Builder {
public:
    virtual ~Builder() = default;

    // 纯虚函数，子类必须实现
    virtual T Build() = 0;
};