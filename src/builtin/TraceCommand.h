#pragma once

#ifndef KSHELL_TRACECOMMAND_H
#define KSHELL_TRACECOMMAND_H

#include <string>
#include <vector>

#include "builtin/BuiltinCommand.h"

namespace kshell
{

BuiltinResult builtinTrace(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinUntrace(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinVars(ShellContext& ctx, const std::vector<std::wstring>& args);
BuiltinResult builtinClrtrace(ShellContext& ctx, const std::vector<std::wstring>& args);

} // namespace kshell

#endif // KSHELL_TRACECOMMAND_H
