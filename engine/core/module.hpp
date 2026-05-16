#pragma once

#include <string>

namespace arx::engine {

class RuntimeContext;

class Module {
public:
    virtual ~Module() = default;

    virtual const char* name() const noexcept = 0;
    virtual bool start(RuntimeContext& context) = 0;
    virtual void tick(RuntimeContext& context, double dt_seconds) = 0;
    virtual void stop(RuntimeContext& context) = 0;
};

}  // namespace arx::engine
