/* Copyright (c) 2025 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <stdint.h>
#include "pass.h"

namespace gpuav {
namespace spirv {

// We need a single pass after to inject the LogError calls
class LogErrorPass : public Pass {
  public:
    LogErrorPass(Module& module);
    const char* Name() const final { return "LogErrorPass"; }

    bool Instrument() final;
    void PrintDebugInfo() const final;

  private:
    void ClearErrorPayloadVariable(Function& function);
    void CreateFunctionCallLogError(Function& function, BasicBlock& block, InstructionIt* inst_it);
    bool IsShaderExiting(const Instruction& inst) const;

    uint32_t error_payload_variable_clear_ = 0;
    uint32_t link_function_id_ = 0;
};

}  // namespace spirv
}  // namespace gpuav